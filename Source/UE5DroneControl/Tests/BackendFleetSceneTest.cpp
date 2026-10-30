#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Command/CommandScreenManager.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Network/DroneNetworkManager.h"
#include "RealTimeDroneReceiver.h"
#include "MultiDroneCharacter.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
class FCheckBackendFleetScene : public IAutomationLatentCommand
{
    TFunction<void()> Check;
public:
    explicit FCheckBackendFleetScene(TFunction<void()> In):Check(MoveTemp(In)){}
    bool Update() override {Check();return true;}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBackendFleetSceneTest,"DroneOps.Map.BackendFleetScene",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBackendFleetSceneTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2));
    ADD_LATENT_AUTOMATION_COMMAND(FCheckBackendFleetScene([this]
    {
        UWorld* World=nullptr;
        for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE)World=Context.World();
        if(!TestNotNull(TEXT("PIE world"),World))return;
        auto* GI=World->GetGameInstance();
        auto* Registry=GI->GetSubsystem<UDroneRegistrySubsystem>();
        auto* Network=GI->GetSubsystem<UDroneNetworkManager>();
        if(!TestNotNull(TEXT("Registry"),Registry) || !TestNotNull(TEXT("Network"),Network))return;
        const bool WasIsolated=Network->IsStrictLocalPreviewIsolation();
        const bool WasStrictPreview=Network->bStrictLocalPreview;
        Network->SetStrictLocalPreviewIsolation(true); // Stop actual poll/WS before direct injection.
        const FString SavedCommandLine(FCommandLine::Get());
        FString BaseCommandLine=SavedCommandLine;
        for(const TCHAR* Role:{TEXT("Map"),TEXT("Command"),TEXT("Video"),TEXT("Standalone")})
            BaseCommandLine.ReplaceInline(*(FString(TEXT("-ClientRole="))+Role),TEXT(""),ESearchCase::IgnoreCase);
        auto Role=[&](const TCHAR* Value){FCommandLine::Set(*(BaseCommandLine+TEXT(" -ClientRole=")+Value));};
        TArray<TWeakObjectPtr<AActor>> Fixtures;
        ON_SCOPE_EXIT
        {
            for(const auto& Weak:Fixtures)if(AActor* Actor=Weak.Get();Actor && !Actor->IsActorBeingDestroyed())Actor->Destroy();
            for(int32 Id:{8101,8102,8103,8104,DroneOpsConst::TargetIdToDroneId(501)})Registry->UnregisterDrone(Id);
            FCommandLine::Set(*SavedCommandLine);
            Network->bStrictLocalPreview=WasStrictPreview;
            Network->bStrictLocalPreviewIsolation=true;
            Network->SetStrictLocalPreviewIsolation(WasIsolated);
        };
        Role(TEXT("Map"));
        TestEqual(TEXT("Test exercises the Backend-authoritative Map role"),UCommandScreenManager::ResolveClientRole(),EDroneClientRole::Map);

        auto Receiver=[&](int32 Id,const TCHAR* Name)
        {
            const FTransform Transform(FVector(0,0,500));
            auto* Actor=World->SpawnActorDeferred<ARealTimeDroneReceiver>(ARealTimeDroneReceiver::StaticClass(),Transform,
                nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if(Actor)
            {
                Actor->DroneId=Id;Actor->DroneName=Name;Actor->bUseWebSocket=true;
                Actor->FinishSpawning(Transform);Fixtures.Add(Actor);
            }
            return Actor;
        };
        auto Sender=[&](int32 Id,bool Enemy)
        {
            const FTransform Transform(FVector(0,0,500));
            auto* Actor=World->SpawnActorDeferred<AMultiDroneCharacter>(AMultiDroneCharacter::StaticClass(),Transform,
                nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if(Actor)
            {
                Actor->DroneId=Id;Actor->DroneName=Enemy?TEXT("Local enemy fixture"):TEXT("Retired friendly sender");
                Actor->bIsEnemyTarget=Enemy;Actor->FinishSpawning(Transform);Fixtures.Add(Actor);
            }
            return Actor;
        };
        auto* Keep=Receiver(8101,TEXT("Authoritative friendly"));
        auto* Retire=Receiver(8102,TEXT("Stale native name label"));
        auto* Duplicate=Receiver(8102,TEXT("Unmapped duplicate label"));
        auto* Shadow=Sender(8102,false);
        const int32 EnemyId=DroneOpsConst::TargetIdToDroneId(501);
        auto* Enemy=Sender(EnemyId,true);
        if(!TestNotNull(TEXT("Keep receiver"),Keep) || !TestNotNull(TEXT("Retired receiver"),Retire)
            || !TestNotNull(TEXT("Duplicate receiver"),Duplicate) || !TestNotNull(TEXT("Sender"),Shadow)
            || !TestNotNull(TEXT("Enemy"),Enemy))return;
        auto Alive=[](const AActor* Actor){return IsValid(Actor) && !Actor->IsActorBeingDestroyed();};
        auto Response=[&](bool Success,const FString& Body)
        {
            Network->OnDroneListResponse(Success,Body,TMap<int32,uint64>{},nullptr);
        };
        auto Rows=[&](int32 Except)
        {
            TArray<TSharedPtr<FJsonValue>> Result;
            for(const auto& Desc:Registry->GetAllDroneDescriptors())
            {
                if(Desc.bIsEnemyTarget || Desc.DroneId==Except)continue;
                auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("id"),Desc.DroneId);
                Row->SetStringField(TEXT("name"),Desc.Name);Row->SetNumberField(TEXT("slot"),Desc.Slot);
                Result.Add(MakeShared<FJsonValueObject>(Row));
            }
            return Result;
        };
        auto Json=[](const TArray<TSharedPtr<FJsonValue>>& Rows)
        {
            FString Text;FJsonSerializer::Serialize(Rows,TJsonWriterFactory<>::Create(&Text));return Text;
        };
        const FString RemovedSnapshot=Json(Rows(8102));
        Network->bStrictLocalPreview=false;
        Network->bStrictLocalPreviewIsolation=false; // Direct input only; I/O remains stopped.

        Response(false,TEXT("[]"));
        TestTrue(TEXT("HTTP failure preserves native actor and its descriptor"),Alive(Retire) && Registry->IsDroneRegistered(8102));
        AddExpectedMessagePlain(TEXT("[DroneNetworkManager] Failed to parse drone list JSON"),ELogVerbosity::Warning,EAutomationExpectedMessageFlags::Contains,1);
        Response(true,TEXT("not JSON"));
        AddExpectedMessagePlain(TEXT("[DroneNetworkManager] Drone list JSON is not an array"),ELogVerbosity::Warning,EAutomationExpectedMessageFlags::Contains,1);
        Response(true,TEXT("{}"));
        AddExpectedMessagePlain(TEXT("[DroneNetworkManager] Invalid drone list entry; snapshot ignored"),ELogVerbosity::Warning,EAutomationExpectedMessageFlags::Contains,1);
        Response(true,TEXT("[{\"id\":8102,\"name\":\"must not partially update\"},null]"));
        AddExpectedMessagePlain(TEXT("[DroneNetworkManager] Invalid drone list identity; snapshot ignored"),ELogVerbosity::Warning,EAutomationExpectedMessageFlags::Contains,5);
        for(const TCHAR* Bad:{TEXT("[{}]"),TEXT("[{\"id\":0}]"),TEXT("[{\"id\":8102.5}]"),
            TEXT("[{\"id\":8102,\"name\":\"must not partially update\"},{\"id\":8102}]"),TEXT("[{\"id\":10000}]")})Response(true,Bad);
        FDroneDescriptor AfterBad;
        TestTrue(TEXT("Invalid complete lists preserve all native fixtures"),Alive(Keep) && Alive(Retire) && Alive(Duplicate) && Alive(Shadow) && Alive(Enemy));
        TestTrue(TEXT("A valid prefix of a corrupt snapshot cannot rename the descriptor"),
            Registry->GetDroneDescriptor(8102,AfterBad) && AfterBad.Name!=TEXT("must not partially update"));

        // Isolation may have local-only previews with no corresponding Backend
        // entry. An injected response must not reclaim those scene actors.
        Network->bStrictLocalPreviewIsolation=true;
        Response(true,RemovedSnapshot);
        TestTrue(TEXT("Strict isolation preserves locally previewed scene actors"),Alive(Retire) && Alive(Duplicate) && Alive(Shadow));
        TestFalse(TEXT("The fixture now reproduces lost registry mappings"),Registry->IsDroneRegistered(8102));
        Network->bStrictLocalPreviewIsolation=false;
        Network->bStrictLocalPreview=true;
        Response(true,RemovedSnapshot);
        TestTrue(TEXT("Legacy local-preview policy preserves scene actors"),Alive(Retire) && Alive(Shadow));
        Network->bStrictLocalPreview=false;

        Response(true,RemovedSnapshot);
        TestTrue(TEXT("Confirmed removal retires the native label actor"),!Alive(Retire));
        TestTrue(TEXT("Confirmed removal also retires an unmapped duplicate"),!Alive(Duplicate));
        TestTrue(TEXT("Confirmed removal retires the sender model"),!Alive(Shadow));
        TestTrue(TEXT("The listed friendly and local enemy remain alive"),Alive(Keep) && Alive(Enemy));
        Response(true,TEXT("[]"));
        TestTrue(TEXT("An acknowledged empty fleet clears the remaining friendly scene"),!Alive(Keep));
        TestFalse(TEXT("An empty fleet has no retained friendly descriptor"),Registry->IsDroneRegistered(8101));
        TestTrue(TEXT("Local enemy survives an authoritative empty friendly fleet"),Alive(Enemy) && Registry->IsDroneRegistered(EnemyId));

        Role(TEXT("Standalone"));
        auto* Legacy=Receiver(8103,TEXT("Standalone local receiver"));
        Response(true,TEXT("[]"));
        TestTrue(TEXT("Standalone retains its independent local receiver"),Alive(Legacy));
        Role(TEXT("Map"));
        Response(true,TEXT("[]"));
        TestTrue(TEXT("Returning to Backend Map cannot resurrect an unregistered placeholder"),!Alive(Legacy));
        AddInfo(TEXT("Synthetic actor lifecycle checks; no native click, physical UAV or flight execution."));
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
