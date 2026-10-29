#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Map/MapMoveSupport.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Misc/ScopeExit.h"

namespace {
struct FTakeoverFixture {
    TSharedRef<FJsonObject> State=MakeShared<FJsonObject>(),All=MakeShared<FJsonObject>(),Plans=MakeShared<FJsonObject>();
    FTakeoverFixture(){State->SetObjectField(TEXT("executions"),All);State->SetObjectField(TEXT("plans"),Plans);
        auto Plan=MakeShared<FJsonObject>();Plan->SetStringField(TEXT("deployment_id"),TEXT("deployment"));Plans->SetObjectField(TEXT("plan"),Plan);}
    TSharedRef<FJsonObject> Add(const FString& Id,int Uav,const FString& Status=TEXT("EXECUTING"),double Created=2,const FString& Group=TEXT("mission-group")){
        auto E=MakeShared<FJsonObject>();E->SetStringField(TEXT("execution_id"),Id);E->SetStringField(TEXT("uav_id"),MapMove::UAV(Uav));
        E->SetStringField(TEXT("state"),Status);E->SetStringField(TEXT("plan_id"),TEXT("plan"));E->SetStringField(TEXT("mission_id"),TEXT("mission"));
        E->SetStringField(TEXT("deployment_id"),TEXT("deployment"));E->SetStringField(TEXT("group_id"),Group);E->SetNumberField(TEXT("created_at"),Created);
        E->SetNumberField(TEXT("control_version"),3);All->SetObjectField(Id,E);return E;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualTakeoverEligibility,"DroneOps.Fleet.ManualTakeover.Eligibility",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FManualTakeoverEligibility::RunTest(const FString&){
    FTakeoverFixture F;const auto E=F.Add(TEXT("source"),1);F.Add(TEXT("peer"),2);
    TestTrue(TEXT("legacy deployed task without control_kind is eligible"),MapMove::CaptureTakeover(F.State,{1}).IsSet());
    for(const auto* Status:{TEXT("EXECUTING"),TEXT("PAUSED"),TEXT("RETURNING")}){
        E->SetStringField(TEXT("state"),Status);TestTrue(TEXT("task has a controllable active state"),MapMove::CaptureTakeover(F.State,{1}).IsSet());
    }
    for(const auto* Status:{TEXT("SCHEDULED"),TEXT("CREATED"),TEXT("PREFLIGHT"),TEXT("STARTING"),TEXT("START_REQUESTED"),TEXT("WAITING_ACK"),TEXT("COMPLETED"),TEXT("ABORTED"),TEXT("FAILED"),TEXT("CANCELLED")}){
        E->SetStringField(TEXT("state"),Status);TestFalse(TEXT("pending and terminal tasks cannot be taken over"),MapMove::CaptureTakeover(F.State,{1}).IsSet());
    }
    E->SetStringField(TEXT("state"),TEXT("EXECUTING"));
    TestFalse(TEXT("takeover requires one selected aircraft"),MapMove::CaptureTakeover(F.State,{1,2}).IsSet());
    TestFalse(TEXT("empty selection cannot be taken over"),MapMove::CaptureTakeover(F.State,{}).IsSet());
    TestFalse(TEXT("idle aircraft is ordinary movement"),MapMove::CaptureTakeover(F.State,{3}).IsSet());
    E->SetStringField(TEXT("deployment_id"),TEXT(""));TestFalse(TEXT("undeployed task is blocked"),MapMove::CaptureTakeover(F.State,{1}).IsSet());
    E->SetStringField(TEXT("deployment_id"),TEXT("deployment"));E->SetStringField(TEXT("plan_id"),TEXT(""));TestFalse(TEXT("free movement cannot be taken over as a mission"),MapMove::CaptureTakeover(F.State,{1}).IsSet());
    E->SetStringField(TEXT("plan_id"),TEXT("plan"));E->SetStringField(TEXT("control_kind"),TEXT("GROUP_MOVE"));TestFalse(TEXT("existing group move remains under its original controls"),MapMove::CaptureTakeover(F.State,{1}).IsSet());
    E->RemoveField(TEXT("control_kind"));E->RemoveField(TEXT("control_version"));TestFalse(TEXT("missing control version cannot authorize takeover"),MapMove::CaptureTakeover(F.State,{1}).IsSet());
    E->SetNumberField(TEXT("control_version"),3);F.Add(TEXT("conflicting-owner"),1);
    TestFalse(TEXT("ambiguous active ownership fails closed"),MapMove::CaptureTakeover(F.State,{1}).IsSet());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualTakeoverPreview,"DroneOps.Fleet.ManualTakeover.PreviewAndRequest",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FManualTakeoverPreview::RunTest(const FString&){
    using namespace PlanUI;
    FTakeoverFixture F;const auto Source=F.Add(TEXT("source"),1);F.Add(TEXT("peer"),2);
    const FVector Destination(116.35,39.98,130.);const auto Intent=MapMove::CaptureTakeover(F.State,{1});
    Source->SetNumberField(TEXT("progress"),.6);Source->SetNumberField(TEXT("updated_at"),100);
    TestTrue(TEXT("telemetry progress does not expire an unchanged control version"),MapMove::PreviewIsCurrent(F.State,{1},Intent));
    const auto Request=MapMove::PreviewRequest(F.State,{1},Destination,Intent);
    if(!TestTrue(TEXT("fresh explicit confirmation creates a request"),Request.IsValid()))return false;
    TestEqual(TEXT("only atomic takeover action is sent"),Field(Request,TEXT("action")),FString(TEXT("execution_takeover_move")));
    TestEqual(TEXT("confirmation retains the original source execution"),Field(Request,TEXT("execution_id")),FString(TEXT("source")));
    TestEqual(TEXT("exact selected UAV is sent"),Field(Request,TEXT("uav_id")),FString(TEXT("UAV-01")));
    TestEqual(TEXT("original control version is sent"),Request->GetNumberField(TEXT("control_version")),3.);
    TestFalse(TEXT("takeover cannot carry another group assignment"),Request->HasField(TEXT("assigned_uav_ids")));
    TestFalse(TEXT("takeover has a request id"),Field(Request,TEXT("request_id")).IsEmpty());
    TestEqual(TEXT("chosen longitude is preserved"),Object(Request,TEXT("target"))->GetNumberField(TEXT("longitude")),Destination.X);
    TestEqual(TEXT("chosen latitude is preserved"),Object(Request,TEXT("target"))->GetNumberField(TEXT("latitude")),Destination.Y);
    TestEqual(TEXT("chosen altitude is preserved"),Object(Request,TEXT("target"))->GetNumberField(TEXT("altitude")),Destination.Z);
    TestEqual(TEXT("preview/request never changes the original execution"),Field(Source,TEXT("state")),FString(TEXT("EXECUTING")));
    TestEqual(TEXT("preview/request never creates a movement locally"),F.All->Values.Num(),2);
    TestFalse(TEXT("changed singleton selection cannot reuse original confirmation"),MapMove::PreviewRequest(F.State,{2},Destination,Intent).IsValid());
    TestFalse(TEXT("expanded selection cannot reuse original confirmation"),MapMove::PreviewRequest(F.State,{1,2},Destination,Intent).IsValid());
    Source->SetNumberField(TEXT("control_version"),4);
    TestFalse(TEXT("pause/resume from another client requires new review"),MapMove::PreviewRequest(F.State,{1},Destination,Intent).IsValid());
    Source->SetNumberField(TEXT("control_version"),3);Source->SetStringField(TEXT("state"),TEXT("COMPLETED"));
    TestFalse(TEXT("completed mission cannot silently downgrade takeover to ordinary move"),MapMove::PreviewRequest(F.State,{1},Destination,Intent).IsValid());
    F.Add(TEXT("replacement"),1);
    TestFalse(TEXT("new execution with same UAV and version cannot inherit confirmation"),MapMove::PreviewRequest(F.State,{1},Destination,Intent).IsValid());
    const auto IdleIntent=MapMove::CaptureTakeover(F.State,{3});F.Add(TEXT("newly-started"),3);
    TestFalse(TEXT("ordinary move preview cannot silently become destructive takeover"),MapMove::PreviewRequest(F.State,{3},Destination,IdleIntent).IsValid());
    const auto Free=MapMove::PreviewRequest(F.State,{4,5},Destination,{});
    if(TestTrue(TEXT("idle group move still builds a request"),Free.IsValid())){
        TestEqual(TEXT("idle group uses existing action"),Field(Free,TEXT("action")),FString(TEXT("execution_group_move")));
        TestEqual(TEXT("idle group retains all members"),Free->GetArrayField(TEXT("assigned_uav_ids")).Num(),2);
        TestFalse(TEXT("ordinary move carries no takeover execution"),Free->HasField(TEXT("execution_id")));
    }
    TestFalse(TEXT("active mixed group cannot bypass takeover via ordinary request"),MapMove::PreviewRequest(F.State,{2,4},Destination,{}).IsValid());
    TestFalse(TEXT("empty selection never builds a request"),MapMove::PreviewRequest(F.State,{},Destination,{}).IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualTakeoverRoutes,"DroneOps.Fleet.ManualTakeover.RouteOwnership",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FManualTakeoverRoutes::RunTest(const FString&){
    using namespace PlanUI;
    FTakeoverFixture F;F.Add(TEXT("older-completed"),1,TEXT("COMPLETED"),1,TEXT("older-group"));
    const auto Source=F.Add(TEXT("source"),1);const auto Peer=F.Add(TEXT("peer"),2);
    auto Owners=ExecutionUI::RouteOwners(F.State,TEXT("plan"));TestEqual(TEXT("original mission displays two members"),Owners.Num(),2);
    Source->SetStringField(TEXT("state"),TEXT("ABORTED"));Source->SetStringField(TEXT("completion_reason"),TEXT("MANUAL_TAKEOVER"));
    const auto Move=F.Add(TEXT("move"),1,TEXT("EXECUTING"),3,TEXT("independent-move"));Move->SetStringField(TEXT("plan_id"),TEXT(""));Move->SetStringField(TEXT("mission_id"),TEXT(""));
    Move->SetStringField(TEXT("control_kind"),TEXT("GROUP_MOVE"));Move->SetStringField(TEXT("source_execution_id"),TEXT("source"));
    Owners=ExecutionUI::RouteOwners(F.State,TEXT("plan"));TestEqual(TEXT("only new movement and continuing peer own routes"),Owners.Num(),2);
    for(const auto& E:Owners)TestTrue(TEXT("retired task route never returns via old group membership"),Field(E,TEXT("execution_id"))==TEXT("move") || Field(E,TEXT("execution_id"))==TEXT("peer"));
    TestTrue(TEXT("retired mission prevents saved route fallback"),ExecutionUI::RetiredMissionOwnsRoute(F.State,TEXT("plan"),TEXT("mission")));
    Peer->SetStringField(TEXT("state"),TEXT("ABORTED"));Peer->SetStringField(TEXT("completion_reason"),TEXT("MANUAL_TAKEOVER"));
    Owners=ExecutionUI::RouteOwners(F.State,TEXT("plan"));TestEqual(TEXT("taking over every member never resurrects an older completed route"),Owners.Num(),1);
    if(Owners.Num()==1)TestEqual(TEXT("independent movement is the sole current route"),Field(Owners[0],TEXT("execution_id")),FString(TEXT("move")));
    Find(F.State,TEXT("plans"),TEXT("plan"))->SetStringField(TEXT("deployment_id"),TEXT("redeployed"));
    TestFalse(TEXT("new deployment restores its own saved route eligibility"),ExecutionUI::RetiredMissionOwnsRoute(F.State,TEXT("plan"),TEXT("mission")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualTakeoverLocalization,"DroneOps.Fleet.ManualTakeover.Localization",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FManualTakeoverLocalization::RunTest(const FString&){
    auto& Internationalization=FInternationalization::Get();auto& Localization=FTextLocalizationManager::Get();
    FInternationalization::FCultureStateSnapshot Previous;Internationalization.BackupCultureState(Previous);
    const bool HadPreview=Localization.IsGameLocalizationPreviewEnabled();
    const FString PreviousPreview=Localization.GetConfiguredGameLocalizationPreviewLanguage();
    ON_SCOPE_EXIT {
        Internationalization.RestoreCultureState(Previous);
        if(HadPreview)Localization.EnableGameLocalizationPreview(PreviousPreview.IsEmpty()?Previous.Language:PreviousPreview);
        else Localization.DisableGameLocalizationPreview();
        Localization.WaitForAsyncTasks();
    };
    const FText Confirm=ProductText::Get(TEXT("Takeover.Confirm")),Warning=ProductText::Get(TEXT("Takeover.Warning"));
    const FText Entry=ProductText::Get(TEXT("Takeover.Action"));
    TestTrue(TEXT("Chinese culture can be selected"),Internationalization.SetCurrentCulture(TEXT("zh-Hans")));
    Localization.EnableGameLocalizationPreview(TEXT("zh-Hans"));Localization.WaitForAsyncTasks();
    TestEqual(TEXT("compiled Chinese confirm resource loads"),Confirm.ToString(),FString(TEXT("接管并移动")));
    TestEqual(TEXT("latest toolbar entry loads compiled Chinese resource"),Entry.ToString(),FString(TEXT("手动接管")));
    TestEqual(TEXT("compiled Chinese consequence warning loads"),Warning.ToString(),FString(TEXT("将终止这架无人机的当前任务，其他无人机继续；原任务不会自动恢复。")));
    TestTrue(TEXT("English culture can be selected"),Internationalization.SetCurrentCulture(TEXT("en")));
    Localization.EnableGameLocalizationPreview(TEXT("en"));Localization.WaitForAsyncTasks();
    TestEqual(TEXT("retained confirm FText returns to English"),Confirm.ToString(),FString(TEXT("Take over and move")));
    TestEqual(TEXT("toolbar entry returns to English"),Entry.ToString(),FString(TEXT("Take over")));
    TestEqual(TEXT("retained warning FText returns to English"),Warning.ToString(),FString(TEXT("This will end this aircraft's current task. Other aircraft will continue; the original task will not resume automatically.")));
    return true;
}
#endif
