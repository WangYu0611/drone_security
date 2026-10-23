#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Shared/SecurityPlanWorkspaceWidget.h"
#include "Shared/PlanWidgetSupport.h"
#include "Serialization/JsonSerializer.h"

namespace {
TSharedPtr<FJsonObject> PlanUxObject(const FString& Json){
    TSharedPtr<FJsonObject> Result;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Result);return Result;
}
TSharedPtr<FJsonObject> ScheduledUxExecution(const FString& Id,const FString& At){
    auto E=MakeShared<FJsonObject>();E->SetStringField(TEXT("execution_id"),Id);E->SetStringField(TEXT("state"),TEXT("SCHEDULED"));E->SetStringField(TEXT("scheduled_start_at"),At);return E;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlanStartReadinessUxTest,"DroneOps.PlanConfiguration.StartReadiness",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlanStartReadinessUxTest::RunTest(const FString&){
    const auto State=PlanUxObject(TEXT("{\"mock_execution\":{\"uavs\":{\"UAV-01\":{},\"UAV-02\":{}}},\"executions\":{}}"));
    const auto Mission=PlanUxObject(TEXT("{\"assigned_uav_id\":\"UAV-01\",\"assigned_uav_ids\":[\"UAV-01\",\"UAV-02\"]}"));
    const TArray<FString> Available{TEXT("UAV-01"),TEXT("UAV-02")};
    auto Blockers=[&](const TArray<FString>& Uavs){return USecurityPlanWorkspaceWidget::ExecutionStartBlockers(State,Mission,Uavs);};
    TestTrue(TEXT("Configured Mock members do not require hardware heartbeats"),Blockers(Available).IsEmpty());
    const auto Other=PlanUxObject(TEXT("{\"execution_id\":\"other\",\"uav_id\":\"UAV-02\",\"plan_name\":\"Other Plan\",\"mission_name\":\"Other Task\",\"state\":\"EXECUTING\"}"));
    PlanUI::Object(State,TEXT("executions"))->SetObjectField(TEXT("other"),Other);
    for(const TCHAR* Status:{TEXT("SCHEDULED"),TEXT("CREATED"),TEXT("PREFLIGHT"),TEXT("STARTING"),TEXT("START_REQUESTED"),TEXT("WAITING_ACK"),TEXT("EXECUTING"),TEXT("PAUSED"),TEXT("RETURNING")}){
        Other->SetStringField(TEXT("state"),Status);const auto Busy=Blockers(Available);
        TestEqual(FString::Printf(TEXT("Secondary member in %s blocks the start"),Status),Busy.Num(),1);
        if(Busy.Num()==1){const FString Caption=Busy[0].ToString();TestTrue(TEXT("Blocker identifies the affected aircraft and owner"),Caption.Contains(TEXT("UAV-02")) && Caption.Contains(TEXT("Other Plan")) && Caption.Contains(TEXT("Other Task")));}
    }
    for(const TCHAR* Status:{TEXT("COMPLETED"),TEXT("ABORTED"),TEXT("FAILED"),TEXT("CANCELLED")}){
        Other->SetStringField(TEXT("state"),Status);TestTrue(FString::Printf(TEXT("Secondary member released by %s can start"),Status),Blockers(Available).IsEmpty());
    }
    const auto MissingRegistryMember=Blockers({TEXT("UAV-01")});TestEqual(TEXT("Unavailable secondary registry member blocks the start"),MissingRegistryMember.Num(),1);
    if(MissingRegistryMember.Num()==1)TestTrue(TEXT("Unavailable caption identifies secondary aircraft"),MissingRegistryMember[0].ToString().Contains(TEXT("UAV-02")));
    PlanUI::Object(PlanUI::Object(State,TEXT("mock_execution")),TEXT("uavs"))->RemoveField(TEXT("UAV-02"));
    TestEqual(TEXT("Missing secondary adapter configuration blocks the start"),Blockers(Available).Num(),1);
    const auto Legacy=MakeShared<FJsonObject>(*Mission);Legacy->RemoveField(TEXT("assigned_uav_ids"));
    TestTrue(TEXT("Legacy single-member assignment remains valid"),USecurityPlanWorkspaceWidget::ExecutionStartBlockers(State,Legacy,Available).IsEmpty());
    Mission->SetArrayField(TEXT("assigned_uav_ids"),TArray<TSharedPtr<FJsonValue>>{});TestFalse(TEXT("An empty explicit assignment cannot be started"),Blockers(Available).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlanScheduleDraftUxTest,"DroneOps.PlanConfiguration.ScheduleDraft",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlanScheduleDraftUxTest::RunTest(const FString&){
    // Exercise the production input methods without a connection or Backend mutation.
    auto* Workspace=NewObject<USecurityPlanWorkspaceWidget>();Workspace->ScheduleInput=NewObject<UEditableTextBox>(Workspace);
    Workspace->ResetStartSchedule(FDateTime(2033,1,2,12,0,0));
    TestEqual(TEXT("First intent gets a future local default"),Workspace->ScheduleInput->GetText().ToString(),FString(TEXT("2033-01-02T12:02:00")));
    Workspace->ScheduleInput->SetText(PlanUI::User(TEXT("2033-01-03T10:00:00")));
    Workspace->ResetStartSchedule(FDateTime(2033,1,2,15,0,0));
    TestEqual(TEXT("Later new intent cannot reuse expired or cancelled input"),Workspace->ScheduleInput->GetText().ToString(),FString(TEXT("2033-01-02T15:02:00")));
    const auto First=ScheduledUxExecution(TEXT("first"),TEXT("2034-04-05T10:00:00Z"));Workspace->RefreshScheduleInput(First);
    const FString FirstLoaded=Workspace->ScheduleInput->GetText().ToString();FDateTime Parsed;
    TestTrue(TEXT("Existing scheduled execution loads an editable local date"),FDateTime::ParseIso8601(*(FirstLoaded+TEXT("Z")),Parsed));
    TestEqual(TEXT("Existing appointment is associated with its execution"),Workspace->ScheduleExecutionId,FString(TEXT("first")));
    const FString Draft=TEXT("2035-06-07T12:34:56");Workspace->ScheduleInput->SetText(PlanUI::User(Draft));
    Workspace->RefreshScheduleInput(First);TestEqual(TEXT("Heartbeat refresh preserves a reschedule draft"),Workspace->ScheduleInput->GetText().ToString(),Draft);
    First->SetStringField(TEXT("scheduled_start_at"),TEXT("2034-04-06T10:00:00Z"));Workspace->RefreshScheduleInput(First);
    TestEqual(TEXT("New acknowledged time does not overwrite typing in the same execution"),Workspace->ScheduleInput->GetText().ToString(),Draft);
    const auto Second=ScheduledUxExecution(TEXT("second"),TEXT("2036-08-09T11:00:00Z"));Workspace->RefreshScheduleInput(Second);
    const FString SecondLoaded=Workspace->ScheduleInput->GetText().ToString();TestTrue(TEXT("Switching execution replaces the previous task's draft"),SecondLoaded!=Draft);
    TestEqual(TEXT("Replacement input belongs to the newly selected execution"),Workspace->ScheduleExecutionId,FString(TEXT("second")));
    Second->SetStringField(TEXT("scheduled_start_at"),TEXT("2036-08-10T11:00:00Z"));Workspace->RefreshScheduleInput(Second);
    TestTrue(TEXT("Acknowledged reschedule refreshes an unedited input"),Workspace->ScheduleInput->GetText().ToString()!=SecondLoaded);
    Workspace->RefreshScheduleInput(First);TestEqual(TEXT("Returning to a prior task loads its own accepted appointment"),Workspace->ScheduleExecutionId,FString(TEXT("first")));
    TestTrue(TEXT("Returning to a prior task does not leak another task's draft"),Workspace->ScheduleInput->GetText().ToString()!=Draft);
    Workspace->ResetStartSchedule(FDateTime(2037,1,1,9,0,0));TestEqual(TEXT("A new start after browsing reservations gets a fresh future default"),Workspace->ScheduleInput->GetText().ToString(),FString(TEXT("2037-01-01T09:02:00")));
    return true;
}
#endif
