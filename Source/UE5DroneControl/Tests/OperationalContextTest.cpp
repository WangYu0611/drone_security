#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Command/CommandScreenManager.h"
#include "Video/VideoShellWidget.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FP1ReplicaCheck, FAutomationTestBase*, Test);
bool FP1ReplicaCheck::Update() {
    UWorld* W=nullptr;
    for (const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::PIE) W=C.World();
    if(!W || !W->GetGameInstance()) { Test->AddError(TEXT("PIE missing")); return true; }
    auto* GI=W->GetGameInstance(); auto* Sync=GI->GetSubsystem<UOperationalContextSubsystem>();
    auto* R=GI->GetSubsystem<UDroneRegistrySubsystem>();
    auto* PC=Cast<ADroneOpsPlayerController>(W->GetFirstPlayerController()); auto* M=PC?PC->GetCommandScreenManager():nullptr;
    Test->TestTrue(TEXT("P1 enabled"),Sync->IsEnabled()); Test->TestNotNull(TEXT("role manager"),M);
    if(!Sync->IsEnabled() || !M)return true;
    TArray<FDroneDescriptor> Original; for(int32 Id:{1,2,3}) { FDroneDescriptor D; if(R->GetDroneDescriptor(Id,D)) Original.Add(D); }
    for(int32 Id:{1,2,3}) { FDroneDescriptor D;D.DroneId=Id;D.Name=TEXT("P1 isolated test");R->RegisterDrone(D); }
    auto Snapshot=[](int64 Version,const FString& Id) {
        auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("context_version"),Version);J->SetStringField(TEXT("active_uav_id"),Id);
        for(const TCHAR* Field:{TEXT("active_alert_id"),TEXT("active_mission_id"),TEXT("active_security_plan_id"),TEXT("active_area_id")})J->SetField(Field,MakeShared<FJsonValueNull>());
        J->SetStringField(TEXT("operation_mode"),TEXT("MONITOR"));return J;
    };
    Test->TestTrue(TEXT("new snapshot applies"),Sync->ApplyContext(Snapshot(101,TEXT("UAV-03"))));
    Test->TestEqual(TEXT("remote selection uses existing Registry"),R->GetPrimarySelectedDrone(),3);
    Test->TestFalse(TEXT("old version rejected"),Sync->ApplyContext(Snapshot(100,TEXT("UAV-01"))));
    Test->TestFalse(TEXT("duplicate version rejected"),Sync->ApplyContext(Snapshot(101,TEXT("UAV-02"))));
    Test->TestEqual(TEXT("old message cannot overwrite primary"),R->GetPrimarySelectedDrone(),3);
    Test->TestFalse(TEXT("offline operation rejected"),Sync->SetActiveUAV(2));
    R->SetPrimarySelectedDrone(2); R->SetMultiSelectedDrones({2}); R->ClearSelection();
    Test->TestEqual(TEXT("offline local mutations do not become authoritative"),R->GetPrimarySelectedDrone(),3);
    auto Recovery=Snapshot(102,TEXT("UAV-01"));Sync->ApplyContext(Recovery);
    Test->TestEqual(TEXT("new recovery snapshot restores"),R->GetPrimarySelectedDrone(),1);
    if(M->GetClientRole()==EDroneClientRole::Video) {
        auto* Shell=M->GetVideoShell();Test->TestNotNull(TEXT("VideoShell only"),Shell);
        if(Shell){Shell->SetFollowTarget(true);Shell->Refresh();Test->TestEqual(TEXT("explicit follow applies remote Video source"),Shell->GetDisplayedDroneId(),1);}
        Test->TestNull(TEXT("no CommandShell"),M->GetShell()); Test->TestNull(TEXT("no MapShell"),M->GetMapShell());
    } else if(M->GetClientRole()==EDroneClientRole::Map) {
        Test->TestNotNull(TEXT("MapShell"),M->GetMapShell());Test->TestNull(TEXT("no CommandShell"),M->GetShell());Test->TestNull(TEXT("no VideoShell"),M->GetVideoShell());
    } else {
        Test->TestNotNull(TEXT("CommandShell"),M->GetShell());Test->TestNull(TEXT("no map service"),M->GetMapService());Test->TestNull(TEXT("no VideoShell"),M->GetVideoShell());
    }
    for(int32 Id:{1,2,3}) R->UnregisterDrone(Id);
    for(const auto& D:Original) R->RegisterDrone(D);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1ReplicaTest,"DroneOps.P1.VersionAndRoleSync",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP1ReplicaTest::RunTest(const FString&) {
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Level/CesiumWorld")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FP1ReplicaCheck(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}

class FP1MultiSelectionAckCheck : public IAutomationLatentCommand {
public:
    explicit FP1MultiSelectionAckCheck(FAutomationTestBase* InTest) : Test(InTest), Start(0) {}
    virtual bool Update() override {
        UWorld* W = nullptr;
        for (const auto& C : GEngine->GetWorldContexts()) if (C.WorldType == EWorldType::PIE) W = C.World();
        if (!W || !W->GetGameInstance()) { Test->AddError(TEXT("PIE missing")); return true; }
        auto* GI = W->GetGameInstance(); auto* Sync = GI->GetSubsystem<UOperationalContextSubsystem>();
        auto* R = GI->GetSubsystem<UDroneRegistrySubsystem>();
        if (Start == 0) Start = FPlatformTime::Seconds();
        if (FPlatformTime::Seconds() - Start > 12) { Test->AddError(TEXT("P1 MultiSelection ACK timed out; dedicated live Backend required")); return true; }
        if (!Sync->IsReady()) return false;
        if (Stage == 0) {
            for (int32 Id : {1,2}) { FDroneDescriptor Previous; if(R->GetDroneDescriptor(Id,Previous)) Original.Add(Previous); FDroneDescriptor D; D.DroneId=Id; D.Name=TEXT("P1 MultiSelection QA"); R->RegisterDrone(D); }
            R->SetPrimarySelectedDrone(1); Stage=1; return false;
        }
        if (Stage == 1 && R->GetPrimarySelectedDrone() == 1) {
            R->SetMultiSelectedDrones({1,2});
            Test->TestEqual(TEXT("local multi-selection retained"),R->GetMultiSelectedDrones().Num(),2);
            R->RemoveFromMultiSelection(1); Stage=2; return false;
        }
        if (Stage == 2 && R->GetPrimarySelectedDrone() == 2 && R->GetMultiSelectedDrones() == TArray<int32>{2}) {
            Test->TestEqual(TEXT("removed primary absent after Backend ACK"),R->GetMultiSelectedDrones().Num(),1);
            R->ClearSelection(); Stage=3; return false;
        }
        if (Stage == 3 && R->GetPrimarySelectedDrone() == 0 && R->GetMultiSelectedDrones().IsEmpty()) {
            for (int32 Id : {1,2}) R->UnregisterDrone(Id);
            for (const auto& D : Original) R->RegisterDrone(D);
            Test->AddInfo(TEXT("live ACK preserves multi-selection removal and authoritative clear without echo")); return true;
        }
        return false;
    }
private:
    FAutomationTestBase* Test;
    double Start;
    int32 Stage=0;
    TArray<FDroneDescriptor> Original;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP1MultiSelectionAckTest,"DroneOps.P1.MultiSelectionAcks",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP1MultiSelectionAckTest::RunTest(const FString&) {
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Level/CesiumWorld")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FP1MultiSelectionAckCheck(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
