#include "Shared/SecurityPlanWorkspaceWidget.h"
#include "Shared/PlanWidgetSupport.h"
#include "Shared/ExecutionPresentation.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif
using namespace PlanUI;
namespace {
bool FocusMapWindow(){
#if PLATFORM_WINDOWS
    struct FMatch { HWND Window=nullptr;int Count=0; } Match;
    ::EnumWindows([](HWND Window,LPARAM Data)->BOOL {wchar_t Title[128];::GetWindowTextW(Window,Title,128);
        if(::IsWindowVisible(Window) && FCStringWide::Strcmp(Title,L"Drone Security | Map")==0){auto* M=reinterpret_cast<FMatch*>(Data);M->Window=Window;++M->Count;}return 1;},reinterpret_cast<LPARAM>(&Match));
    if(Match.Count==1){if(::IsIconic(Match.Window))::ShowWindow(Match.Window,SW_RESTORE);return ::SetForegroundWindow(Match.Window)!=0;}
#endif
    return false;
}
}

void UPlanMissionListWidget::NativeOnInitialized(){Super::NativeOnInitialized();Content=WidgetTree->ConstructWidget<UVerticalBox>();WidgetTree->RootWidget=Content;}
void UPlanMissionListWidget::Refresh(const TSharedPtr<FJsonObject>& State,const TSharedPtr<FJsonObject>& Plan){
    const int64 NewVersion=State?State->GetNumberField(TEXT("version")):-1;const auto NewPlan=Field(Plan,TEXT("id"));
    if(NewVersion==Version && NewPlan==PlanId)return;Version=NewVersion;PlanId=NewPlan;Content->ClearChildren();MissionIds.Empty();
    const TArray<TSharedPtr<FJsonValue>>* Ids;if(!Plan || !Plan->TryGetArrayField(TEXT("mission_ids"),Ids) || Ids->IsEmpty()){Label(WidgetTree,Content,T(TEXT("Plan.NoMissions")));return;}
    for(const auto& V:*Ids){const auto M=Find(State,TEXT("missions"),V->AsString());const auto R=Find(State,TEXT("paths"),Field(M,TEXT("route_id")));const TArray<TSharedPtr<FJsonValue>>* Points;
        const int Count=R && R->TryGetArrayField(TEXT("waypoints"),Points)?Points->Num():0;
        const int Index=MissionIds.Add(V->AsString());const auto UAV=Field(M,TEXT("assigned_uav_id"));
        Label(WidgetTree,Content,FText::Format(T(TEXT("Plan.MissionSummary")),User(Field(M,TEXT("name"))),UAV.IsEmpty()?T(TEXT("Common.None")):User(UAV),FText::AsNumber(Count)));
        auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Content->AddChild(Row);
        auto Add=[&](const TCHAR* A,const TCHAR* K){Button(WidgetTree,Row,A,K,Index)->OnAction.AddDynamic(this,&UPlanMissionListWidget::Action);};
        Add(TEXT("select_mission"),TEXT("Plan.Mission"));

        if(!UAV.IsEmpty())Add(TEXT("video_mission"),TEXT("Video.Open"));
        if(Count<2)Label(WidgetTree,Content,T(TEXT("Errors.ROUTE_TOO_SHORT")));
    }
}
void UPlanReviewWidget::NativeOnInitialized(){Super::NativeOnInitialized();auto* V=WidgetTree->ConstructWidget<UVerticalBox>();WidgetTree->RootWidget=V;Summary=Label(WidgetTree,V,FText::GetEmpty());}
void UPlanReviewWidget::Refresh(const TSharedPtr<FJsonObject>& State,const TSharedPtr<FJsonObject>& Plan){
    if(!Plan){Summary->SetText(T(TEXT("Plan.NoPlan")));return;}
    TArray<FText> Lines{User(Field(Plan,TEXT("name"))),User(Field(Plan,TEXT("description")))};
    const TArray<TSharedPtr<FJsonValue>>* Ids;if(Plan->TryGetArrayField(TEXT("mission_ids"),Ids))for(const auto& V:*Ids){
        const auto M=Find(State,TEXT("missions"),V->AsString());const auto R=Find(State,TEXT("paths"),Field(M,TEXT("route_id")));const TArray<TSharedPtr<FJsonValue>>* Points;int Count=0;double Speed=0,Wait=0,Distance=0;bool Loop=false;
        if(R){R->TryGetBoolField(TEXT("bClosedLoop"),Loop);if(R->TryGetArrayField(TEXT("waypoints"),Points)){Count=Points->Num();for(const auto& P:*Points){Speed=FMath::Max(Speed,P->AsObject()->GetNumberField(TEXT("segmentSpeed")));Wait+=P->AsObject()->GetNumberField(TEXT("waitTime"));}}}
        Lines.Add(FText::Format(T(TEXT("Plan.MissionSummary")),User(Field(M,TEXT("name"))),User(Field(M,TEXT("assigned_uav_id"))),FText::AsNumber(Count)));
        if(R && R->TryGetArrayField(TEXT("waypoints"),Points))for(int I=1;I<Points->Num()+(Loop && Points->Num()>2?1:0);++I){const auto A=(*Points)[I-1]->AsObject(),B=(*Points)[I%Points->Num()]->AsObject();const double Lat=FMath::DegreesToRadians((A->GetNumberField(TEXT("latitude"))+B->GetNumberField(TEXT("latitude")))*.5);const double Y=FMath::DegreesToRadians(A->GetNumberField(TEXT("latitude"))-B->GetNumberField(TEXT("latitude")))*6371000.,X=FMath::DegreesToRadians(A->GetNumberField(TEXT("longitude"))-B->GetNumberField(TEXT("longitude")))*6371000.*FMath::Cos(Lat);Distance+=FMath::Sqrt(X*X+Y*Y);}
        Lines.Add(FText::Format(T(TEXT("Workflow.ReviewDistance")),FText::AsNumber(Distance)));
        Lines.Add(ProductText::Get(TEXT("Plan.")+Field(M,TEXT("status"))));

    }
    Lines.Add(T(TEXT("Workflow.CheckInformation")));
    Lines.Add(T(TEXT("Plan.Blocking")));const auto Validation=Object(Plan,TEXT("validation"));const TArray<TSharedPtr<FJsonValue>>* Issues;
    if(Validation && Validation->TryGetArrayField(TEXT("issues"),Issues))for(const auto& V:*Issues){const auto I=V->AsObject();Lines.Add(FText::Format(FText::AsCultureInvariant(TEXT("{0}: {1}")),User(Field(Find(State,TEXT("missions"),Field(I,TEXT("mission_id"))),TEXT("name"))),ProductText::Get(TEXT("Errors.")+Field(I,TEXT("code")))));}
    Lines.Add(T(TEXT("Plan.Warnings")));Lines.Add(T(TEXT("Plan.ReviewNotice")));if(Field(Plan,TEXT("status"))==TEXT("DEPLOYED") && !ExecutionUI::Latest(State,Field(Plan,TEXT("id"))))Lines.Add(T(TEXT("Workflow.DeploymentNotice")));
    const auto Deployment=Find(State,TEXT("deployments"),Field(Plan,TEXT("deployment_id")));
    if(Deployment){Lines.Add(T(TEXT("Plan.Deployment")));Lines.Add(User(Field(Deployment,TEXT("id"))));Lines.Add(FText::AsDateTime(FDateTime::FromUnixTimestamp(Deployment->GetNumberField(TEXT("deployed_at")))));}
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    Lines.Add(FText::Format(T(TEXT("Workflow.Readiness")),ProductText::Get(Sync->IsReady()?TEXT("Stage1.Online"):TEXT("Stage1.Offline")),ProductText::Get(Sync->IsRoleOnline(TEXT("Map"))?TEXT("Stage1.Online"):TEXT("Stage1.Offline"))));
    Summary->SetText(FText::Join(FText::AsCultureInvariant(TEXT("\n")),Lines));
}
void USecurityPlanWorkspaceWidget::NativeOnInitialized(){
    Super::NativeOnInitialized();Content=Root(WidgetTree);
    auto Add=[&](UPanelWidget* Parent,const TCHAR* A,const TCHAR* K){auto* B=Button(WidgetTree,Parent,A,K);B->OnAction.AddDynamic(this,&USecurityPlanWorkspaceWidget::Action);CommandTheme::Button(B);Actions.Add(A,B);return B;};
    ListPage=WidgetTree->ConstructWidget<UVerticalBox>();Content->AddChild(ListPage);
    Label(WidgetTree,ListPage,T(TEXT("Nav.Plans")),26);Add(ListPage,TEXT("new_plan"),TEXT("Workflow.NewPlan"));
    Add(ListPage,TEXT("confirm_delete"),TEXT("Workflow.DeleteDraft"));Add(ListPage,TEXT("cancel_delete"),TEXT("Common.Cancel"));
    PlanCards=WidgetTree->ConstructWidget<UVerticalBox>();ListPage->AddChild(PlanCards);
    Workspace=WidgetTree->ConstructWidget<UVerticalBox>();Content->AddChild(Workspace);
    Add(Workspace,TEXT("back_list"),TEXT("Workflow.BackPlans"));
    WorkspaceTitle=Label(WidgetTree,Workspace,FText::GetEmpty(),26);
    Summary=Label(WidgetTree,Workspace,FText::GetEmpty(),14);
    Add(Workspace,TEXT("request_map_plan_move"),TEXT("Geometry.MovePlan"));
    GeometryStatus=Label(WidgetTree,Workspace,FText::GetEmpty(),16);
    Steps=Label(WidgetTree,Workspace,FText::GetEmpty(),16);
    auto Page=[&](){auto* V=WidgetTree->ConstructWidget<UVerticalBox>();Workspace->AddChild(V);return V;};
    BasicPage=Page();TaskPage=Page();RoutePage=Page();ReviewPage=Page();
    auto Input=[&](UPanelWidget* Parent,const TCHAR* Key){Label(WidgetTree,Parent,T(Key),15);auto* E=WidgetTree->ConstructWidget<UEditableTextBox>();CommandTheme::Input(E);Parent->AddChild(E);return E;};
    BasicHeading=Label(WidgetTree,BasicPage,T(TEXT("Workflow.NewPlan")),24);
    PlanName=Input(BasicPage,TEXT("Plan.Name"));Description=Input(BasicPage,TEXT("Plan.Description"));
    Add(BasicPage,TEXT("create_plan"),TEXT("Workflow.CreateContinue"));Add(BasicPage,TEXT("update_plan"),TEXT("Common.Save"));
    Add(BasicPage,TEXT("next_task"),TEXT("Workflow.NextTask"));Add(BasicPage,TEXT("cancel_new"),TEXT("Common.Cancel"));
    Label(WidgetTree,TaskPage,T(TEXT("Workflow.TaskConfig")),20);
    MissionList=CreateWidget<UPlanMissionListWidget>(GetOwningPlayer());TaskPage->AddChild(MissionList);MissionList->OnAction.AddDynamic(this,&USecurityPlanWorkspaceWidget::MissionAction);
    MissionName=Input(TaskPage,TEXT("Plan.MissionName"));
    Label(WidgetTree,TaskPage,T(TEXT("Workflow.AssignedUAV")),15);
    UAVs=WidgetTree->ConstructWidget<UComboBoxString>();UAVs->OnGenerateWidgetEvent.BindDynamic(this,&USecurityPlanWorkspaceWidget::UAVOption);UAVs->OnSelectionChanged.AddDynamic(this,&USecurityPlanWorkspaceWidget::UAVSelected);
    auto* UAVHeight=WidgetTree->ConstructWidget<USizeBox>();UAVHeight->SetMinDesiredHeight(40);UAVHeight->SetContent(UAVs);TaskPage->AddChild(UAVHeight);
    auto ComboStyle=UAVs->GetWidgetStyle();auto ComboButton=ComboStyle.ComboButtonStyle;FButtonStyle ButtonStyle;ButtonStyle.SetNormal(FSlateRoundedBoxBrush(CommandTheme::Elevated,5));ButtonStyle.SetHovered(FSlateRoundedBoxBrush(CommandTheme::Hover,5));ButtonStyle.SetPressed(FSlateRoundedBoxBrush(CommandTheme::Selected,5));ComboButton.SetButtonStyle(ButtonStyle);ComboStyle.SetComboButtonStyle(ComboButton);UAVs->SetWidgetStyle(ComboStyle);
    Label(WidgetTree,TaskPage,T(TEXT("Workflow.Patrol")),14);
    auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();TaskPage->AddChild(Row);
    Add(Row,TEXT("assign"),TEXT("Plan.Assign"));Add(Row,TEXT("rename_mission"),TEXT("Plan.Rename"));Add(Row,TEXT("add_mission"),TEXT("Plan.AddMission"));
    Add(TaskPage,TEXT("request_map_route_edit"),TEXT("Workflow.EditMap"));
    Add(TaskPage,TEXT("basic_step"),TEXT("Workflow.BasicInfo"));
    Label(WidgetTree,RoutePage,T(TEXT("Workflow.RouteEditing")),20);RouteStatus=Label(WidgetTree,RoutePage,FText::GetEmpty(),16);
    Add(RoutePage,TEXT("retry_map"),TEXT("Workflow.ReturnMap"));
    ReviewHeading=Label(WidgetTree,ReviewPage,T(TEXT("Workflow.PreDeployReview")),20);
    Add(ReviewPage,TEXT("confirm_deployment"),TEXT("Execution.Deploy"));
    Add(ReviewPage,TEXT("execution_start_prompt"),TEXT("Execution.Start"));
    Add(ReviewPage,TEXT("execution_open"),TEXT("Execution.Open"));
    Review=CreateWidget<UPlanReviewWidget>(GetOwningPlayer());ReviewPage->AddChild(Review);
    Add(ReviewPage,TEXT("task_step"),TEXT("Workflow.BackEdit"));
    Add(ReviewPage,TEXT("copy_plan"),TEXT("Workflow.NewVersion"));
    ExecutionHistory=Label(WidgetTree,ReviewPage,FText::GetEmpty(),14);
    ExecutionPage=Page();Label(WidgetTree,ExecutionPage,T(TEXT("Execution.Simulation")),18);
    ExecutionSummary=Label(WidgetTree,ExecutionPage,FText::GetEmpty(),22);
    ExecutionProgress=WidgetTree->ConstructWidget<UProgressBar>();ExecutionPage->AddChild(ExecutionProgress);
    ExecutionPrompt=Label(WidgetTree,ExecutionPage,FText::GetEmpty(),16);
    for(const auto& Item:TArray<TPair<FString,FString>>{{TEXT("confirm"),TEXT("Confirm")},{TEXT("pause"),TEXT("Pause")},{TEXT("resume"),TEXT("Resume")},{TEXT("return_prompt"),TEXT("Return")},{TEXT("abort_prompt"),TEXT("Abort")},{TEXT("cancel"),TEXT("Cancel")},{TEXT("details"),TEXT("History")},{TEXT("video"),TEXT("Video")}})
        Add(ExecutionPage,*(TEXT("execution_")+Item.Key),*(TEXT("Execution.")+Item.Value));
    // Keep the existing action bindings available to regression automation, hidden from the product flow.
    Plans=WidgetTree->ConstructWidget<UComboBoxString>();Plans->OnSelectionChanged.AddDynamic(this,&USecurityPlanWorkspaceWidget::PlanSelected);
    for(const TCHAR* A:{TEXT("unassign"),TEXT("delete_mission"),TEXT("validate"),TEXT("review_page"),TEXT("review"),TEXT("deploy_page"),TEXT("deploy"),TEXT("cancel")}){
        auto* B=Add(Content,A,TEXT("Common.Cancel"));B->SetVisibility(ESlateVisibility::Collapsed);
    }
    Result=Label(WidgetTree,Content,FText::GetEmpty(),14);
    WidgetTree->ForEachWidget([](UWidget* W){if(auto* Slot=Cast<UVerticalBoxSlot>(W->Slot))Slot->SetPadding(FMargin(0,6));});
    for(const TCHAR* Key:{TEXT("create_plan"),TEXT("new_plan"),TEXT("request_map_route_edit"),TEXT("confirm_deployment"),TEXT("execution_start_prompt"),TEXT("execution_confirm"),TEXT("execution_pause"),TEXT("execution_resume")})CommandTheme::Button(Actions[Key],true);
    CommandTheme::Text(Cast<UTextBlock>(Actions[TEXT("execution_return_prompt")]->GetContent()),14,CommandTheme::Warning);
    CommandTheme::Text(Cast<UTextBlock>(Actions[TEXT("execution_abort_prompt")]->GetContent()),14,FLinearColor(1,.3f,.3f));
    UWidget* ExistingRoot=WidgetTree->RootWidget.Get();auto* Overlay=WidgetTree->ConstructWidget<UOverlay>();WidgetTree->RootWidget=Overlay;auto* MainSlot=Overlay->AddChildToOverlay(ExistingRoot);MainSlot->SetHorizontalAlignment(HAlign_Fill);MainSlot->SetVerticalAlignment(VAlign_Fill);
    NewPlanDialog=WidgetTree->ConstructWidget<UBorder>();CommandTheme::Panel(NewPlanDialog,CommandTheme::Surface,CommandTheme::Cyan);NewPlanDialog->SetPadding(FMargin(24));
    auto* DialogWidth=WidgetTree->ConstructWidget<USizeBox>();DialogWidth->SetWidthOverride(600);DialogWidth->SetContent(NewPlanDialog);
    auto* DialogSlot=Overlay->AddChildToOverlay(DialogWidth);DialogSlot->SetHorizontalAlignment(HAlign_Center);DialogSlot->SetVerticalAlignment(VAlign_Center);
    Refresh();
}
void USecurityPlanWorkspaceWidget::Refresh(){
    if(!Content)return;auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();const auto State=Sync->GetPlans();
    PlanId=Field(Sync->GetContext(),TEXT("active_security_plan_id"));MissionId=Field(Sync->GetContext(),TEXT("active_mission_id"));const auto P=Find(State,TEXT("plans"),PlanId),M=Find(State,TEXT("missions"),MissionId);
    bRefreshing=true;TArray<FString> Ids,Labels;const auto All=Object(State,TEXT("plans"));if(All)for(const auto& E:All->Values){Ids.Add(FString(E.Key.ToView()));Labels.Add(Field(E.Value->AsObject(),TEXT("name")));}
    if(Labels!=PlanLabels){Plans->ClearOptions();for(const auto& L:Labels)Plans->AddOption(L);PlanLabels=Labels;PlanIds=Ids;}
    const int Index=PlanIds.IndexOfByKey(PlanId);if(Index>=0)Plans->SetSelectedOption(PlanLabels[Index]);
    Ids.Empty();for(const auto& D:GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetFriendlyDroneDescriptors())Ids.Add(FString::Printf(TEXT("UAV-%02d"),D.DroneId));
    if(Ids!=UavIds){const auto Selected=UAVs->GetSelectedOption();UAVs->ClearOptions();for(const auto& ID:Ids)UAVs->AddOption(ID);UavIds=Ids;UAVs->SetSelectedOption(Selected);}
    bRefreshing=false;const FString Selection=PlanId+TEXT("/")+MissionId;
    if(Selection!=LoadedSelection && (PlanId.IsEmpty() || P)){LoadedSelection=Selection;bReview=false;bConfirm=false;PlanName->SetText(User(Field(P,TEXT("name"))));Description->SetText(User(Field(P,TEXT("description"))));MissionName->SetText(User(Field(M,TEXT("name"))));if(Field(M,TEXT("assigned_uav_id")).IsEmpty())UAVs->ClearSelection();else UAVs->SetSelectedOption(Field(M,TEXT("assigned_uav_id")));}
    const auto Status=Field(P,TEXT("status"));const bool Deployed=Status==TEXT("DEPLOYED"),Ready=Status==TEXT("READY"),Reviewed=PlanUI::Reviewed(P);
    if(P && ConfirmRevision!=P->GetNumberField(TEXT("content_revision")))bConfirm=false;
    PlanName->SetIsReadOnly(Deployed && !bCreating);Description->SetIsReadOnly(Deployed && !bCreating);MissionName->SetIsReadOnly(Deployed);UAVs->SetIsEnabled(!Deployed);
    for(const auto& A:Actions)A.Value->SetIsEnabled(!bPending && Sync->IsReady());
    for(const TCHAR* A:{TEXT("update_plan"),TEXT("add_mission"),TEXT("rename_mission"),TEXT("delete_mission"),TEXT("assign"),TEXT("unassign"),TEXT("request_map_route_edit")})Actions[A]->SetIsEnabled(!bPending && Sync->IsReady() && P && !Deployed);
    const bool MovingBlocked=ExecutionUI::Latest(State,PlanId,true).IsValid();
    Actions[TEXT("request_map_plan_move")]->SetIsEnabled(!bPending && Sync->IsReady() && P && M && !MovingBlocked);
    GeometryStatus->SetText(MovingBlocked?T(TEXT("Errors.PLAN_EXECUTING")):FText::GetEmpty());GeometryStatus->SetVisibility(MovingBlocked?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    Actions[TEXT("create_plan")]->SetIsEnabled(!bPending && Sync->IsReady() && !PlanName->GetText().IsEmpty());
    auto Show=[&](const TCHAR* A,bool B){Actions[A]->SetVisibility(B?ESlateVisibility::Visible:ESlateVisibility::Collapsed);};
    Show(TEXT("confirm_delete"),!PendingDeletePlan.IsEmpty());Show(TEXT("cancel_delete"),!PendingDeletePlan.IsEmpty());Show(TEXT("copy_plan"),Deployed);Show(TEXT("deploy_page"),false);
    ReviewHeading->SetText(T(Deployed?TEXT("Workflow.DeploymentSuccess"):TEXT("Workflow.PreDeployReview")));
    Review->Refresh(State,P);MissionList->Refresh(State,P);
    if(!bInitializedSelection && Sync->IsHydrated()){bInitializedSelection=true;bList=!P;}
    const FString Workflow=Field(P,TEXT("workflow_step"));
    if(LastWorkflow!=PlanId+Workflow){if(Workflow==TEXT("PreDeployReview"))bList=false;LastWorkflow=PlanId+Workflow;CurrentStep=Workflow==TEXT("BasicInfo")?0:Workflow==TEXT("RouteEditing")?2:Workflow==TEXT("PreDeployReview")?3:1;}
    if(Deployed)CurrentStep=4;
    if(bCreating)CurrentStep=0;
    auto Visible=[](UWidget* W,bool Yes){W->SetVisibility(Yes?ESlateVisibility::Visible:ESlateVisibility::Collapsed);};
    Content->SetIsEnabled(!bCreating);
    if(bCreating && BasicPage->GetParent()!=NewPlanDialog){BasicPage->RemoveFromParent();NewPlanDialog->SetContent(BasicPage);}
    if(!bCreating && BasicPage->GetParent()==NewPlanDialog){NewPlanDialog->SetContent(nullptr);Workspace->AddChild(BasicPage);}
    Visible(NewPlanDialog,bCreating);Visible(BasicHeading,true);BasicHeading->SetText(bCreating?T(TEXT("Workflow.NewPlan")):P?FText::Format(T(TEXT("Workflow.CreatedAt")),FText::AsDateTime(FDateTime::FromUnixTimestamp(P->GetNumberField(TEXT("created_at"))))):FText::GetEmpty());
    Visible(ListPage,bList || bCreating);Visible(Workspace,!bList && !bCreating);Visible(BasicPage,CurrentStep==0);Visible(TaskPage,CurrentStep==1);Visible(RoutePage,CurrentStep==2);Visible(ReviewPage,CurrentStep>=3);
    Show(TEXT("assign"),false);Show(TEXT("next_task"),!bCreating);Show(TEXT("create_plan"),bCreating);Show(TEXT("cancel_new"),bCreating);Show(TEXT("update_plan"),!bCreating);Show(TEXT("task_step"),!Deployed && !bCreating);
    WorkspaceTitle->SetText(bCreating?T(TEXT("Workflow.NewPlan")):User(Field(P,TEXT("name"))));
    TArray<FText> StepNames;const TCHAR* Keys[]={TEXT("Workflow.StepInfo"),TEXT("Workflow.StepTask"),TEXT("Workflow.StepRoute"),TEXT("Workflow.StepCheck"),TEXT("Workflow.StepDeploy")};
    for(int I=0;I<5;++I)StepNames.Add(FText::Format(T(I==CurrentStep?TEXT("Workflow.ActiveStep"):TEXT("Workflow.Step")),FText::AsNumber(I+1),T(Keys[I])));
    Steps->SetText(FText::Join(User(TEXT("   /   ")),StepNames));
    Actions[TEXT("request_map_route_edit")]->SetIsEnabled(!bPending && Sync->IsReady() && P && !Deployed && !Field(M,TEXT("assigned_uav_id")).IsEmpty());
    const auto Validation=Object(P,TEXT("validation"));const TArray<TSharedPtr<FJsonValue>>* Issues=nullptr;
    const bool Complete=Validation && Validation->TryGetArrayField(TEXT("issues"),Issues) && Issues->IsEmpty();
    Show(TEXT("confirm_deployment"),!Deployed);Actions[TEXT("confirm_deployment")]->SetIsEnabled(!bPending && Sync->IsReady() && Sync->IsRoleOnline(TEXT("Map")) && Complete);
    const auto Route=Find(State,TEXT("paths"),Field(M,TEXT("route_id")));const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;
    const int Count=Route && Route->TryGetArrayField(TEXT("waypoints"),Points)?Points->Num():0;
    RouteStatus->SetText(FText::Format(T(Sync->IsRoleOnline(TEXT("Map"))?TEXT("Workflow.MapEditingStatus"):TEXT("Workflow.MapOfflineStatus")),User(Field(P,TEXT("name"))),User(Field(M,TEXT("name"))),User(Field(M,TEXT("assigned_uav_id"))),FText::AsNumber(Count)));
    const int64 Version=State?State->GetNumberField(TEXT("version")):-1;
    if(Version!=CardsVersion){CardsVersion=Version;PlanCards->ClearChildren();
        for(int I=0;I<PlanIds.Num();++I){const auto Card=Find(State,TEXT("plans"),PlanIds[I]);
            Label(WidgetTree,PlanCards,User(Field(Card,TEXT("name"))),20);
            Label(WidgetTree,PlanCards,FText::Format(T(TEXT("Workflow.PlanCard")),ProductText::Get(TEXT("Plan.")+Field(Card,TEXT("status"))),FText::AsNumber(Card->GetArrayField(TEXT("mission_ids")).Num()),FText::AsDateTime(FDateTime::FromUnixTimestamp(Card->GetNumberField(TEXT("updated_at"))))));
            auto* B=Button(WidgetTree,PlanCards,TEXT("open_card"),TEXT("Workflow.OpenPlan"),I);B->OnAction.AddDynamic(this,&USecurityPlanWorkspaceWidget::Action);
            auto* CopyButton=Button(WidgetTree,PlanCards,TEXT("copy_card"),Field(Card,TEXT("status"))==TEXT("DEPLOYED")?TEXT("Workflow.NewVersion"):TEXT("Workflow.Duplicate"),I);CopyButton->OnAction.AddDynamic(this,&USecurityPlanWorkspaceWidget::Action);
            if(Field(Card,TEXT("status"))==TEXT("DRAFT")){auto* DeleteButton=Button(WidgetTree,PlanCards,TEXT("delete_card"),TEXT("Workflow.DeleteDraft"),I);DeleteButton->OnAction.AddDynamic(this,&USecurityPlanWorkspaceWidget::Action);}
        }
    }
    TArray<FText> Lines{P?ProductText::Get(TEXT("Plan.")+Status):T(TEXT("Plan.NoPlan"))};const auto V=Object(P,TEXT("validation"));if(V)Lines.Add(FText::Format(T(TEXT("Plan.Counts")),FText::AsNumber(V->GetNumberField(TEXT("mission_count"))),FText::AsNumber(V->GetNumberField(TEXT("assigned_count"))),FText::AsNumber(V->GetNumberField(TEXT("routes_ready")))));
    if(Deployed && ExecutionUI::Latest(State,PlanId))Lines[0]=T(TEXT("Workflow.DeploymentSuccess"));
    Summary->SetText(FText::Join(User(TEXT("\n")),Lines));
    RefreshExecution(State,P);
    FString EditingMission;const auto Sessions=Object(State,TEXT("edit_sessions"));if(Sessions)for(const auto& E:Sessions->Values){const auto Session=E.Value->AsObject();if(Field(Session,TEXT("plan_id"))==PlanId){EditingMission=Field(Find(State,TEXT("missions"),Field(Session,TEXT("mission_id"))),TEXT("name"));break;}}
    if(!EditingMission.IsEmpty()){
        for(const TCHAR* A:{TEXT("review_page"),TEXT("review"),TEXT("deploy_page"),TEXT("deploy"),TEXT("confirm_deployment")})Actions[A]->SetIsEnabled(false);
        Result->SetText(FText::Format(T(TEXT("Plan.EditingBlock")),User(EditingMission)));return;
    }
    Result->SetText(bPending?T(TEXT("Common.Waiting")):ErrorCode.IsEmpty()?FText::GetEmpty():ProductText::Get(TEXT("Errors.")+ErrorCode));
}
void USecurityPlanWorkspaceWidget::Submit(const TSharedRef<FJsonObject>& R){
    if(bPending)return;bPending=true;ErrorCode.Empty();const auto Weak=TWeakObjectPtr<USecurityPlanWorkspaceWidget>(this);
    if(!GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->SubmitPlan(R,[Weak](TSharedPtr<FJsonObject> Reply){if(!Weak.IsValid())return;Weak->bPending=false;Weak->ErrorCode=PlanUI::Error(Reply);if(Weak->ErrorCode.IsEmpty()){Weak->bCreating=false;Weak->bList=Field(Weak->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetContext(),TEXT("active_security_plan_id")).IsEmpty();}Weak->Refresh();})){bPending=false;ErrorCode=TEXT("SYNC_OFFLINE");}
}
void USecurityPlanWorkspaceWidget::PlanSelected(FString Item,ESelectInfo::Type){if(bRefreshing)return;const int I=PlanLabels.IndexOfByKey(Item);if(I<0)return;auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("select"));R->SetStringField(TEXT("plan_id"),PlanIds[I]);const auto P=Find(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),TEXT("plans"),PlanIds[I]);if(P && !P->GetArrayField(TEXT("mission_ids")).IsEmpty())R->SetStringField(TEXT("mission_id"),P->GetArrayField(TEXT("mission_ids"))[0]->AsString());Submit(R);}
void USecurityPlanWorkspaceWidget::MissionAction(FName Name,int32 Index){
    if(!MissionList->MissionIds.IsValidIndex(Index))return;const auto ID=MissionList->MissionIds[Index];auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    if(Name==TEXT("video_mission")){Sync->OpenVideo(Field(Find(Sync->GetPlans(),TEXT("missions"),ID),TEXT("assigned_uav_id")));return;}
    auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),Name==TEXT("edit_mission")?TEXT("request_map_route_edit"):TEXT("select"));R->SetStringField(TEXT("plan_id"),PlanId);R->SetStringField(TEXT("mission_id"),ID);Submit(R);
}
void USecurityPlanWorkspaceWidget::Action(FName Name,int32 Index){
    if(Name.ToString().StartsWith(TEXT("execution_"))){ExecutionAction(Name);return;}
    if(Name==TEXT("back_list") || Name==TEXT("cancel_new")){bList=true;bCreating=false;Refresh();return;}
    if(Name==TEXT("new_plan")){bList=false;bCreating=true;PlanName->SetText(FText::GetEmpty());Description->SetText(FText::GetEmpty());Refresh();return;}
    if(Name==TEXT("basic_step") || Name==TEXT("task_step") || Name==TEXT("next_task")){
        auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("set_workflow_step"));R->SetStringField(TEXT("plan_id"),PlanId);R->SetStringField(TEXT("workflow_step"),Name==TEXT("basic_step")?TEXT("BasicInfo"):TEXT("TaskConfig"));Submit(R);return;
    }
    if(Name==TEXT("cancel_delete")){PendingDeletePlan.Empty();Refresh();return;}
    if(Name==TEXT("confirm_delete")){if(!PendingDeletePlan.IsEmpty()){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("delete_plan"));R->SetStringField(TEXT("plan_id"),PendingDeletePlan);PendingDeletePlan.Empty();Submit(R);}return;}
    if(Name==TEXT("delete_card")){if(PlanIds.IsValidIndex(Index)){PendingDeletePlan=PlanIds[Index];Refresh();}return;}
    if(Name==TEXT("open_card") || Name==TEXT("copy_card")){
        if(PlanIds.IsValidIndex(Index)){bList=false;bCreating=false;auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),Name==TEXT("open_card")?TEXT("open_plan"):TEXT("copy_plan"));R->SetStringField(TEXT("plan_id"),PlanIds[Index]);R->SetBoolField(TEXT("allow_draft"),true);
            const auto P=Find(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),TEXT("plans"),PlanIds[Index]);if(P && !P->GetArrayField(TEXT("mission_ids")).IsEmpty())R->SetStringField(TEXT("mission_id"),P->GetArrayField(TEXT("mission_ids"))[0]->AsString());Submit(R);}return;
    }
    if(Name==TEXT("confirm_deployment")){ConfirmDeployment();return;}
    if(Name==TEXT("retry_map")){
        auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
        if(S->IsRoleOnline(TEXT("Map"))){if(!FocusMapWindow())ErrorCode=TEXT("MAP_FOCUS_MANUAL");return;}
        Name=TEXT("request_map_route_edit");
    }
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();if(Name==TEXT("en") || Name==TEXT("zh-Hans")){Sync->SetLanguage(Name.ToString());return;}
    if(Name==TEXT("active_video")){Sync->OpenVideo(Field(Sync->GetContext(),TEXT("active_uav_id")));return;}
    if(Name==TEXT("cancel")){bReview=false;bConfirm=false;return;}if(Name==TEXT("review_page")){bReview=true;return;}
    if(Name==TEXT("deploy_page")){const auto P=Find(Sync->GetPlans(),TEXT("plans"),PlanId);if(P && Reviewed(P)){ConfirmRevision=P->GetNumberField(TEXT("content_revision"));bConfirm=true;}return;}
    auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),Name==TEXT("unassign")?TEXT("assign"):Name.ToString());R->SetStringField(TEXT("plan_id"),PlanId);R->SetStringField(TEXT("mission_id"),MissionId);
    if(Name==TEXT("copy_plan"))R->SetStringField(TEXT("name"),FText::Format(T(TEXT("Workflow.VersionName")),User(Field(Find(Sync->GetPlans(),TEXT("plans"),PlanId),TEXT("name")))).ToString());
    if(Name==TEXT("create_plan"))R->SetStringField(TEXT("default_mission_name"),T(TEXT("Workflow.DefaultTask")).ToString());
    if(Name==TEXT("create_plan") || Name==TEXT("update_plan")){R->SetStringField(TEXT("name"),PlanName->GetText().ToString());R->SetStringField(TEXT("description"),Description->GetText().ToString());}
    if(Name==TEXT("add_mission") || Name==TEXT("rename_mission"))R->SetStringField(TEXT("name"),MissionName->GetText().ToString());
    if(Name==TEXT("assign"))R->SetStringField(TEXT("assigned_uav_id"),UAVs->GetSelectedOption());
    if(Name==TEXT("deploy")){if(!bConfirm)return;bConfirm=false;}Submit(R);
}



void USecurityPlanWorkspaceWidget::ConfirmDeployment(){
    if(bPending)return;
    bPending=true;ErrorCode.Empty();
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    const auto Weak=TWeakObjectPtr<USecurityPlanWorkspaceWidget>(this);
    auto Check=MakeShared<FJsonObject>();Check->SetStringField(TEXT("action"),TEXT("validate"));Check->SetStringField(TEXT("plan_id"),PlanId);
    if(!Sync->SubmitPlan(Check,[Weak,Sync](TSharedPtr<FJsonObject> Reply){
        if(!Weak.IsValid())return;Weak->ErrorCode=PlanUI::Error(Reply);
        const auto P=Find(Sync->GetPlans(),TEXT("plans"),Weak->PlanId);
        if(!Weak->ErrorCode.IsEmpty() || Field(P,TEXT("status"))!=TEXT("READY")){Weak->bPending=false;Weak->Refresh();return;}
        auto ReviewRequest=MakeShared<FJsonObject>();ReviewRequest->SetStringField(TEXT("action"),TEXT("review"));ReviewRequest->SetStringField(TEXT("plan_id"),Weak->PlanId);
        if(!Sync->SubmitPlan(ReviewRequest,[Weak,Sync](TSharedPtr<FJsonObject> ReviewedReply){
            if(!Weak.IsValid())return;Weak->ErrorCode=PlanUI::Error(ReviewedReply);Weak->bPending=false;
            if(!Weak->ErrorCode.IsEmpty()){Weak->Refresh();return;}
            auto Deploy=MakeShared<FJsonObject>();Deploy->SetStringField(TEXT("action"),TEXT("deploy"));Deploy->SetStringField(TEXT("plan_id"),Weak->PlanId);Weak->Submit(Deploy);
        })){Weak->bPending=false;Weak->ErrorCode=TEXT("SYNC_OFFLINE");}
    })){bPending=false;ErrorCode=TEXT("SYNC_OFFLINE");}
}

void UPlanUAVOptionWidget::NativeOnInitialized(){Super::NativeOnInitialized();Caption=WidgetTree->ConstructWidget<UTextBlock>();CommandTheme::Text(Caption,16,CommandTheme::PrimaryText);WidgetTree->RootWidget=Caption;}
void UPlanUAVOptionWidget::SetCaption(const FString& Item){Caption->SetText(User(Item));}
UWidget* USecurityPlanWorkspaceWidget::UAVOption(FString Item){auto* Option=CreateWidget<UPlanUAVOptionWidget>(GetOwningPlayer());Option->SetCaption(Item);return Option;}
void USecurityPlanWorkspaceWidget::UAVSelected(FString,ESelectInfo::Type Type){if(!bRefreshing && Type!=ESelectInfo::Direct && !bPending)Action(TEXT("assign"),0);}

void USecurityPlanWorkspaceWidget::RefreshExecution(const TSharedPtr<FJsonObject>& State,const TSharedPtr<FJsonObject>& P){
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    auto E=ExecutionUI::Latest(State,PlanId);const auto LatestId=Field(E,TEXT("execution_id"));
    if(!LatestId.IsEmpty() && LatestId!=ExecutionId){ExecutionId=LatestId;bExecutionView=ExecutionUI::Active(E);ExecutionIntent.Empty();}
    auto Show=[&](const TCHAR* K,bool Yes){Actions[K]->SetVisibility(Yes?ESlateVisibility::Visible:ESlateVisibility::Collapsed);};
    const bool Deployed=Field(P,TEXT("status"))==TEXT("DEPLOYED");
    Show(TEXT("execution_start_prompt"),Deployed && !ExecutionUI::Active(E));Show(TEXT("execution_open"),E.IsValid());
    if(!Deployed){bExecutionView=false;ExecutionIntent.Empty();}
    const bool Visible=Deployed && bExecutionView && !bList && !bCreating;
    ExecutionPage->SetVisibility(Visible?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(Visible)ReviewPage->SetVisibility(ESlateVisibility::Collapsed);
    const auto StateName=Field(E,TEXT("state"));
    ExecutionSummary->SetText(ExecutionIntent==TEXT("start")?T(TEXT("Execution.StartConfirm")):ExecutionUI::Summary(E));ExecutionProgress->SetVisibility(ExecutionIntent.IsEmpty()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);ExecutionProgress->SetPercent(E?E->GetNumberField(TEXT("progress")):0);
    const bool Prompt=!ExecutionIntent.IsEmpty();
    Show(TEXT("execution_pause"),!Prompt && StateName==TEXT("EXECUTING"));Show(TEXT("execution_resume"),!Prompt && StateName==TEXT("PAUSED"));
    Show(TEXT("execution_return_prompt"),!Prompt && StateName==TEXT("EXECUTING"));Show(TEXT("execution_abort_prompt"),!Prompt && (StateName==TEXT("EXECUTING") || StateName==TEXT("PAUSED")));
    Show(TEXT("execution_confirm"),Prompt);Show(TEXT("execution_cancel"),Prompt);Show(TEXT("execution_details"),!Prompt);Show(TEXT("execution_video"),E.IsValid() && !Prompt);
    if(auto* Caption=Cast<UTextBlock>(Actions[TEXT("execution_confirm")]->GetContent()))Caption->SetText(T(ExecutionIntent==TEXT("start")?TEXT("Execution.ConfirmStart"):ExecutionIntent==TEXT("return")?TEXT("Execution.ConfirmReturn"):TEXT("Execution.ConfirmAbort")));
    TArray<FText> Lines;
    if(ExecutionIntent==TEXT("start")){
        const auto M=Find(State,TEXT("missions"),MissionId),R=Find(State,TEXT("paths"),Field(M,TEXT("route_id")));
        const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;const int Count=R && R->TryGetArrayField(TEXT("waypoints"),Points)?Points->Num():0;
        Lines.Add(T(TEXT("Execution.StartConfirm")));Lines.Add(User(Field(P,TEXT("name"))));Lines.Add(User(Field(M,TEXT("name"))));Lines.Add(User(Field(M,TEXT("assigned_uav_id"))));
        Lines.Add(FText::Format(T(TEXT("Map.Count")),FText::AsNumber(Count)));
        const auto Config=Object(State,TEXT("mock_execution"));const auto Fixture=Find(Config,TEXT("uavs"),Field(M,TEXT("assigned_uav_id")));
        auto From=Object(Fixture,TEXT("home_position"));const auto AllExecutions=Object(State,TEXT("executions"));double Latest=-1;
        if(AllExecutions)for(const auto& It:AllExecutions->Values){const auto Item=It.Value->AsObject();if(Field(Item,TEXT("uav_id"))==Field(M,TEXT("assigned_uav_id")) && Item->GetNumberField(TEXT("created_at"))>Latest){Latest=Item->GetNumberField(TEXT("created_at"));From=Object(Item,TEXT("position"));}}
        double Distance=0,Seconds=0;if(From && Points && Config)for(const auto& V:*Points){const auto Point=V->AsObject();const double Y=(Point->GetNumberField(TEXT("latitude"))-From->GetNumberField(TEXT("latitude")))*PI/180.*6371000.;const double X=(Point->GetNumberField(TEXT("longitude"))-From->GetNumberField(TEXT("longitude")))*PI/180.*6371000.*FMath::Cos((Point->GetNumberField(TEXT("latitude"))+From->GetNumberField(TEXT("latitude")))*.5*PI/180.);const double Z=Point->GetNumberField(TEXT("altitude"))-From->GetNumberField(TEXT("altitude"));const double D=FMath::Sqrt(X*X+Y*Y+Z*Z),Speed=Point->GetNumberField(TEXT("segmentSpeed"));Distance+=D;Seconds+=D/(Speed>0?Speed:Config->GetNumberField(TEXT("default_speed_mps")))+Point->GetNumberField(TEXT("waitTime"));From=Point;}
        bool Closed=false;if(R)R->TryGetBoolField(TEXT("bClosedLoop"),Closed);
        if(Closed && From && Points && Points->Num()>2 && Config){const auto Point=(*Points)[0]->AsObject();const double Y=(Point->GetNumberField(TEXT("latitude"))-From->GetNumberField(TEXT("latitude")))*PI/180.*6371000.,X=(Point->GetNumberField(TEXT("longitude"))-From->GetNumberField(TEXT("longitude")))*PI/180.*6371000.*FMath::Cos((Point->GetNumberField(TEXT("latitude"))+From->GetNumberField(TEXT("latitude")))*.5*PI/180.),Z=Point->GetNumberField(TEXT("altitude"))-From->GetNumberField(TEXT("altitude"));const double D=FMath::Sqrt(X*X+Y*Y+Z*Z),Speed=From->GetNumberField(TEXT("segmentSpeed"));Distance+=D;Seconds+=D/(Speed>0?Speed:Config->GetNumberField(TEXT("default_speed_mps")));}
        Lines.Add(FText::Format(T(TEXT("Execution.Estimate")),FText::AsNumber(FMath::RoundToInt(Distance)),FText::AsNumber(FMath::RoundToInt(Seconds))));
        Lines.Add(T(Fixture?TEXT("Execution.MockAvailable"):TEXT("Errors.MOCK_UAV_UNAVAILABLE")));Lines.Add(T(TEXT("Execution.MockCheck")));
        bool Conflict=false;const auto All=Object(State,TEXT("executions"));if(All)for(const auto& It:All->Values)if(ExecutionUI::Active(It.Value->AsObject()) && Field(It.Value->AsObject(),TEXT("uav_id"))==Field(M,TEXT("assigned_uav_id")))Conflict=true;
        Lines.Add(T(Conflict?TEXT("Errors.EXECUTION_CONFLICT"):TEXT("Execution.NoConflict")));
        Lines.Add(T(TEXT("Execution.DeploymentValid")));
        Actions[TEXT("execution_confirm")]->SetIsEnabled(!bPending && Sync->IsReady() && Sync->IsRoleOnline(TEXT("Map")) && Count>=2 && Fixture.IsValid() && UavIds.Contains(Field(M,TEXT("assigned_uav_id"))) && !Conflict);
        Lines.Add(FText::Format(T(TEXT("Workflow.Readiness")),T(Sync->IsReady()?TEXT("Stage1.Online"):TEXT("Stage1.Offline")),T(Sync->IsRoleOnline(TEXT("Map"))?TEXT("Stage1.Online"):TEXT("Stage1.Offline"))));
    }else if(Prompt)Lines.Add(ProductText::Get(TEXT("Execution.")+ExecutionIntent+TEXT("Confirm")));
    else if(E){
        Lines.Add(FText::Format(T(TEXT("Execution.Duration")),FText::AsNumber(FMath::RoundToInt(E->GetNumberField(TEXT("elapsed_seconds"))))));
        if(!Field(E,TEXT("completion_reason")).IsEmpty())Lines.Add(ProductText::Get(TEXT("Execution.")+Field(E,TEXT("completion_reason"))));
        if(!Field(E,TEXT("failure_reason")).IsEmpty())Lines.Add(ProductText::Get(TEXT("Errors.")+Field(E,TEXT("failure_reason"))));
    }
    if(bPending)Lines.Add(T(TEXT("Common.Waiting")));ExecutionPrompt->SetText(FText::Join(User(TEXT("\n")),Lines));
    Lines.Empty();Lines.Add(T(TEXT("Execution.History")));const auto All=Object(State,TEXT("executions"));
    if(All)for(const auto& Entry:All->Values){const auto Item=Entry.Value->AsObject();if(Field(Item,TEXT("plan_id"))==PlanId)Lines.Add(FText::Format(User(TEXT("{0}  {1}  {2}")),User(Field(Item,TEXT("execution_id"))),ProductText::Get(TEXT("Execution.")+Field(Item,TEXT("state"))),FText::AsDateTime(FDateTime::FromUnixTimestamp(Item->GetNumberField(TEXT("created_at"))))));}
    ExecutionHistory->SetText(FText::Join(User(TEXT("\n")),Lines));
}
void USecurityPlanWorkspaceWidget::ExecutionAction(FName Name){
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();const auto E=ExecutionUI::Latest(Sync->GetPlans(),PlanId);
    if(bPending)return;
    if(Name==TEXT("execution_start_prompt")){bExecutionView=true;ExecutionIntent=TEXT("start");StartRequestId=FGuid::NewGuid().ToString();Refresh();return;}
    if(Name==TEXT("execution_open")){bExecutionView=true;ExecutionIntent.Empty();Refresh();return;}
    if(Name==TEXT("execution_details")){bExecutionView=false;Refresh();return;}
    if(Name==TEXT("execution_cancel")){ExecutionIntent.Empty();bExecutionView=ExecutionUI::Active(E);Refresh();return;}
    if(Name==TEXT("execution_video")){if(E)Sync->OpenVideo(Field(E,TEXT("uav_id")));return;}
    if(Name==TEXT("execution_return_prompt") || Name==TEXT("execution_abort_prompt")){ExecutionIntent=Name==TEXT("execution_return_prompt")?TEXT("return"):TEXT("abort");Refresh();return;}
    FString Action=Name.ToString();if(Name==TEXT("execution_confirm")){if(ExecutionIntent.IsEmpty())return;Action=TEXT("execution_")+ExecutionIntent;}
    auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),Action);R->SetStringField(TEXT("plan_id"),PlanId);R->SetStringField(TEXT("mission_id"),MissionId);
    R->SetStringField(TEXT("request_id"),Action==TEXT("execution_start")?StartRequestId:FGuid::NewGuid().ToString());
    if(E){R->SetStringField(TEXT("execution_id"),Field(E,TEXT("execution_id")));R->SetNumberField(TEXT("control_version"),E->GetNumberField(TEXT("control_version")));}
    bPending=true;ErrorCode.Empty();const auto Weak=TWeakObjectPtr<USecurityPlanWorkspaceWidget>(this);
    if(!Sync->SubmitPlan(R,[Weak](TSharedPtr<FJsonObject> Reply){if(!Weak.IsValid())return;Weak->bPending=false;Weak->ErrorCode=PlanUI::Error(Reply);if(Weak->ErrorCode.IsEmpty()){Weak->ExecutionIntent.Empty();Weak->bExecutionView=true;}Weak->Refresh();})){bPending=false;ErrorCode=TEXT("SYNC_OFFLINE");}Refresh();
}
