#include "Shared/SecurityPlanWorkspaceWidget.h"
#include "Shared/PlanWidgetSupport.h"
#include "Shared/PlanConfiguration.h"
#include "Components/CheckBox.h"

using namespace PlanUI;

bool USecurityPlanWorkspaceWidget::IsConfiguring() const {
    return bCreating || PlanConfiguration::Active(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans());
}

bool USecurityPlanWorkspaceWidget::CanEditConfiguration() const {
    const auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    return Sync->IsHydrated() && PlanConfiguration::IsOwner(Sync->GetPlans(),Sync->GetInstanceId())
        && (bCreating?PlanConfiguration::PlanId(Sync->GetPlans()).IsEmpty():PlanConfiguration::PlanId(Sync->GetPlans())==PlanId);
}

bool USecurityPlanWorkspaceWidget::HasUnsavedChanges() const {
    if(!PlanName || !Description)return false;
    if(bCreating)return !PlanName->GetText().IsEmpty() || !Description->GetText().IsEmpty();
    if(bList)return false;
    const auto State=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans();
    const auto Plan=Find(State,TEXT("plans"),PlanId),Mission=Find(State,TEXT("missions"),MissionId);
    if(!Plan || Field(Plan,TEXT("status"))==TEXT("DEPLOYED"))return false;
    if(PlanName->GetText().ToString()!=Field(Plan,TEXT("name")) || Description->GetText().ToString()!=Field(Plan,TEXT("description")))return true;
    if(!Mission)return false;
    if(MissionName->GetText().ToString()!=Field(Mission,TEXT("name")))return true;
    TArray<FString> Selected,Saved;
    for(const auto& Entry:AssignmentChecks)if(Entry.Value->IsChecked())Selected.Add(Entry.Key);
    const TArray<TSharedPtr<FJsonValue>>* Members=nullptr;
    if(Mission->TryGetArrayField(TEXT("assigned_uav_ids"),Members))for(const auto& V:*Members)Saved.Add(V->AsString());
    else if(!Field(Mission,TEXT("assigned_uav_id")).IsEmpty())Saved.Add(Field(Mission,TEXT("assigned_uav_id")));
    Selected.Sort();Saved.Sort();if(Selected!=Saved)return true;
    double Spacing=0,SavedSpacing=3.;Mission->TryGetNumberField(TEXT("spacing_m"),SavedSpacing);
    return !LexTryParseString(Spacing,*SpacingInput->GetText().ToString()) || !FMath::IsFinite(Spacing) || !FMath::IsNearlyEqual(Spacing,SavedSpacing);
}

FText USecurityPlanWorkspaceWidget::ConfigurationStatusText() const {
    const auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    const auto State=Sync->GetPlans();
    const FText Name=bCreating?(PlanName->GetText().IsEmpty()?T(TEXT("Configuration.NewPlan")):PlanName->GetText()):PlanConfiguration::PlanName(State);
    const TCHAR* Stages[]={TEXT("Workflow.StepInfo"),TEXT("Workflow.StepTask"),TEXT("Workflow.StepRoute"),TEXT("Workflow.StepCheck"),TEXT("Workflow.StepDeploy")};
    const FText SaveState=bPending?T(TEXT("Common.Waiting")):!Sync->IsHydrated()?T(TEXT("Configuration.OfflineDraft")):
        !CanEditConfiguration()?T(TEXT("Configuration.ReadOnly")):HasUnsavedChanges()?T(TEXT("Configuration.Unsaved")):bCreating?T(TEXT("Configuration.NotCreated")):T(TEXT("Configuration.Saved"));
    return FText::Format(T(TEXT("Configuration.Context")),Name,T(Stages[FMath::Clamp(CurrentStep,0,4)]),SaveState);
}

void USecurityPlanWorkspaceWidget::BeginConfiguration(const FString& Id,TFunction<void()> Continue) {
    if(bPending)return;
    bPending=true;ErrorCode.Empty();auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("begin_configuration"));R->SetStringField(TEXT("plan_id"),Id);
    const auto Weak=TWeakObjectPtr<USecurityPlanWorkspaceWidget>(this);
    if(!GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->SubmitPlan(R,[Weak,Continue](TSharedPtr<FJsonObject> Reply){
        if(!Weak.IsValid())return;Weak->bPending=false;Weak->ErrorCode=PlanUI::Error(Reply);
        if(Weak->ErrorCode.IsEmpty() && Continue)Continue();Weak->Refresh();
    })){bPending=false;ErrorCode=TEXT("SYNC_OFFLINE");Refresh();}
}

void USecurityPlanWorkspaceWidget::GuardConfigurationChange(TFunction<void()> Continue) {
    if(bPending)return;
    if(!HasUnsavedChanges()){if(Continue)Continue();return;}
    GuardedConfigurationChange=MoveTemp(Continue);bConfigurationGuard=true;Refresh();
    if(auto* Scroll=Cast<UScrollBox>(Content->GetParent()))Scroll->ScrollWidgetIntoView(ConfigurationGuard,false);
}

void USecurityPlanWorkspaceWidget::FocusConfiguration() { bList=false;Refresh(); }

void USecurityPlanWorkspaceWidget::RequestFinishConfiguration(TFunction<void()> OnFinished) {
    if(bPending)return;
    AfterConfigurationFinish=MoveTemp(OnFinished);
    GuardConfigurationChange([this]{FinishConfiguration();});
}

void USecurityPlanWorkspaceWidget::FinishConfiguration() {
    if(bPending)return;
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    if(!PlanConfiguration::Active(Sync->GetPlans())){
        bCreating=false;bList=true;auto Continue=MoveTemp(AfterConfigurationFinish);if(Continue)Continue();Refresh();return;
    }
    bPending=true;auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("end_configuration"));
    R->SetStringField(TEXT("plan_id"),PlanConfiguration::PlanId(Sync->GetPlans()));
    const auto Weak=TWeakObjectPtr<USecurityPlanWorkspaceWidget>(this);
    if(!Sync->SubmitPlan(R,[Weak](TSharedPtr<FJsonObject> Reply){
        if(!Weak.IsValid())return;Weak->bPending=false;Weak->ErrorCode=PlanUI::Error(Reply);
        if(Weak->ErrorCode.IsEmpty()){
            Weak->bCreating=false;Weak->bList=true;Weak->bConfigurationGuard=false;
            auto Continue=MoveTemp(Weak->AfterConfigurationFinish);if(Continue)Continue();
        }Weak->Refresh();
    })){bPending=false;ErrorCode=TEXT("SYNC_OFFLINE");Refresh();}
}

void USecurityPlanWorkspaceWidget::ResetConfigurationInputs() {
    ErrorCode.Empty();
    if(bCreating){PlanName->SetText(FText::GetEmpty());Description->SetText(FText::GetEmpty());return;}
    LoadedSelection.Empty();AssignmentSelection.Empty();Refresh();
}

void USecurityPlanWorkspaceWidget::SaveConfiguration(TFunction<void()> Continue) {
    if(bPending)return;
    if(!CanEditConfiguration()){ErrorCode=TEXT("CONFIGURATION_IN_PROGRESS");Refresh();return;}
    ErrorCode.Empty();ConfigurationSaveQueue.Empty();
    const auto State=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans();
    const auto Plan=Find(State,TEXT("plans"),PlanId),Mission=Find(State,TEXT("missions"),MissionId);
    auto Request=[&](const TCHAR* Action){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),Action);R->SetStringField(TEXT("plan_id"),PlanId);R->SetStringField(TEXT("mission_id"),MissionId);return R;};
    const FString Name=PlanName->GetText().ToString();
    if(Name.TrimStartAndEnd().IsEmpty()){ErrorCode=TEXT("INVALID_NAME");Refresh();return;}
    if(bCreating || (Plan && (Name!=Field(Plan,TEXT("name")) || Description->GetText().ToString()!=Field(Plan,TEXT("description"))))) {
        auto R=Request(bCreating?TEXT("create_plan"):TEXT("update_plan"));R->SetStringField(TEXT("name"),Name);R->SetStringField(TEXT("description"),Description->GetText().ToString());
        if(bCreating)R->SetStringField(TEXT("default_mission_name"),T(TEXT("Workflow.DefaultTask")).ToString());ConfigurationSaveQueue.Add(R);
    }
    if(!bCreating && Mission) {
        const FString NewMissionName=MissionName->GetText().ToString();
        if(NewMissionName!=Field(Mission,TEXT("name"))){
            if(NewMissionName.TrimStartAndEnd().IsEmpty()){ErrorCode=TEXT("INVALID_NAME");Refresh();return;}
            auto R=Request(TEXT("rename_mission"));R->SetStringField(TEXT("name"),NewMissionName);ConfigurationSaveQueue.Add(R);
        }
        TArray<FString> Selected,Saved;for(const auto& Entry:AssignmentChecks)if(Entry.Value->IsChecked())Selected.Add(Entry.Key);
        const TArray<TSharedPtr<FJsonValue>>* Members=nullptr;
        if(Mission->TryGetArrayField(TEXT("assigned_uav_ids"),Members))for(const auto& V:*Members)Saved.Add(V->AsString());
        else if(!Field(Mission,TEXT("assigned_uav_id")).IsEmpty())Saved.Add(Field(Mission,TEXT("assigned_uav_id")));
        Selected.Sort();Saved.Sort();double Spacing=0,SavedSpacing=3.;Mission->TryGetNumberField(TEXT("spacing_m"),SavedSpacing);
        if(!LexTryParseString(Spacing,*SpacingInput->GetText().ToString()) || !FMath::IsFinite(Spacing) || Spacing<=0){ErrorCode=TEXT("INVALID_SEPARATION");Refresh();return;}
        if(Selected!=Saved || !FMath::IsNearlyEqual(Spacing,SavedSpacing)){
            auto R=Request(TEXT("assign"));TArray<TSharedPtr<FJsonValue>> Values;for(const auto& Id:Selected)Values.Add(MakeShared<FJsonValueString>(Id));
            R->SetArrayField(TEXT("assigned_uav_ids"),Values);R->SetStringField(TEXT("formation"),TEXT("Line"));R->SetNumberField(TEXT("spacing_m"),Spacing);ConfigurationSaveQueue.Add(R);
        }
    }
    AfterConfigurationSave=MoveTemp(Continue);bPending=true;SaveNextConfigurationChange();
}

void USecurityPlanWorkspaceWidget::SaveNextConfigurationChange() {
    if(ConfigurationSaveQueue.IsEmpty()){
        bPending=false;bConfigurationGuard=false;auto Continue=MoveTemp(AfterConfigurationSave);if(Continue)Continue();Refresh();return;
    }
    auto R=ConfigurationSaveQueue[0];ConfigurationSaveQueue.RemoveAt(0);const bool Creating=Field(R,TEXT("action"))==TEXT("create_plan");
    const auto Weak=TWeakObjectPtr<USecurityPlanWorkspaceWidget>(this);
    if(!GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->SubmitPlan(R,[Weak,Creating](TSharedPtr<FJsonObject> Reply){
        if(!Weak.IsValid())return;Weak->ErrorCode=PlanUI::Error(Reply);Weak->ExecutionErrorDetails=ReservationError(Reply);
        if(!Weak->ErrorCode.IsEmpty()){Weak->bPending=false;Weak->ConfigurationSaveQueue.Empty();Weak->AfterConfigurationSave={};Weak->Refresh();return;}
        if(Creating){Weak->bCreating=false;Weak->bList=false;Weak->LoadedSelection.Empty();Weak->Refresh();}
        Weak->SaveNextConfigurationChange();
    })){bPending=false;ConfigurationSaveQueue.Empty();AfterConfigurationSave={};ErrorCode=TEXT("SYNC_OFFLINE");Refresh();}
}

void USecurityPlanWorkspaceWidget::RefreshConfiguration() {
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();const bool Active=IsConfiguring(),Owner=CanEditConfiguration();
    const auto Plan=Find(Sync->GetPlans(),TEXT("plans"),PlanId);
    const bool Deployed=!bCreating && Field(Plan,TEXT("status"))==TEXT("DEPLOYED");
    ConfigurationStatus->SetText(T(Deployed?TEXT("Configuration.DeployedReadOnly"):TEXT("Configuration.ReadOnly")));
    // The shell keeps the current plan/stage and Finish action visible across pages.
    ConfigurationStatus->SetVisibility(!Active && !bList?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    ConfigurationGuard->SetVisibility(bConfigurationGuard?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    const bool Editable=Owner && !Deployed && !bPending && !bConfigurationGuard;
    Workspace->SetIsEnabled(!bConfigurationGuard);
    ListPage->SetIsEnabled(!bConfigurationGuard);
    for(auto* Input:{PlanName.Get(),Description.Get(),MissionName.Get(),SpacingInput.Get()})Input->SetIsReadOnly(!Editable);
    AssignmentList->SetIsEnabled(Editable);MissionList->SetIsEnabled(!bPending && !bConfigurationGuard);
    const TSet<FName> EditingActions={TEXT("create_plan"),TEXT("update_plan"),TEXT("add_mission"),TEXT("rename_mission"),TEXT("delete_mission"),TEXT("assign"),TEXT("assign_group"),TEXT("unassign"),TEXT("clear_unavailable"),TEXT("request_map_route_edit"),TEXT("basic_step"),TEXT("task_step"),TEXT("next_task"),TEXT("confirm_deployment")};
    for(const auto& Key:EditingActions)if(Actions.Contains(Key))Actions[Key]->SetIsEnabled(Actions[Key]->GetIsEnabled() && Editable);
    for(const TCHAR* Key:{TEXT("guard_save"),TEXT("guard_discard"),TEXT("guard_keep")})Actions[Key]->SetIsEnabled(!bPending && (Key!=FString(TEXT("guard_save")) || Owner));
    Actions[TEXT("resume_configuration")]->SetVisibility(!bList && !bCreating && !Deployed && !Owner && !PlanId.IsEmpty()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    Actions[TEXT("resume_configuration")]->SetIsEnabled(!bPending && Sync->IsHydrated() && !PlanConfiguration::Active(Sync->GetPlans()));
    Actions[TEXT("request_map_plan_move")]->SetVisibility(Active?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
    if(Active)for(const TCHAR* Key:{TEXT("execution_start_prompt"),TEXT("execution_start_now"),TEXT("execution_reschedule")})Actions[Key]->SetVisibility(ESlateVisibility::Collapsed);
    // The persistent shell controls expose pause/abort without selecting another plan.
    if(auto* Caption=Cast<UTextBlock>(Actions[TEXT("back_list")]->GetContent()))Caption->SetText(T(Active?TEXT("Configuration.Finish"):TEXT("Workflow.BackPlans")));
    Actions[TEXT("back_list")]->SetVisibility(Active?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
}
