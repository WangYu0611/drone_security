#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Command/CommandTacticalMap.h"
#include "Map/MapMoveSupport.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
using namespace PlanUI;

class FMapPointMoveChecks:public IAutomationLatentCommand {
    FAutomationTestBase* Test;double Began=FPlatformTime::Seconds();int Step=0,BeforeExecutions=0;
    TWeakObjectPtr<UCommandTacticalMap> Map;FString VideoBefore,Execution;
public:
    explicit FMapPointMoveChecks(FAutomationTestBase* In):Test(In){}
    bool Update() override {
        if(FPlatformTime::Seconds()-Began>75){if(Map.IsValid())Map->RemoveFromParent();Test->AddError(FString::Printf(TEXT("Command map move timeout at step %d"),Step));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();if(!World)return false;
        auto* PC=Cast<ADroneOpsPlayerController>(World->GetFirstPlayerController());if(!PC)return false;
        auto* Sync=World->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
        auto* Registry=World->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
        if(!Sync->IsHydrated() || Registry->GetFriendlyDroneDescriptors().Num()<2)return false;
        const auto Executions=Object(Sync->GetPlans(),TEXT("executions"));if(!Executions)return false;
        if(!Map.IsValid()){Map=CreateWidget<UCommandTacticalMap>(PC);Map->AddToViewport(200);return false;}
        auto* View=Map.Get();if(View->GetCachedGeometry().GetLocalSize().X<100)return false;View->Refresh();
        const auto Members=MapMove::Selection(Registry);
        if(Step==0){
            FJsonSerializer::Serialize(Sync->GetVideoView().ToSharedRef(),TJsonWriterFactory<>::Create(&VideoBefore));
            Sync->SetLocalSelection(1,{1});++Step;return false;
        }
        const auto Point=View->GetCachedGeometry().GetLocalSize()*.5+FVector2D(30,10);
        if(Step==1){
            if(Members!=TArray<int32>{1})return false;FVector Position;if(!MapMove::Position(Registry,Sync->GetPlans(),1,Position,true))return false;
            View->Center=UCommandTacticalMap::Project(Position.Y,Position.X,0)/256.;View->bCentered=true;
            BeforeExecutions=Executions->Values.Num();
            Test->TestTrue(TEXT("single UAV can preview a map destination"),View->PickMoveTarget(Point));
            Test->TestEqual(TEXT("preview contains exactly selected UAV"),View->MoveMembers.Num(),1);
            Test->TestEqual(TEXT("2D preview preserves acknowledged altitude"),View->MoveTarget.Z,Position.Z);
            Test->TestEqual(TEXT("preview never submits execution"),Executions->Values.Num(),BeforeExecutions);
            View->MoveAction(TEXT("cancel_move"),0);Test->TestFalse(TEXT("cancel retires preview"),View->bHasMoveTarget);
            Test->TestEqual(TEXT("cancel has no execution side effect"),Executions->Values.Num(),BeforeExecutions);
            View->PickMoveTarget(Point);Sync->SetLocalSelection(2,{2});++Step;return false;
        }
        if(Step==2){
            if(Members!=TArray<int32>{2})return false;View->Refresh();
            Test->TestFalse(TEXT("accepted selection change invalidates old preview"),View->bHasMoveTarget);
            View->MoveAction(TEXT("confirm_move"),0);Test->TestFalse(TEXT("stale confirmation cannot submit wrong UAV"),View->bMovePending);
            Sync->SetLocalSelection(1,{1});++Step;return false;
        }
        if(Step==3){
            if(Members!=TArray<int32>{1})return false;
            Test->TestTrue(TEXT("fresh destination is restaged"),View->PickMoveTarget(Point));
            View->MoveAction(TEXT("confirm_move"),0);Test->TestTrue(TEXT("explicit confirm waits for Backend ACK"),View->bMovePending);++Step;return false;
        }
        if(Step==4){
            if(View->bMovePending)return false;
            // This eastward path passes too close to unselected UAV-02. Preserve
            // the real refusal as a safety assertion before testing a clear path.
            if(!Test->TestEqual(TEXT("Backend rejects an unsafe pass near an unselected aircraft"),View->MoveErrorCode,FString(TEXT("TRAJECTORY_CONFLICT")))){
                Test->AddError(TEXT("Unexpected movement reply: ")+View->MoveMessage.ToString());View->RemoveFromParent();return true;}
            Test->TestEqual(TEXT("rejected path creates no execution"),Executions->Values.Num(),BeforeExecutions);
            Test->TestTrue(TEXT("denied target remains reviewable"),View->bHasMoveTarget);
            const auto SafePoint=View->GetCachedGeometry().GetLocalSize()*.5+FVector2D(-30,10);
            Test->TestTrue(TEXT("safe westward destination can be previewed"),View->PickMoveTarget(SafePoint));
            View->MoveAction(TEXT("confirm_move"),0);Test->TestTrue(TEXT("safe confirmation waits for ACK"),View->bMovePending);++Step;return false;
        }
        if(Step==5){
            if(View->bMovePending)return false;
            if(!View->MoveErrorCode.IsEmpty()){
                Test->AddError(TEXT("Safe movement refused: ")+View->MoveErrorCode+TEXT(" / ")+View->MoveMessage.ToString());View->RemoveFromParent();return true;}
            const auto Latest=ExecutionUI::Latest(Sync->GetPlans(),TEXT(""),true);if(!Latest || Field(Latest,TEXT("control_kind"))!=TEXT("GROUP_MOVE"))return false;
            Execution=Field(Latest,TEXT("execution_id"));Test->TestEqual(TEXT("Backend accepted the requested single UAV"),Field(Latest,TEXT("uav_id")),FString(TEXT("UAV-01")));
            Test->TestEqual(TEXT("one confirm creates exactly one execution"),Executions->Values.Num(),BeforeExecutions+1);
            Test->TestFalse(TEXT("ACK retires preview"),View->bHasMoveTarget);
            FString Video;FJsonSerializer::Serialize(Sync->GetVideoView().ToSharedRef(),TJsonWriterFactory<>::Create(&Video));
            Test->TestEqual(TEXT("selection, preview, cancel and execution never retarget Video"),Video,VideoBefore);
            auto Request=MakeShared<FJsonObject>();Request->SetStringField(TEXT("action"),TEXT("execution_abort"));Request->SetStringField(TEXT("request_id"),FGuid::NewGuid().ToString());Request->SetStringField(TEXT("execution_id"),Execution);Request->SetNumberField(TEXT("control_version"),Latest->GetNumberField(TEXT("control_version")));
            Sync->SubmitPlan(Request,[](TSharedPtr<FJsonObject>){});++Step;return false;
        }
        const auto Current=Find(Sync->GetPlans(),TEXT("executions"),Execution);if(Field(Current,TEXT("state"))!=TEXT("ABORTED"))return false;
        View->RemoveFromParent();return true;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommandSinglePointMoveTest,"DroneOps.Command.SinglePointMove",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCommandSinglePointMoveTest::RunTest(const FString&){
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(6));
    ADD_LATENT_AUTOMATION_COMMAND(FMapPointMoveChecks(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapProjectionInverseTest,"DroneOps.Command.MapProjectionInverse",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMapProjectionInverseTest::RunTest(const FString&){
    for(int Zoom:{3,15,19})for(const FVector2D Geographic:{FVector2D(116.347,39.981),FVector2D(-73.97,40.75),FVector2D(0,0)}){
        const auto RoundTrip=UCommandTacticalMap::Unproject(UCommandTacticalMap::Project(Geographic.Y,Geographic.X,Zoom),Zoom);
        TestTrue(TEXT("clicked raster pixel maps to exact longitude/latitude across zooms"),RoundTrip.Equals(Geographic,1.e-8));
    }return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapMockSourceTest,"DroneOps.Command.MapMockSource",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMapMockSourceTest::RunTest(const FString&){
    auto State=MakeShared<FJsonObject>(),All=MakeShared<FJsonObject>();State->SetObjectField(TEXT("executions"),All);
    TestTrue(TEXT("no execution permits configured Mock home"),MapMove::MockFallbackAllowed(State,1));
    auto E=MakeShared<FJsonObject>();E->SetStringField(TEXT("uav_id"),TEXT("UAV-01"));E->SetStringField(TEXT("state"),TEXT("COMPLETED"));E->SetNumberField(TEXT("updated_at"),1);All->SetObjectField(TEXT("old"),E);
    TestFalse(TEXT("legacy history without explicit simulation cannot become Mock position"),MapMove::MockFallbackAllowed(State,1));
    E->SetBoolField(TEXT("simulation"),false);TestFalse(TEXT("real execution history cannot become Mock position"),MapMove::MockFallbackAllowed(State,1));
    E->SetBoolField(TEXT("simulation"),true);TestTrue(TEXT("explicit simulated execution permits Mock position"),MapMove::MockFallbackAllowed(State,1));
    auto Scheduled=MakeShared<FJsonObject>();Scheduled->SetStringField(TEXT("uav_id"),TEXT("UAV-01"));Scheduled->SetStringField(TEXT("state"),TEXT("SCHEDULED"));Scheduled->SetNumberField(TEXT("updated_at"),9);Scheduled->SetBoolField(TEXT("simulation"),false);All->SetObjectField(TEXT("scheduled"),Scheduled);
    TestTrue(TEXT("scheduled task never overrides current acknowledged position"),MapMove::MockFallbackAllowed(State,1));
    Scheduled->SetStringField(TEXT("state"),TEXT("ABORTED"));TestFalse(TEXT("latest actual position source wins"),MapMove::MockFallbackAllowed(State,1));
    TestTrue(TEXT("other UAV history does not disable this aircraft's Mock home"),MapMove::MockFallbackAllowed(State,2));
    State->SetStringField(TEXT("execution_adapter"),TEXT("Mock"));FDroneTelemetrySnapshot Snapshot;Snapshot.bSimulation=true;Snapshot.TelemetrySource=TEXT("mock_home");
    TestFalse(TEXT("stale Registry Mock snapshot cannot bypass newer real execution source"),MapMove::TelemetrySourceAllowed(State,1,Snapshot));
    Snapshot.bSimulation=false;Snapshot.TelemetrySource=TEXT("received");TestTrue(TEXT("received hardware telemetry remains a valid source"),MapMove::TelemetrySourceAllowed(State,1,Snapshot));return true;
}
#endif
