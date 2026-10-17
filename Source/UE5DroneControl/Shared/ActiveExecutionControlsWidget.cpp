#include "Shared/ActiveExecutionControlsWidget.h"
#include "Shared/PlanWidgetSupport.h"
#include "Shared/ExecutionPresentation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"

using namespace PlanUI;
namespace ExecutionControls {
TArray<FGroup> Groups(const TSharedPtr<FJsonObject>& State,bool ActiveOnly) {
    TArray<FGroup> Result;TMap<FString,int32> Indices;const auto All=Object(State,TEXT("executions"));
    if(!All)return Result;
    for(const auto& Entry:All->Values){const auto E=Entry.Value->AsObject();if(!E)continue;
        FString Id=Field(E,TEXT("group_id"));if(Id.IsEmpty())Id=Field(E,TEXT("execution_id"));if(Id.IsEmpty())continue;
        int32* Found=Indices.Find(Id);int32 Index;
        if(Found)Index=*Found;else {Index=Result.AddDefaulted();Indices.Add(Id,Index);Result[Index].Id=Id;}
        auto& G=Result[Index];G.Executions.Add(E);G.Members.AddUnique(Field(E,TEXT("uav_id")));
        if(G.PlanName.IsEmpty())G.PlanName=Field(E,TEXT("plan_name"));if(G.MissionName.IsEmpty())G.MissionName=Field(E,TEXT("mission_name"));
        double Value=0;E->TryGetNumberField(TEXT("created_at"),Value);G.CreatedAt=FMath::Max(G.CreatedAt,Value);
        Value=0;E->TryGetNumberField(TEXT("progress"),Value);G.Progress+=FMath::Clamp(Value,0.,1.);
        const FString S=Field(E,TEXT("state"));if(ExecutionUI::Active(E)){
            ++G.ActiveCount;G.CanPause|=S==TEXT("EXECUTING") || S==TEXT("RETURNING");G.CanResume|=S==TEXT("PAUSED");
            G.CanAbort|=S!=TEXT("SCHEDULED");
        }
    }
    for(auto& G:Result){G.Members.Sort();G.Progress/=FMath::Max(1,G.Executions.Num());
        TSet<FString> States;for(const auto& E:G.Executions)if(!G.ActiveCount || ExecutionUI::Active(E))States.Add(Field(E,TEXT("state")));
        G.State=States.Num()==1?*States.CreateConstIterator():TEXT("Mixed");
    }
    if(ActiveOnly)Result.RemoveAll([](const FGroup& G){return G.ActiveCount==0;});
    Result.Sort([](const FGroup& A,const FGroup& B){return A.CreatedAt==B.CreatedAt?A.Id<B.Id:A.CreatedAt>B.CreatedAt;});return Result;
}
FGroup Find(const TSharedPtr<FJsonObject>& State,const FString& Id) {
    for(const auto& G:Groups(State,false))if(G.Id==Id)return G;return {};
}
TSharedPtr<FJsonObject> Representative(const FGroup& Group,FName Action) {
    for(const auto& E:Group.Executions){if(!ExecutionUI::Active(E))continue;const auto S=Field(E,TEXT("state"));
        if(Action.IsNone() || (Action==TEXT("pause") && (S==TEXT("EXECUTING") || S==TEXT("RETURNING")))
            || (Action==TEXT("resume") && S==TEXT("PAUSED")) || (Action==TEXT("abort") && S!=TEXT("SCHEDULED")))return E;
    }
    return Action.IsNone() && !Group.Executions.IsEmpty()?Group.Executions[0]:nullptr;
}
}

void UActiveExecutionControlsWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();auto* Border=WidgetTree->ConstructWidget<UBorder>();CommandTheme::Panel(Border);Border->SetPadding(FMargin(10,6));WidgetTree->RootWidget=Border;
    auto* Content=WidgetTree->ConstructWidget<UVerticalBox>();Border->SetContent(Content);
    Label(WidgetTree,Content,T(TEXT("ExecutionControls.Title")),16);
    auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();Content->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Rows=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Rows);
    Result=Label(WidgetTree,Content,FText::GetEmpty(),12);Refresh();
}
void UActiveExecutionControlsWidget::RebuildRows() {
    Rows->ClearChildren();Labels.Empty();PauseButtons.Empty();ResumeButtons.Empty();AbortButtons.Empty();ConfirmButtons.Empty();CancelButtons.Empty();ActionTargets.Empty();
    for(int32 I=0;I<GroupIds.Num();++I){auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,3));
        const int32 ActionId=++NextActionId;ActionTargets.Add(ActionId,GroupIds[I]);
        auto* Text=Label(WidgetTree,Row,FText::GetEmpty(),13);CastChecked<UHorizontalBoxSlot>(Text->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Labels.Add(Text);
        auto Add=[&](const TCHAR* Name,const TCHAR* Key,TArray<TObjectPtr<UCommandActionButton>>& Buttons){auto* B=Button(WidgetTree,Row,Name,Key,ActionId);B->OnAction.AddDynamic(this,&UActiveExecutionControlsWidget::Action);CastChecked<UHorizontalBoxSlot>(B->Slot)->SetPadding(FMargin(3,0));Buttons.Add(B);};
        Add(TEXT("pause"),TEXT("Execution.Pause"),PauseButtons);Add(TEXT("resume"),TEXT("Execution.Resume"),ResumeButtons);
        Add(TEXT("abort"),TEXT("Execution.Abort"),AbortButtons);Add(TEXT("abort_confirm"),TEXT("ExecutionControls.ConfirmAbort"),ConfirmButtons);Add(TEXT("abort_cancel"),TEXT("Common.Cancel"),CancelButtons);
    }
}
void UActiveExecutionControlsWidget::Refresh() {
    if(!Rows)return;if(!LastResult.IsEmpty() && FPlatformTime::Seconds()>=LastResultUntil){LastResult=FText::GetEmpty();bLastFailed=false;}
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();const auto Groups=ExecutionControls::Groups(Sync->GetPlans());TArray<FString> Ids;
    for(const auto& G:Groups)Ids.Add(G.Id);if(Ids!=GroupIds){GroupIds=Ids;RebuildRows();}
    for(auto It=AbortConfirmGroups.CreateIterator();It;++It)if(!Ids.Contains(*It))It.RemoveCurrent();
    const bool Online=Sync->IsReady() && Sync->IsHydrated();
    for(int32 I=0;I<Groups.Num();++I){const auto& G=Groups[I];const bool Pending=PendingGroups.Contains(G.Id),Confirm=AbortConfirmGroups.Contains(G.Id);
        const auto State=T(*(TEXT("Execution.")+G.State));
        const auto GroupText=FText::Format(T(TEXT("ExecutionControls.Group")),G.PlanName.IsEmpty()?T(TEXT("Group.Title")):ProductText::Source(G.PlanName),G.MissionName.IsEmpty()?T(TEXT("Group.Title")):ProductText::Source(G.MissionName),User(FString::Join(G.Members,TEXT(", "))),State,FText::AsPercent(G.Progress),User(G.Id));
        Labels[I]->SetText(Pending?FText::Format(User(TEXT("{0} · {1}")),GroupText,T(TEXT("Common.Waiting"))):GroupText);
        auto Set=[&](UCommandActionButton* B,bool Visible,bool Enabled){B->SetVisibility(Visible?ESlateVisibility::Visible:ESlateVisibility::Collapsed);B->SetIsEnabled(Online && !Pending && Enabled);};
        Set(PauseButtons[I],!Confirm && G.CanPause,G.CanPause);Set(ResumeButtons[I],!Confirm && G.CanResume,G.CanResume);Set(AbortButtons[I],!Confirm && G.CanAbort,G.CanAbort);
        Set(ConfirmButtons[I],Confirm,G.CanAbort);Set(CancelButtons[I],Confirm,true);CancelButtons[I]->SetIsEnabled(!Pending);
        if(Pending)Labels[I]->SetColorAndOpacity(CommandTheme::Warning);else Labels[I]->SetColorAndOpacity(Online?CommandTheme::PrimaryText:CommandTheme::SecondaryText);
    }
    Result->SetText(!Online?T(TEXT("Errors.SYNC_OFFLINE")):!PendingGroups.IsEmpty()?T(TEXT("Common.Waiting")):LastResult);
    Result->SetColorAndOpacity(!Online || bLastFailed?CommandTheme::Warning:CommandTheme::SecondaryText);
    SetVisibility(GroupIds.IsEmpty() && PendingGroups.IsEmpty() && LastResult.IsEmpty()?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
}
void UActiveExecutionControlsWidget::Action(FName Name,int32 Index) {
    // A row removed during an ACK cannot retarget an already queued click to its replacement.
    const auto* Target=ActionTargets.Find(Index);if(!Target)return;const FString Id=*Target;if(PendingGroups.Contains(Id))return;
    if(Name==TEXT("abort_cancel")){AbortConfirmGroups.Remove(Id);Refresh();return;}
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();if(!Sync->IsReady() || !Sync->IsHydrated()){bLastFailed=true;LastResult=T(TEXT("Errors.SYNC_OFFLINE"));LastResultUntil=FPlatformTime::Seconds()+8.;Refresh();return;}
    const auto Group=ExecutionControls::Find(Sync->GetPlans(),Id);
    if(Name==TEXT("abort")){if(Group.CanAbort)AbortConfirmGroups.Add(Id);Refresh();return;}
    if(Name==TEXT("abort_confirm")){if(!AbortConfirmGroups.Remove(Id))return;Name=TEXT("abort");}
    if(Name!=TEXT("pause") && Name!=TEXT("resume") && Name!=TEXT("abort"))return;
    const auto E=ExecutionControls::Representative(Group,Name);if(!E){bLastFailed=true;LastResult=T(TEXT("Errors.EXECUTION_STATE_INVALID"));LastResultUntil=FPlatformTime::Seconds()+8.;Refresh();return;}
    auto Request=MakeShared<FJsonObject>();Request->SetStringField(TEXT("request_id"),FGuid::NewGuid().ToString());Request->SetStringField(TEXT("action"),TEXT("execution_")+Name.ToString());
    Request->SetStringField(TEXT("execution_id"),Field(E,TEXT("execution_id")));Request->SetNumberField(TEXT("control_version"),E->GetNumberField(TEXT("control_version")));
    const auto TargetName=FText::Format(User(TEXT("{0} · {1}")),Group.PlanName.IsEmpty()?T(TEXT("Group.Title")):ProductText::Source(Group.PlanName),User(FString::Join(Group.Members,TEXT(", "))));
    PendingGroups.Add(Id);bLastFailed=false;LastResult=FText::GetEmpty();const auto Weak=TWeakObjectPtr<UActiveExecutionControlsWidget>(this);
    if(!Sync->SubmitPlan(Request,[Weak,Id,TargetName](TSharedPtr<FJsonObject> Reply){if(!Weak.IsValid())return;Weak->PendingGroups.Remove(Id);const auto Code=PlanUI::Error(Reply);
        Weak->bLastFailed=!Code.IsEmpty();Weak->LastResultUntil=FPlatformTime::Seconds()+8.;Weak->LastResult=FText::Format(T(TEXT("ExecutionControls.Result")),TargetName,Code.IsEmpty()?T(TEXT("Group.Accepted")):T(*(TEXT("Errors.")+Code)));Weak->Refresh();
    })){PendingGroups.Remove(Id);bLastFailed=true;LastResult=T(TEXT("Errors.SYNC_OFFLINE"));LastResultUntil=FPlatformTime::Seconds()+8.;}Refresh();
}
