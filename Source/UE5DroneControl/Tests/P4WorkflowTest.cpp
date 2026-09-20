#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Shared/OperationalEventStore.h"
#include "Shared/UILanguageSubsystem.h"
#include "Shared/ProductText.h"
#include "Shared/SecurityPlanWorkspaceWidget.h"
#include "Map/MapMissionRouteWidget.h"
#include "Command/CommandScreenManager.h"
#include "Video/VideoShellWidget.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Internationalization/Internationalization.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/PlanWidgetSupport.h"
#include "UObject/UObjectIterator.h"
#include "PathEditor/DronePathActor.h"
#include "Shared/ExecutionPresentation.h"
#include "Map/MapExecutionWidget.h"
namespace {
UWorld* World(){for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)return C.World();return nullptr;}
TSharedRef<FJsonObject> Context(int V,const FString& UAV){auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("context_version"),V);for(const TCHAR* K:{TEXT("active_alert_id"),TEXT("active_mission_id"),TEXT("active_security_plan_id"),TEXT("active_area_id")})O->SetField(K,MakeShared<FJsonValueNull>());O->SetStringField(TEXT("active_uav_id"),UAV);O->SetStringField(TEXT("operation_mode"),TEXT("MONITOR"));return O;}
TSharedRef<FJsonObject> Video(int V,const FString& Id){auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("video_view_version"),V);O->SetStringField(TEXT("video_target_uav_id"),Id);return O;}
TSharedRef<FJsonObject> Language(int V,const FString& L){auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("ui_preferences_version"),V);O->SetStringField(TEXT("language"),L);return O;}
class FP4Replica:public IAutomationLatentCommand {
    FAutomationTestBase* Test;bool IsVideo;
public:FP4Replica(FAutomationTestBase* T,bool V):Test(T),IsVideo(V){}
    bool Update() override {
        auto* W=World();if(!W){Test->AddError(TEXT("PIE world missing"));return true;}auto* GI=W->GetGameInstance();auto* S=GI->GetSubsystem<UOperationalContextSubsystem>();auto* Registry=GI->GetSubsystem<UDroneRegistrySubsystem>();
        for(int Id:{1,2}){FDroneDescriptor D;D.DroneId=Id;D.Name=TEXT("QA original name");Registry->RegisterDrone(D);}
        Test->TestTrue(TEXT("accept independent video snapshot"),S->ApplyVideoView(Video(90000,TEXT("UAV-01"))));
        S->ApplyContext(Context(90000,TEXT("UAV-02")));
        auto* PC=Cast<ADroneOpsPlayerController>(W->GetFirstPlayerController());auto* Shell=PC && PC->GetCommandScreenManager()?PC->GetCommandScreenManager()->GetVideoShell():nullptr;
        if(IsVideo){Test->TestNotNull(TEXT("real Video shell"),Shell);if(!Shell)return true;Shell->Refresh();Test->TestEqual(TEXT("selection did not switch video"),Shell->GetDisplayedDroneId(),1);}
        const int Generation=Shell?Shell->GetBrowserGeneration():0;const auto* Browser=Shell?Shell->GetBrowser():nullptr;
        auto ContextBefore=S->GetContext();const auto PlansBefore=S->GetPlans();
        Test->TestTrue(TEXT("no target catalog contains explicit instruction"),ProductText::Get(TEXT("Video.NoTargetHint")).ToString().Contains(TEXT("video")) || ProductText::Get(TEXT("Video.NoTargetHint")).ToString().Contains(TEXT("视频")));
        const FText Stable=ProductText::Get(TEXT("Plan.Title"));S->ApplyUIPreferences(Language(90000,TEXT("zh-Hans")));
        Test->TestEqual(TEXT("formal localization changes retained FText"),Stable.ToString(),FString(TEXT("安保方案")));
        FOperationalEvent E;E.EventType=TEXT("UAV_ASSIGNED");E.Params=MakeShared<FJsonObject>();E.Params->SetStringField(TEXT("uav_id"),TEXT("UAV-01"));E.Params->SetStringField(TEXT("mission_name"),TEXT("East Perimeter"));
        Test->TestTrue(TEXT("structured event renders Chinese without changing input"),E.DisplayText().ToString().Contains(TEXT("已分配")) && E.DisplayText().ToString().Contains(TEXT("East Perimeter")));
        S->ApplyUIPreferences(Language(90001,TEXT("en")));Test->TestEqual(TEXT("retained FText switches English"),Stable.ToString(),FString(TEXT("SECURITY PLAN")));
        Test->TestTrue(TEXT("structured event re-renders English"),E.DisplayText().ToString().Contains(TEXT("assigned to mission East Perimeter")));
        Test->TestFalse(TEXT("stale language rejected"),S->ApplyUIPreferences(Language(89999,TEXT("zh-Hans"))));
        Test->TestFalse(TEXT("stale video target rejected"),S->ApplyVideoView(Video(89999,TEXT("UAV-02"))));
        Test->TestTrue(TEXT("language retains context object"),ContextBefore==S->GetContext());Test->TestTrue(TEXT("language retains plans and review object"),PlansBefore==S->GetPlans());
        Test->TestEqual(TEXT("language keeps video target"),S->GetVideoView()->GetStringField(TEXT("video_target_uav_id")),FString(TEXT("UAV-01")));
        if(Shell){Shell->Refresh();Test->TestEqual(TEXT("language does not reload browser"),Shell->GetBrowserGeneration(),Generation);Test->TestTrue(TEXT("browser object retained"),Shell->GetBrowser()==Browser);Test->TestFalse(TEXT("no source never playing"),Shell->GetMediaState()==TEXT("PLAYING"));S->ApplyVideoView(Video(90001,TEXT("UAV-02")));Shell->Refresh();Test->TestEqual(TEXT("explicit video target changes source"),Shell->GetDisplayedDroneId(),2);}
        if(!IsVideo && PC){FDronePathSaveData D;D.PathId=1;FDroneWaypointSaveData A;A.Location=FVector(10,20,30);D.Waypoints.Add(A);PC->LoadMissionPath(D,true);S->ApplyUIPreferences(Language(90002,TEXT("zh-Hans")));Test->TestTrue(TEXT("language leaves route editor open"),PC->IsPathEditMode());Test->TestEqual(TEXT("language retains draft waypoint"),PC->BuildEditingPathsData().FindChecked(1).Waypoints.Num(),1);PC->ClearEditingPaths();S->ApplyUIPreferences(Language(90003,TEXT("en")));}
        return true;
    }
};
class FP4CommandLive:public IAutomationLatentCommand {
    FAutomationTestBase* Test;int Step=0;bool Waiting=false,Done=false;double Started=FPlatformTime::Seconds();FString Plan,Mission;
public:explicit FP4CommandLive(FAutomationTestBase* T):Test(T){}
    bool Update() override {
        if(Done)return true;if(FPlatformTime::Seconds()-Started>60){Test->AddError(TEXT("Live P4 workflow timed out"));return true;}if(Waiting)return false;
        auto* W=World();if(!W)return false;auto* S=W->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();if(!S->IsReady())return false;
        const TCHAR* Actions[]={TEXT("create_plan"),TEXT("add_mission"),TEXT("rename_mission"),TEXT("assign"),TEXT("validate"),TEXT("save_route"),TEXT("delete_mission")};if(Step>=7)return true;
        auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),Actions[Step]);R->SetStringField(TEXT("name"),TEXT("P4 UE live configuration"));R->SetStringField(TEXT("plan_id"),Plan);R->SetStringField(TEXT("mission_id"),Mission);R->SetStringField(TEXT("assigned_uav_id"),TEXT("UAV-01"));Waiting=true;
        if(!S->SubmitPlan(R,[this,S](TSharedPtr<FJsonObject> Reply){Waiting=false;if(!Reply){Test->AddError(TEXT("Live Backend response missing"));Done=true;return;}
            if(Step==5){Test->TestTrue(TEXT("Command route mutation rejected by Backend"),Reply->HasField(TEXT("error")) || Reply->HasField(TEXT("code")));++Step;return;}
            if(Reply->HasField(TEXT("error")) || Reply->HasField(TEXT("code"))){Test->AddError(TEXT("Authorized Command workflow failed"));Done=true;return;}
            if(Step==0)Plan=Reply->GetStringField(TEXT("plan_id"));if(Step==1)Mission=Reply->GetStringField(TEXT("mission_id"));
            const auto P=S->GetPlans()->GetObjectField(TEXT("plans"))->GetObjectField(Plan);Test->TestTrue(TEXT("Backend acknowledged live workflow"),P.IsValid());
            if(Step==4){Test->TestEqual(TEXT("missing route remains DRAFT"),P->GetStringField(TEXT("status")),FString(TEXT("DRAFT")));Test->TestFalse(TEXT("configuration is not flight ready"),P->GetObjectField(TEXT("validation"))->GetBoolField(TEXT("ready")));}++Step;
        })){Waiting=false;Test->AddError(TEXT("Live request could not be sent"));return true;}return false;
    }
};
}
// Exercise the real Map widget, actors, undo stack and real HTTP failure/ACK path.
// Requires p4_ue_route_fixture.mjs and the isolated P4 Backend endpoints.
class FP4MapDraft:public IAutomationLatentCommand {
    FAutomationTestBase* Test;int Step=0;double Started=FPlatformTime::Seconds();
    TWeakObjectPtr<UMapMissionRouteWidget> Panel;TWeakObjectPtr<ADronePathActor> Actor;
    int UndoCount=0,PointCount=0;FString Plan,Mission;int64 Version=0;
public:explicit FP4MapDraft(FAutomationTestBase* T):Test(T){}
    bool Update() override {
        if(FPlatformTime::Seconds()-Started>75){Test->AddError(TEXT("Map live draft test timeout"));return true;}
        auto* W=World();if(!W)return false;auto* S=W->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
        auto* PC=Cast<ADroneOpsPlayerController>(W->GetFirstPlayerController());if(!PC || !S->IsReady() || !S->GetPlans())return false;
        if(!Panel.IsValid()){for(TObjectIterator<UMapMissionRouteWidget> I;I;++I)if(I->GetWorld()==W){Panel=*I;break;}if(!Panel.IsValid())return false;}
        auto* P=Panel.Get();P->Refresh();
        if(Step==0){
            auto Plans=PlanUI::Object(S->GetPlans(),TEXT("plans"));for(const auto& Entry:Plans->Values)if(PlanUI::Field(Entry.Value->AsObject(),TEXT("name"))==TEXT("P4 UE ROUTE GUARD")){Plan=FString(Entry.Key.ToView());Mission=Entry.Value->AsObject()->GetArrayField(TEXT("mission_ids"))[0]->AsString();}
            if(Plan.IsEmpty()){Test->AddError(TEXT("Route fixture missing"));return true;}
            P->SwitchTo(Plan,Mission,true);Step=1;return false;
        }
        if(Step==1){
            if(P->bPending)return false;if(P->SessionId.IsEmpty()){Test->AddError(TEXT("Backend begin ACK missing"));return true;}
            Test->TestTrue(TEXT("editing begins after lease ACK"),PC->IsPathEditMode());
            PC->AddGeographicWaypointInEditMode(EGeographicCoordinateSystem::WGS84,116.34,39.98,80);
            PC->AddGeographicWaypointInEditMode(EGeographicCoordinateSystem::WGS84,116.341,39.981,80);
            P->Refresh();Step=2;return false;
        }
        if(Step==2){
            if(P->bPending)return false;Test->TestTrue(TEXT("geometry marks local draft dirty"),P->bDirty);
            PointCount=P->RouteJson()->GetArrayField(TEXT("waypoints")).Num();Test->TestTrue(TEXT("real waypoint actors created"),PointCount>=2);
            UndoCount=PC->EditUndoStack.Num();Actor=(PC->EditingPaths.IsEmpty()?nullptr:PC->EditingPaths[0].Get());
            P->SwitchTo(Plan,TEXT(""),false);Test->TestTrue(TEXT("remote empty mission prompts instead of replacing draft"),P->bPrompt);
            Test->TestEqual(TEXT("pinned mission survives remote selection"),P->MissionId,Mission);
            P->Action(TEXT("cancel"),0);Test->TestFalse(TEXT("cancel dismisses leave prompt"),P->bPrompt);
            Version=S->GetPlans()->GetNumberField(TEXT("version"));S->GetPlans()->SetNumberField(TEXT("version"),0);
            P->Save();Test->TestTrue(TEXT("save remains open until HTTP ACK"),PC->IsPathEditMode());Step=3;return false;
        }
        if(Step==3){
            if(P->bPending)return false;S->GetPlans()->SetNumberField(TEXT("version"),Version);
            Test->TestEqual(TEXT("real stale-version rejection"),P->ErrorCode,FString(TEXT("VERSION_CONFLICT")));
            Test->TestTrue(TEXT("failed save retains editor and dirty state"),PC->IsPathEditMode() && P->bDirty && !P->SessionId.IsEmpty());
            Test->TestEqual(TEXT("failed save retains waypoints"),P->RouteJson()->GetArrayField(TEXT("waypoints")).Num(),PointCount);
            Test->TestEqual(TEXT("failed save retains undo history"),PC->EditUndoStack.Num(),UndoCount);
            Test->TestTrue(TEXT("failed save retains original path actor"),Actor.IsValid() && (PC->EditingPaths.IsEmpty()?nullptr:PC->EditingPaths[0].Get())==Actor.Get());
            P->Save();Step=4;return false;
        }
        if(Step==4){
            if(P->bPending)return false;Test->TestTrue(TEXT("draft save ACK retains session and editor"),!P->SessionId.IsEmpty() && PC->IsPathEditMode() && !P->bDirty);
            Test->TestTrue(TEXT("successful save has no error"),P->ErrorCode.IsEmpty());
            const auto M=PlanUI::Find(S->GetPlans(),TEXT("missions"),Mission);const auto R=PlanUI::Find(S->GetPlans(),TEXT("paths"),PlanUI::Field(M,TEXT("route_id")));
            Test->TestTrue(TEXT("server saved route is hydrated"),R.IsValid() && R->GetArrayField(TEXT("waypoints")).Num()==PointCount);
            P->Begin();Step=5;return false;
        }
        if(Step==5){
            if(P->bPending)return false;PC->AddGeographicWaypointInEditMode(EGeographicCoordinateSystem::WGS84,116.342,39.982,80);P->Refresh();Step=6;return false;
        }
        if(Step==6){if(P->bPending)return false;P->SwitchTo(Plan,TEXT(""),false);P->Discard();Step=7;return false;}
        if(P->bPending)return false;Test->TestTrue(TEXT("discard ACK permits pending empty mission selection"),P->MissionId.IsEmpty() && P->SessionId.IsEmpty() && !P->bDirty);return true;
    }
};

// Uses the real Command workspace and the deployed protocol fixture.
class FP4CommandReviewCopy:public IAutomationLatentCommand {
    FAutomationTestBase* Test;int Step=0;double Started=FPlatformTime::Seconds();
    TWeakObjectPtr<USecurityPlanWorkspaceWidget> Panel;FString Original,Copy,DeploymentBefore;int64 ContextVersion=0;
public:explicit FP4CommandReviewCopy(FAutomationTestBase* T):Test(T){}
    bool Update() override {
        if(FPlatformTime::Seconds()-Started>65){Test->AddError(TEXT("Command review/copy timeout"));return true;}
        auto* W=World();if(!W)return false;auto* S=W->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();if(!S->IsReady()||!S->GetPlans())return false;
        if(!Panel.IsValid()){for(TObjectIterator<USecurityPlanWorkspaceWidget> I;I;++I)if(I->GetWorld()==W){Panel=*I;break;}if(!Panel.IsValid())return false;}
        auto* P=Panel.Get();P->Refresh();if(P->bPending)return false;
        auto Send=[&](const TCHAR* A,const FString& ID){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),A);R->SetStringField(TEXT("plan_id"),ID);P->Submit(R);};
        if(Step==0){for(const auto& E:PlanUI::Object(S->GetPlans(),TEXT("plans"))->Values)if(PlanUI::Field(E.Value->AsObject(),TEXT("status"))==TEXT("DEPLOYED")){Original=FString(E.Key.ToView());break;}if(Original.IsEmpty()){Test->AddError(TEXT("Run P4 protocol fixture before Command review/copy test"));return true;}
            FJsonSerializer::Serialize(PlanUI::Object(S->GetPlans(),TEXT("deployments")).ToSharedRef(),TJsonWriterFactory<>::Create(&DeploymentBefore));Send(TEXT("select"),Original);++Step;return false;}
        if(Step==1){Test->TestTrue(TEXT("deployed name is read only"),P->PlanName->GetIsReadOnly());Test->TestFalse(TEXT("deployed content action disabled"),P->Actions[TEXT("update_plan")]->GetIsEnabled());Test->TestEqual(TEXT("copy action visible"),P->Actions[TEXT("copy_plan")]->GetVisibility(),ESlateVisibility::Visible);
            ContextVersion=S->GetContext()->GetNumberField(TEXT("context_version"));P->Refresh();P->Refresh();Test->TestFalse(TEXT("ComboBox refresh does not send selection"),P->bPending);++Step;return false;}
        if(Step==2){Test->TestEqual(TEXT("ComboBox refresh leaves authoritative selection version"),int64(S->GetContext()->GetNumberField(TEXT("context_version"))),ContextVersion);CollectGarbage(RF_NoFlags);P->Action(TEXT("copy_plan"),0);++Step;return false;}
        if(Step==3){Copy=P->PlanId;Test->TestTrue(TEXT("copy selects new plan"),!Copy.IsEmpty()&&Copy!=Original);if(Copy.IsEmpty()||Copy==Original)return true;const auto C=PlanUI::Find(S->GetPlans(),TEXT("plans"),Copy);Test->TestEqual(TEXT("copy is DRAFT"),PlanUI::Field(C,TEXT("status")),FString(TEXT("DRAFT")));Test->TestFalse(TEXT("copy is editable"),P->PlanName->GetIsReadOnly());P->Action(TEXT("validate"),0);++Step;return false;}
        if(Step==4){P->Action(TEXT("review"),0);++Step;return false;}
        if(Step==5){Test->TestTrue(TEXT("review bound to copied revision"),PlanUI::Reviewed(PlanUI::Find(S->GetPlans(),TEXT("plans"),Copy)));P->PlanName->SetText(FText::AsCultureInvariant(TEXT("P4 UE changed after review")));P->Action(TEXT("update_plan"),0);++Step;return false;}
        const auto C=PlanUI::Find(S->GetPlans(),TEXT("plans"),Copy);Test->TestFalse(TEXT("content mutation invalidates review"),PlanUI::Reviewed(C));Test->TestEqual(TEXT("changed content is DRAFT"),PlanUI::Field(C,TEXT("status")),FString(TEXT("DRAFT")));Test->TestEqual(TEXT("deploy is hidden after invalidation"),P->Actions[TEXT("deploy_page")]->GetVisibility(),ESlateVisibility::Collapsed);
        FString After;FJsonSerializer::Serialize(PlanUI::Object(S->GetPlans(),TEXT("deployments")).ToSharedRef(),TJsonWriterFactory<>::Create(&After));Test->TestEqual(TEXT("copy and edit leave original deployment immutable"),After,DeploymentBefore);return true;
    }
};
class FP52ExecutionWidgets:public IAutomationLatentCommand {
    FAutomationTestBase* Test;int Step=0;double Started=FPlatformTime::Seconds();bool Command;
public:FP52ExecutionWidgets(FAutomationTestBase* T,bool C):Test(T),Command(C){}
    bool Update() override {
        if(FPlatformTime::Seconds()-Started>45){Test->AddError(TEXT("Execution widget hydration timeout"));return true;}
        auto* W=World();if(!W)return false;auto* S=W->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();if(!S->IsHydrated())return false;
        const auto E=ExecutionUI::Latest(S->GetPlans(),TEXT(""),true);if(!E){Test->AddError(TEXT("P52 paused protocol fixture missing"));return true;}
        if(!Command){UMapExecutionWidget* Monitor=nullptr;for(TObjectIterator<UMapExecutionWidget> I;I;++I)if(I->GetWorld()==W){Monitor=*I;break;}
            int32 DroneId=0;LexTryParseString(DroneId,*PlanUI::Field(E,TEXT("uav_id")).Mid(4));
            auto* Registry=W->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
            auto* Mirror=Registry->GetReceiverActor(DroneId);auto* Preview=Registry->GetSenderPawn(DroneId);
            if(!Mirror || !Preview)return false;
            Test->TestNotNull(TEXT("Map execution monitor exists"),Monitor);if(Monitor){Monitor->Refresh();Test->TestTrue(TEXT("Map execution monitor visible after hydration"),Monitor->IsVisible());}
            Test->TestTrue(TEXT("Mock execution suppresses duplicate local preview"),Preview->IsHidden() && !Preview->IsActorTickEnabled());
            Test->TestTrue(TEXT("Mock receiver retains visible aircraft presentation"),!Mirror->IsHidden());
            Test->TestTrue(TEXT("Preview logical position matches authoritative receiver"),Preview->GetActorLocation().Equals(Mirror->GetActorLocation(),.01));
            const auto Before=S->GetVideoView();Test->TestTrue(TEXT("execution retains deployed geometry"),PlanUI::Object(E,TEXT("route_snapshot"))->GetArrayField(TEXT("waypoints")).Num()>=4);
            Test->TestTrue(TEXT("Map UI refresh does not alter video target"),Before==S->GetVideoView());return true;}
        USecurityPlanWorkspaceWidget* P=nullptr;for(TObjectIterator<USecurityPlanWorkspaceWidget> I;I;++I)if(I->GetWorld()==W){P=*I;break;}if(!P)return false;
        if(Step==0){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("select"));R->SetStringField(TEXT("plan_id"),PlanUI::Field(E,TEXT("plan_id")));R->SetStringField(TEXT("mission_id"),PlanUI::Field(E,TEXT("mission_id")));P->Submit(R);++Step;return false;}
        if(P->bPending)return false;P->bList=false;P->Refresh();
        if(Step==1){Test->TestTrue(TEXT("execution workspace restored"),P->ExecutionPage->IsVisible());Test->TestTrue(TEXT("paused execution displays Resume"),P->Actions[TEXT("execution_resume")]->IsVisible());P->ExecutionAction(TEXT("execution_resume"));++Step;return false;}
        if(Step==2){if(PlanUI::Field(E,TEXT("state"))!=TEXT("EXECUTING"))return false;Test->TestTrue(TEXT("executing displays Pause"),P->Actions[TEXT("execution_pause")]->IsVisible());P->ExecutionAction(TEXT("execution_pause"));++Step;return false;}
        if(PlanUI::Field(E,TEXT("state"))!=TEXT("PAUSED"))return false;Test->TestTrue(TEXT("backend ACK restored pause controls"),P->Actions[TEXT("execution_resume")]->IsVisible());
        P->bExecutionView=false;P->Refresh();Test->TestTrue(TEXT("recent execution is available from deployed page"),P->Actions[TEXT("execution_open")]->IsVisible());return true;
    }
};
#define P4_TEST(Class,Name,Command) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,"DroneOps.P4." Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
bool Class::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Level/CesiumWorld")));ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));ADD_LATENT_AUTOMATION_COMMAND(Command);ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
P4_TEST(FP4Localization,"Localization.FTextAndDraftIsolation",FP4Replica(this,false))
P4_TEST(FP4VideoTarget,"VideoTarget.ExplicitSelectionAndBrowserIsolation",FP4Replica(this,true))
P4_TEST(FP4CommandWorkflow,"Workflow.CommandLiveCRUD",FP4CommandLive(this))
P4_TEST(FP4MapDraftTest,"Workflow.MapLiveDraftGuard",FP4MapDraft(this))
P4_TEST(FP4CommandReviewCopyTest,"Workflow.CommandReviewReadonlyCopyAndCombo",FP4CommandReviewCopy(this))
P4_TEST(FP52CommandExecution,"P52.CommandExecutionControls",FP52ExecutionWidgets(this,true))
P4_TEST(FP52MapExecution,"P52.MapExecutionMonitor",FP52ExecutionWidgets(this,false))
#undef P4_TEST
#endif
