#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Shared/OperationalEventStore.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "Command/CommandScreenManager.h"
#include "Video/VideoShellWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "PathEditor/DronePathActor.h"
#include "PathEditor/DroneWaypointActor.h"

namespace {
class FP3ReplicaCheck : public IAutomationLatentCommand {
    FAutomationTestBase* Test;bool Video;bool Route=false;
public:
    FP3ReplicaCheck(FAutomationTestBase* T,bool V,bool R=false):Test(T),Video(V),Route(R){}
    bool Update() override {
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();
        if(!W){Test->AddError(TEXT("PIE world missing"));return true;}
        auto* Sync=W->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
        auto* PC=Cast<ADroneOpsPlayerController>(W->GetFirstPlayerController());
        if(Route) {
            if(!PC){Test->AddError(TEXT("Map controller required"));return true;}
            FDronePathSaveData Data;Data.PathId=2;Data.bClosedLoop=true;
            FDroneWaypointSaveData A;A.Location=FVector(0,0,100);A.WaitTime=3;Data.Waypoints.Add(A);
            FDroneWaypointSaveData B;B.Location=FVector(1000,500,100);B.SegmentSpeed=5;B.WaitTime=4;Data.Waypoints.Add(B);
            PC->LoadMissionPath(Data,true);auto Saved=PC->BuildEditingPathsData();
            Test->TestTrue(TEXT("reuse existing path editing mode"),PC->IsPathEditMode());
            Test->TestEqual(TEXT("one mission one path"),Saved.Num(),1);
            if(const auto* Path=Saved.Find(2)) {Test->TestEqual(TEXT("waypoints restored"),Path->Waypoints.Num(),2);
                if(Path->Waypoints.Num()==2){Test->TestTrue(TEXT("business coordinates retained"),Path->Waypoints[1].Location.Equals(B.Location));Test->TestEqual(TEXT("speed retained"),Path->Waypoints[1].SegmentSpeed,5.f);Test->TestEqual(TEXT("wait retained"),Path->Waypoints[1].WaitTime,4.f);}}
            bool Moved=false;
            for(TActorIterator<ADroneWaypointActor> It(W);It;++It) {
                if(!IsValid(It->PathActor) || It->PathActor->PathNumericId!=2 || It->WaypointIndex!=0)continue;
                It->MoveAlongGizmoAxis(EGizmoAxis::X,250.f);
                const auto After=PC->BuildEditingPathsData();const auto* Path=After.Find(2);
                if(Path && Path->Waypoints.Num()==2){
                    Test->TestTrue(TEXT("first mission waypoint axis move persists in existing path"),Path->Waypoints[0].Location.Equals(A.Location+FVector(250,0,0)));
                    Test->TestTrue(TEXT("axis move does not move another waypoint"),Path->Waypoints[1].Location.Equals(B.Location));
                    Test->TestEqual(TEXT("axis move retains wait time"),Path->Waypoints[0].WaitTime,A.WaitTime);
                    Moved=true;
                }
                break;
            }
            Test->TestTrue(TEXT("existing mission waypoint actor found and moved"),Moved);
            PC->SetMissionPathEditing(false);Test->TestFalse(TEXT("saved visualization does not edit"),PC->IsPathEditMode());PC->ClearEditingPaths();return true;
        }
        auto* Shell=PC && PC->GetCommandScreenManager()?PC->GetCommandScreenManager()->GetVideoShell():nullptr;
        const int32 Source=Shell?Shell->GetDisplayedDroneId():0;
        auto State=MakeShared<FJsonObject>();State->SetNumberField(TEXT("version"),9000);
        for(const TCHAR* Key:{TEXT("plans"),TEXT("missions"),TEXT("paths")})State->SetObjectField(Key,MakeShared<FJsonObject>());
        auto Event=MakeShared<FJsonObject>();Event->SetNumberField(TEXT("sequence"),9000);Event->SetNumberField(TEXT("timestamp"),1700000000);
        Event->SetStringField(TEXT("category"),TEXT("PLAN"));Event->SetStringField(TEXT("event_type"),TEXT("PLAN_DEPLOYED"));
        Event->SetStringField(TEXT("source"),TEXT("P3 QA"));Event->SetStringField(TEXT("target_id"),TEXT("qa-plan"));Event->SetStringField(TEXT("message"),TEXT("SYNTHETIC QA deployment"));
        State->SetArrayField(TEXT("events"),{MakeShared<FJsonValueObject>(Event)});
        Sync->ApplyPlans(State);Test->TestEqual(TEXT("new plan snapshot applied"),Sync->GetPlans()->GetNumberField(TEXT("version")),9000.0);
        auto* Events=W->GetGameInstance()->GetSubsystem<UOperationalEventStore>();const auto Count=Events->GetLatestSequence();
        Sync->ApplyPlans(State);Test->TestEqual(TEXT("duplicate event suppressed"),Events->GetLatestSequence(),Count);
        auto Old=MakeShared<FJsonObject>();Old->SetNumberField(TEXT("version"),8999);Sync->ApplyPlans(Old);
        Test->TestEqual(TEXT("stale plan snapshot rejected"),Sync->GetPlans()->GetNumberField(TEXT("version")),9000.0);
        if(Video){Test->TestNotNull(TEXT("Video role required"),Shell);if(Shell){Shell->Refresh();Test->TestEqual(TEXT("plan event retains local source"),Shell->GetDisplayedDroneId(),Source);Test->TestFalse(TEXT("plan event never enables follow"),Shell->IsFollowingTarget());}}
        return true;
    }
};
}
#define P3_REPLICA(Class,Name,Video) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,"DroneOps.P3." Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
bool Class::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Level/CesiumWorld")));ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));ADD_LATENT_AUTOMATION_COMMAND(FP3ReplicaCheck(this,Video));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
P3_REPLICA(FP3Selection,"CrossClient.PlanSelection",false)
P3_REPLICA(FP3Video,"Video.NoPlanSideEffects",true)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP3Route,"DroneOps.P3.Route.EditorReuse",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FP3Route::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Level/CesiumWorld")));ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));ADD_LATENT_AUTOMATION_COMMAND(FP3ReplicaCheck(this,false,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#undef P3_REPLICA
#endif
