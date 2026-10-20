#include "Command/CommandShellWidget.h"
#include "Command/CommandActionButton.h"
#include "Command/CommandCenterPanel.h"
#include "Command/FigmaCommandStyle.h"
#include "Shared/ActiveExecutionControlsWidget.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Shared/SecurityPlanWorkspaceWidget.h"
#include "Shared/ExecutionPresentation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

void UCommandShellWidget::RefreshRunningTasks()
{
    if(!RunningTaskRows)return;
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    const auto Groups=ExecutionControls::Groups(Sync->GetPlans());
    TArray<FString> Ids;for(const auto& Group:Groups)Ids.Add(Group.Id);
    const auto Language=PlanUI::Field(Sync->GetUIPreferences(),TEXT("language"));
    RunningTaskTitle->SetText(FText::Format(ProductText::Get(TEXT("Overview.ActiveTasks")),FText::AsNumber(Groups.Num())));
    RunningTaskEmpty->SetVisibility(Groups.IsEmpty()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    OverviewSummarySize->SetHeightOverride(Groups.IsEmpty()?70:40+48*FMath::Min(Groups.Num(),3));
    if(Ids!=RunningGroupIds || RunningTaskLanguage!=Language){
        RunningGroupIds=Ids;RunningTaskLanguage=Language;
        RunningTaskRows->ClearChildren();RunningTaskNames.Empty();RunningTaskDetails.Empty();RunningTaskButtons.Empty();RunningTaskTargets.Empty();
        for(const auto& Id:Ids){
            auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();
            auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetHeightOverride(48);Size->SetContent(Row);RunningTaskRows->AddChild(Size);
            auto* Texts=WidgetTree->ConstructWidget<UVerticalBox>();
            auto* TextSlot=Row->AddChildToHorizontalBox(Texts);TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));TextSlot->SetVerticalAlignment(VAlign_Center);
            for(int32 Line=0;Line<2;++Line){
                auto* Text=WidgetTree->ConstructWidget<UTextBlock>();FigmaUI::Text(Text,Line?12:14,Line?FigmaUI::Muted:FigmaUI::PrimaryText,!Line);
                Text->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);Text->SetClipping(EWidgetClipping::ClipToBounds);Texts->AddChild(Text);
                (Line?RunningTaskDetails:RunningTaskNames).Add(Text);
            }
            auto* Open=WidgetTree->ConstructWidget<UCommandActionButton>();const int32 Target=++NextRunningTaskTarget;
            RunningTaskTargets.Add(Target,Id);Open->Configure(TEXT("open_task"),Target);Open->OnAction.AddDynamic(this,&UCommandShellWidget::OpenRunningTask);FigmaUI::Button(Open,false,true);
            auto* Caption=WidgetTree->ConstructWidget<UTextBlock>();Caption->SetText(ProductText::Get(TEXT("Overview.ViewTask")));FigmaUI::Text(Caption,12);Open->SetContent(Caption);
            auto* OpenSlot=Row->AddChildToHorizontalBox(Open);OpenSlot->SetVerticalAlignment(VAlign_Center);OpenSlot->SetPadding(FMargin(8,0,0,0));RunningTaskButtons.Add(Open);
        }
    }
    for(int32 I=0;I<Groups.Num();++I){
        const auto& Group=Groups[I];const auto Representative=ExecutionControls::Representative(Group);
        const bool Mission=!PlanUI::Field(Representative,TEXT("plan_id")).IsEmpty() && !PlanUI::Field(Representative,TEXT("mission_id")).IsEmpty();
        const FText Name=Mission?PlanUI::User(Group.MissionName):ProductText::Get(TEXT("Overview.FreeMove"));
        RunningTaskNames[I]->SetText(FText::Format(ProductText::Get(TEXT("Overview.TaskState")),Name,ProductText::Get(TEXT("Execution.")+Group.State),FText::AsPercent(Group.Progress)));
        const FText Owner=Mission?PlanUI::User(Group.PlanName):ProductText::Get(TEXT("Overview.FreeMove"));
        RunningTaskDetails[I]->SetText(FText::Format(ProductText::Get(TEXT("Overview.TaskMembers")),Owner,PlanUI::User(FString::Join(Group.Members,TEXT(", ")))));
        RunningTaskNames[I]->SetToolTipText(RunningTaskNames[I]->GetText());RunningTaskDetails[I]->SetToolTipText(RunningTaskDetails[I]->GetText());
        RunningTaskButtons[I]->SetVisibility(Mission?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
        RunningTaskButtons[I]->SetIsEnabled(Sync->IsReady() && Sync->IsHydrated() && CenterPanel && CenterPanel->GetPlanWorkspace() && !CenterPanel->GetPlanWorkspace()->IsMissionNavigationBlocked());
    }
}

void UCommandShellWidget::OpenRunningTask(FName Action,int32 Index)
{
    if(Action!=TEXT("open_task") || !CenterPanel)return;
    auto* Workspace=CenterPanel->GetPlanWorkspace();if(!Workspace || Workspace->IsMissionNavigationBlocked())return;
    const auto* Target=RunningTaskTargets.Find(Index);if(!Target)return;
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    const auto Group=ExecutionControls::Find(Sync->GetPlans(),*Target);
    if(!Group.ActiveCount)return; // A completed or replaced row cannot open a different task.
    const auto Execution=ExecutionControls::Representative(Group);
    const FString PlanId=PlanUI::Field(Execution,TEXT("plan_id")),MissionId=PlanUI::Field(Execution,TEXT("mission_id"));
    if(PlanId.IsEmpty() || MissionId.IsEmpty())return;
    Workspace->OpenMissionExecution(PlanId,MissionId);
    Navigate(TEXT("navigate"),1);
}
