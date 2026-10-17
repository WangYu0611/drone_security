#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Map/MapSurfaceHeight.h"
#include "Map/FleetTargeting.h"
#include "Map/MapFleetInteractionWidget.h"
#include "Map/MapShellWidget.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandMapInteractionService.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Core/CesiumCoordinateService.h"
#include "DroneOps/Core/ICoordinateService.h"
#include "Shared/ExecutionPresentation.h"
#include "CesiumGeoreference.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
using namespace PlanUI;

namespace {
AStaticMeshActor* Surface(UWorld* W,UObject* C,const FVector& Geo) {
    auto* A=W->SpawnActor<AStaticMeshActor>();auto* M=A->GetStaticMeshComponent();M->SetMobility(EComponentMobility::Movable);
    M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
    M->SetCollisionEnabled(ECollisionEnabled::QueryOnly);M->SetCollisionObjectType(ECC_WorldStatic);M->SetCollisionResponseToAllChannels(ECR_Block);
    A->SetActorScale3D(FVector(2000,2000,1));A->SetActorLocation(ICoordinateService::Execute_GeographicToWorld(C,Geo.Y,Geo.X,Geo.Z));return A;
}
// Read-only development probe. It records numerical terrain/model/camera evidence
// when explicitly requested through a test process's startup command line.
FAutoConsoleCommandWithWorld Probe(TEXT("DroneOps.SurfaceProbe"),TEXT("Read loaded surface heights into Saved/FleetTerrainHeight."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W){
        if(!W)return;FTimerHandle Handle;
        W->GetTimerManager().SetTimer(Handle,FTimerDelegate::CreateWeakLambda(W,[W]{
            auto* GI=W->GetGameInstance();if(!GI)return;auto* R=GI->GetSubsystem<UDroneRegistrySubsystem>();
            auto* S=GI->GetSubsystem<UOperationalContextSubsystem>();auto* C=R->GetCoordinateService().GetObject();if(!C || !S->IsHydrated())return;
            auto Doc=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Items;
            for(const auto& D:R->GetFriendlyDroneDescriptors()){
                const auto P=ExecutionUI::MockPosition(S->GetPlans(),FString::Printf(TEXT("UAV-%02d"),D.DroneId));if(!P)continue;
                const FVector G(P->GetNumberField(TEXT("longitude")),P->GetNumberField(TEXT("latitude")),P->GetNumberField(TEXT("altitude")));double Ground=0;
                auto Item=MakeShared<FJsonObject>();Item->SetNumberField(TEXT("uav"),D.DroneId);Item->SetNumberField(TEXT("altitude"),G.Z);
                const bool Loaded=MapSurface::Height(W,C,G,Ground);Item->SetBoolField(TEXT("surface_loaded"),Loaded);
                if(Loaded){Item->SetNumberField(TEXT("ground"),Ground);Item->SetNumberField(TEXT("agl"),G.Z-Ground);}
                if(auto* A=R->GetReceiverActor(D.DroneId)){const auto Actual=ICoordinateService::Execute_WorldToGeographic(C,A->GetActorLocation());
                    Item->SetNumberField(TEXT("model_altitude"),Actual.Z);Item->SetNumberField(TEXT("model_error_m"),FMath::Abs(Actual.Z-G.Z));}
                Items.Add(MakeShared<FJsonValueObject>(Item));
            }
            Doc->SetArrayField(TEXT("uavs"),Items);
            auto* PC=Cast<ADroneOpsPlayerController>(W->GetFirstPlayerController());auto* Manager=PC?PC->GetCommandScreenManager():nullptr;
            if(Manager && Manager->GetMapService()->GetMapCamera()){
                auto G=ICoordinateService::Execute_WorldToGeographic(C,Manager->GetMapService()->GetMapCamera()->GetActorLocation());double Ground=0;
                Doc->SetNumberField(TEXT("camera_altitude"),G.Z);if(MapSurface::Height(W,C,G,Ground))Doc->SetNumberField(TEXT("camera_agl"),G.Z-Ground);
            }
            const auto Folder=FPaths::ProjectSavedDir()/TEXT("FleetTerrainHeight");IFileManager::Get().MakeDirectory(*Folder,true);
            FString Json;FJsonSerializer::Serialize(Doc,TJsonWriterFactory<>::Create(&Json));
            FFileHelper::SaveStringToFile(Json,*(Folder/FString::Printf(TEXT("probe-%u.json"),FPlatformProcess::GetCurrentProcessId())),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        }),1.f,true);
    }));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFleetSurfaceTest,"DroneOps.Fleet.SurfaceHeight",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFleetSurfaceTest::RunTest(const FString&) {
    UWorld* W=FAutomationEditorCommonUtils::CreateNewMap();auto* Ref=W->SpawnActor<ACesiumGeoreference>();
    Ref->SetOriginLongitudeLatitudeHeight(FVector(116.34703,39.9807,0));
    auto* C=NewObject<UCesiumCoordinateService>();C->Initialize(W);auto* Plane=Surface(W,C,FVector(116.34703,39.9807,100));
    const FVector Geo(116.34703,39.9807,40);double Ground=-99;
    TestTrue(TEXT("loaded terrain supplies a real ellipsoid datum"),MapSurface::Height(W,C,Geo,Ground));
    TestTrue(TEXT("40 ellipsoid is underground beneath a 100m surface"),FMath::Abs(Ground-100)<.01 && Geo.Z<Ground);
    const FVector Desired=ICoordinateService::Execute_GeographicToWorld(C,Geo.Y,Geo.X,40);FVector Safe;
    TestTrue(TEXT("camera below ground is raised above the loaded surface"),MapSurface::ClampCamera(W,C,Desired,Safe));
    TestTrue(TEXT("camera has 5m ground clearance"),FMath::Abs(ICoordinateService::Execute_WorldToGeographic(C,Safe).Z-105)<.01);
    TestTrue(TEXT("camera calculation never changes its authoritative input"),Desired.Equals(ICoordinateService::Execute_GeographicToWorld(C,Geo.Y,Geo.X,40),.001));
    Plane->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Ground=-99;TestFalse(TEXT("missing terrain is unresolved, never ground zero"),MapSurface::Height(W,C,Geo,Ground));
    TestEqual(TEXT("failed sample preserves the caller's last datum"),Ground,-99.);return true;
}

class FFleetTerrainWorkflow : public IAutomationLatentCommand {
    FAutomationTestBase* Test;int Step=0;double Began=FPlatformTime::Seconds();int Before=0;FString Group;
public:
    explicit FFleetTerrainWorkflow(FAutomationTestBase* T):Test(T){}
    bool Update() override {
        if(FPlatformTime::Seconds()-Began>120){Test->AddError(TEXT("Terrain workflow timeout"));return true;}
        UWorld* W=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE)W=Context.World();if(!W)return false;
        auto* PC=Cast<ADroneOpsPlayerController>(W->GetFirstPlayerController());auto* Manager=PC?PC->GetCommandScreenManager():nullptr;
        auto* Shell=Manager?Manager->GetMapShell():nullptr;auto* F=Shell?Shell->GetFleetInteraction():nullptr;
        auto* S=W->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();auto* R=W->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
        if(!F || !S->IsHydrated() || F->bPending)return false;auto All=Object(S->GetPlans(),TEXT("executions"));
        auto* C=R->GetCoordinateService().GetObject();FVector Anchor;if(!F->Telemetry(1,Anchor))return false;
        if(Step==0){
            Surface(W,C,FVector(Anchor.X,Anchor.Y,100));Manager->GetMapService()->Enter3DMode();
            S->SetLocalSelection(1,{1,2,3,4});Before=All->Values.Num();++Step;return false;
        }
        if(Step==1){
            if(R->GetMultiSelectedDrones().Num()!=4 || F->bSelectionPending)return false;F->Refresh();
            Test->TestTrue(TEXT("3D defaults to height above the loaded surface"),F->bHeightAGL);
            double Safe=0;Test->TestTrue(TEXT("legacy 30m absolute positions are diagnosed below terrain"),F->RecoveryHeight(Safe));
            Test->TestTrue(TEXT("recovery preserves formation above 100m ground with 30m clearance"),FMath::Abs(Safe-130)<.01);
            F->Action(TEXT("recover"),0);Test->TestTrue(TEXT("recovery only stages an explicit vertical preview"),F->bHasTarget && F->bRecoveryPreview && !F->bPending);
            Test->TestEqual(TEXT("recovery preview does not create execution"),All->Values.Num(),Before);
            const auto Destination=F->TargetGeographic;double Height=0;Test->TestTrue(TEXT("AGL resolves"),F->TargetHeight(Height));
            Test->TestTrue(TEXT("30m AGL becomes 130m ellipsoid"),FMath::Abs(Height-130)<.01);
            F->Action(TEXT("height_mode"),0);F->Action(TEXT("height_mode"),0);
            Test->TestTrue(TEXT("reference switching preserves actual destination"),F->TargetGeographic.Equals(Destination,.0001));
            F->TargetHeightInput->SetText(User(TEXT("0")));F->TargetHeightChanged(F->TargetHeightInput->GetText());F->Action(TEXT("confirm"),0);
            Test->TestFalse(TEXT("zero AGL cannot submit"),F->bPending);
            F->TargetHeightInput->SetText(User(TEXT("30")));F->TargetHeightChanged(F->TargetHeightInput->GetText());
            Test->TestTrue(TEXT("vertical Mock recovery has a valid terrain envelope"),F->TerrainError().IsEmpty());
            F->Action(TEXT("confirm"),0);++Step;return false;
        }
        if(Step==2){
            auto Latest=ExecutionUI::Latest(S->GetPlans());if(!Latest || All->Values.Num()!=Before+4)return false;
            Group=Field(Latest,TEXT("group_id"));int Completed=0;
            for(const auto& Item:All->Values){const auto E=Item.Value->AsObject();if(Field(E,TEXT("group_id"))==Group && Field(E,TEXT("state"))==TEXT("COMPLETED"))++Completed;}
            if(Completed!=4)return false;
            for(int Id=1;Id<=4;++Id){FVector P;F->Telemetry(Id,P);double Ground=0;
                Test->TestTrue(TEXT("accepted aircraft are above actual terrain"),F->SurfaceHeight(P,Ground) && P.Z-Ground>=29.99);
                const auto Model=ICoordinateService::Execute_WorldToGeographic(C,R->GetReceiverActor(Id)->GetActorLocation());
                Test->TestTrue(TEXT("rendered model uses exact Backend altitude"),FMath::Abs(Model.Z-P.Z)<.001);}
            F->SetMode(EMapFleetMode::GroupMove);F->TargetMembers={1,2,3,4};F->TargetPrimary=1;F->TargetGeographic=Anchor;F->bHasTarget=true;
            F->TargetHeightInput->SetText(User(TEXT("40")));F->TargetHeightChanged(F->TargetHeightInput->GetText());
            Test->TestTrue(TEXT("new AGL movement remains above terrain"),FMath::Abs(F->TargetGeographic.Z-140)<.01);
            F->bHeightAGL=false;F->TargetHeightInput->SetText(User(TEXT("40")));F->TargetHeightChanged(F->TargetHeightInput->GetText());
            Test->TestFalse(TEXT("absolute target below terrain is explicitly rejected"),F->TerrainError().IsEmpty());
            const int Count=All->Values.Num();F->Action(TEXT("confirm"),0);Test->TestFalse(TEXT("underground preview never submits"),F->bPending);
            Test->TestEqual(TEXT("invalid terrain preview leaves executions unchanged"),All->Values.Num(),Count);
            Surface(W,C,FVector(Anchor.X,Anchor.Y,110));F->bHeightAGL=true;F->SetMode(EMapFleetMode::GroupMove);
            double Entered=0,Resolved=0;FleetTargeting::ParseHeight(F->TargetHeightInput->GetText().ToString(),Entered);
            Test->TestTrue(TEXT("valid current 20m AGL is preserved instead of silently raised to 30"),FMath::Abs(Entered-20)<.01);
            Test->TestTrue(TEXT("current AGL resolves to the unchanged actual height"),F->TargetHeight(Resolved) && FMath::Abs(Resolved-Anchor.Z)<.01);
            F->SetMode(EMapFleetMode::Browse);return true;
        }return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFleetTerrainWorkflowTest,"DroneOps.Fleet.TerrainWorkflow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFleetTerrainWorkflowTest::RunTest(const FString&) {
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FFleetTerrainWorkflow(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
