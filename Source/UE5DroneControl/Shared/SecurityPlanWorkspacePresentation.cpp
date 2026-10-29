#include "Shared/SecurityPlanWorkspaceWidget.h"
#include "Shared/PlanWidgetSupport.h"
#include "Shared/PlanConfiguration.h"
#include "Command/FigmaCommandStyle.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

using namespace PlanUI;

void USecurityPlanWorkspaceWidget::BuildPlanPresentation()
{
    // Recompose the existing controls; their action identities and acknowledged
    // draft model remain unchanged. Workspace stays the mission list's owner.
    Workspace->RemoveFromParent();
    auto* DetailRoot=WidgetTree->ConstructWidget<UVerticalBox>();WorkspaceScroll->AddChild(DetailRoot);
    auto* DetailNavigation=WidgetTree->ConstructWidget<UHorizontalBox>();DetailRoot->AddChildToVerticalBox(DetailNavigation)->SetPadding(FMargin(0,0,0,14));
    auto MoveAction=[&](const TCHAR* Key,UHorizontalBox* Parent){auto* B=Actions[Key].Get();B->RemoveFromParent();auto* S=Parent->AddChildToHorizontalBox(B);S->SetPadding(FMargin(0,0,12,0));S->SetVerticalAlignment(VAlign_Center);FigmaUI::Button(B,false,true);auto Style=B->GetStyle();Style.SetNormal(FSlateRoundedBoxBrush(FLinearColor(FColor(30,36,51)),12));B->SetStyle(Style);return B;};
    auto* Back=MoveAction(TEXT("back_list"),DetailNavigation);
    auto* BackCaption=Back->GetContent();CastChecked<UTextBlock>(BackCaption)->SetText(T(TEXT("Nav.Plans")));BackCaption->RemoveFromParent();auto* BackRow=WidgetTree->ConstructWidget<UHorizontalBox>();Back->SetContent(BackRow);
    auto* BackIcon=BackRow->AddChildToHorizontalBox(FigmaUI::Icon(WidgetTree,TEXT("plan-detail-back"),FVector2D(12,10)));BackIcon->SetVerticalAlignment(VAlign_Center);BackIcon->SetPadding(FMargin(0,0,8,0));BackRow->AddChildToHorizontalBox(BackCaption)->SetVerticalAlignment(VAlign_Center);
    MoveAction(TEXT("edit_plan"),DetailNavigation);MoveAction(TEXT("delete_plan_prompt"),DetailNavigation);MoveAction(TEXT("resume_configuration"),DetailNavigation);
    DetailColumns=WidgetTree->ConstructWidget<UHorizontalBox>();DetailRoot->AddChild(DetailColumns);
    auto* MainColumnSlot=DetailColumns->AddChildToHorizontalBox(Workspace);FSlateChildSize MainWeight(ESlateSizeRule::Fill);MainWeight.Value=.66f;MainColumnSlot->SetSize(MainWeight);MainColumnSlot->SetPadding(FMargin(0,0,16,0));
    DetailSidebar=WidgetTree->ConstructWidget<UVerticalBox>();auto* SideSlot=DetailColumns->AddChildToHorizontalBox(DetailSidebar);FSlateChildSize SideWeight(ESlateSizeRule::Fill);SideWeight.Value=.34f;SideSlot->SetSize(SideWeight);
    for(UWidget* Child:Workspace->GetAllChildren())Child->RemoveFromParent();
    auto Card=[&](UVerticalBox* Parent,const FLinearColor& Fill=FigmaUI::Inset){auto* B=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(B,Fill,FigmaUI::Outline,9,FMargin(20));Parent->AddChildToVerticalBox(B)->SetPadding(FMargin(0,0,0,16));return B;};
    auto Divider=[&](UVerticalBox* Parent,const TCHAR* Semantic,double Width){auto* Line=WidgetTree->ConstructWidget<USizeBox>();Line->SetWidthOverride(Width);Line->SetHeightOverride(1);Line->SetContent(FigmaUI::Icon(WidgetTree,Semantic,FVector2D(Width,1)));auto* S=Parent->AddChildToVerticalBox(Line);S->SetHorizontalAlignment(HAlign_Center);S->SetPadding(FMargin(0,8,0,12));};
    auto* OverviewCard=Card(Workspace);auto* Overview=WidgetTree->ConstructWidget<UVerticalBox>();OverviewCard->SetContent(Overview);
    auto* TitleRow=WidgetTree->ConstructWidget<UHorizontalBox>();Overview->AddChild(TitleRow);
    WorkspaceTitle->RemoveFromParent();FigmaUI::Text(WorkspaceTitle,26,FigmaUI::PrimaryText,true);WorkspaceTitle->SetAutoWrapText(false);WorkspaceTitle->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);WorkspaceTitle->SetClipping(EWidgetClipping::ClipToBounds);
    auto* TitleWidth=WidgetTree->ConstructWidget<USizeBox>();TitleWidth->SetMaxDesiredWidth(520);TitleWidth->SetContent(WorkspaceTitle);auto* TitleSlot=TitleRow->AddChildToHorizontalBox(TitleWidth);TitleSlot->SetVerticalAlignment(VAlign_Center);
    PlanStatusCaption=Label(WidgetTree,TitleRow,FText::GetEmpty(),12);FigmaUI::Text(PlanStatusCaption,12,FigmaUI::Online);auto* StatusSlot=CastChecked<UHorizontalBoxSlot>(PlanStatusCaption->Slot);StatusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));StatusSlot->SetVerticalAlignment(VAlign_Center);StatusSlot->SetPadding(FMargin(14,0));
    MoveAction(TEXT("request_map_plan_move"),TitleRow);
    Summary->RemoveFromParent();FigmaUI::Text(Summary,14,FigmaUI::Muted);Overview->AddChildToVerticalBox(Summary)->SetPadding(FMargin(0,10,0,18));
    GeometryStatus->RemoveFromParent();Overview->AddChild(GeometryStatus);
    auto* TrackCard=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(TrackCard,FigmaUI::Inset,FigmaUI::Outline,8,FMargin(12,8));Overview->AddChild(TrackCard);
    auto* TrackHeight=WidgetTree->ConstructWidget<USizeBox>();TrackHeight->SetHeightOverride(88);TrackCard->SetContent(TrackHeight);
    auto* TrackFit=WidgetTree->ConstructWidget<UScaleBox>();TrackFit->SetStretch(EStretch::ScaleToFit);TrackHeight->SetContent(TrackFit);
    auto* TrackSize=WidgetTree->ConstructWidget<USizeBox>();TrackSize->SetWidthOverride(796);TrackSize->SetHeightOverride(80);TrackFit->SetContent(TrackSize);
    auto* TrackCanvas=WidgetTree->ConstructWidget<UCanvasPanel>();TrackSize->SetContent(TrackCanvas);
    auto Place=[&](UWidget* W,const FVector2D& Position,const FVector2D& Size){auto* S=TrackCanvas->AddChildToCanvas(W);S->SetPosition(Position);S->SetSize(Size);return S;};
    Place(FigmaUI::Icon(WidgetTree,TEXT("plan-detail-track"),FVector2D(700,32)),FVector2D(48,6),FVector2D(700,31.18));
    const double Centers[]={63.59,229.31,397.02,564.22,732.44};
    const TCHAR* StepKeys[]={TEXT("Workflow.BasicInfo"),TEXT("Workflow.TaskConfig"),TEXT("Workflow.StepRoute"),TEXT("Workflow.PreDeployReview"),TEXT("Workflow.StepDeploy")};
    for(int I=0;I<5;++I){
        auto* Marker=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(Marker,FLinearColor::Transparent,FLinearColor::Transparent,18,FMargin(0));Place(Marker,FVector2D(Centers[I]-15.59,6),FVector2D(31.18,31.18));StepMarkers.Add(Marker);
        auto* Number=Label(WidgetTree,Marker,FText::AsNumber(I+1),12);FigmaUI::Text(Number,12);Number->SetJustification(ETextJustify::Center);Marker->SetHorizontalAlignment(HAlign_Center);Marker->SetVerticalAlignment(VAlign_Center);
        auto* StepLabel=WidgetTree->ConstructWidget<UTextBlock>();FigmaUI::Text(StepLabel,12);StepLabel->SetText(T(StepKeys[I]));StepLabel->SetAutoWrapText(true);StepLabel->SetJustification(ETextJustify::Center);Place(StepLabel,FVector2D(Centers[I]-63,48),FVector2D(126,30));StepLabels.Add(StepLabel);
    }
    Steps->RemoveFromParent();Overview->AddChild(Steps);Steps->SetVisibility(ESlateVisibility::Collapsed);
    MissionList->RemoveFromParent();Workspace->AddChildToVerticalBox(MissionList)->SetPadding(FMargin(0,0,0,16));
    for(UVerticalBox* Page:{BasicPageHost.Get(),TaskPage.Get(),RoutePage.Get(),ExecutionPage.Get()}){
        Page->RemoveFromParent();Workspace->AddChildToVerticalBox(Page)->SetPadding(FMargin(0,0,0,16));
    }
    DeploymentActions=Card(Workspace);auto* Operations=WidgetTree->ConstructWidget<UVerticalBox>();DeploymentActions->SetContent(Operations);
    FigmaUI::Text(Label(WidgetTree,Operations,T(TEXT("Figma.PlanDetail.DeploymentActions")),24),24,FigmaUI::PrimaryText,true);Divider(Operations,TEXT("plan-detail-divider"),820);
    auto* OperationsRow=WidgetTree->ConstructWidget<UHorizontalBox>();Operations->AddChild(OperationsRow);
    for(const TCHAR* Key:{TEXT("confirm_deployment"),TEXT("execution_start_prompt"),TEXT("execution_open"),TEXT("task_step")}){
        auto* B=MoveAction(Key,OperationsRow);CastChecked<UHorizontalBoxSlot>(B->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }
    ReviewPage->RemoveFromParent();DetailSidebar->AddChild(ReviewPage);
    auto* ReviewCard=Card(ReviewPage);auto* ReviewContent=WidgetTree->ConstructWidget<UVerticalBox>();ReviewCard->SetContent(ReviewContent);
    ReviewHeading->RemoveFromParent();FigmaUI::Text(ReviewHeading,24,FigmaUI::PrimaryText,true);ReviewContent->AddChild(ReviewHeading);Divider(ReviewContent,TEXT("plan-detail-review-divider"),420);
    DeploymentBlockers->RemoveFromParent();ReviewContent->AddChild(DeploymentBlockers);
    Review->RemoveFromParent();ReviewContent->AddChild(Review);
    auto* HistoryCard=Card(DetailSidebar);auto* HistoryContent=WidgetTree->ConstructWidget<UVerticalBox>();HistoryCard->SetContent(HistoryContent);
    FigmaUI::Text(Label(WidgetTree,HistoryContent,T(TEXT("Execution.History")),22),22,FigmaUI::PrimaryText,true);Divider(HistoryContent,TEXT("plan-detail-review-divider"),420);
    ExecutionHistory->RemoveFromParent();FigmaUI::Text(ExecutionHistory,12,FigmaUI::Muted);HistoryContent->AddChild(ExecutionHistory);
    auto* Copy=Actions[TEXT("copy_plan")].Get();Copy->RemoveFromParent();DetailSidebar->AddChild(Copy);FigmaUI::Button(Copy,false,true);

    auto* Overlay = WidgetTree->ConstructWidget<UOverlay>();
    WidgetTree->RootWidget = Overlay;
    auto* MainSlot = Overlay->AddChildToOverlay(Content);
    MainSlot->SetHorizontalAlignment(HAlign_Fill);
    MainSlot->SetVerticalAlignment(VAlign_Fill);

    // Modal layers receive input over the page while the shell's execution
    // controls stay available. Closing still goes through the configuration guard.
    auto MakeLayer = [&]()
    {
        auto* Layer = WidgetTree->ConstructWidget<UOverlay>();
        auto* LayerSlot = Overlay->AddChildToOverlay(Layer);
        LayerSlot->SetHorizontalAlignment(HAlign_Fill);
        LayerSlot->SetVerticalAlignment(VAlign_Fill);
        auto* Shade = WidgetTree->ConstructWidget<UBorder>();
        FigmaUI::Panel(Shade, FLinearColor(0,0,0,.73f), FLinearColor::Transparent, 0, FMargin(0));
        auto* ShadeSlot = Layer->AddChildToOverlay(Shade);
        ShadeSlot->SetHorizontalAlignment(HAlign_Fill);
        ShadeSlot->SetVerticalAlignment(VAlign_Fill);
        return Layer;
    };
    NewPlanLayer = MakeLayer();
    // The design retains the disabled create affordance above its dimmed page.
    auto* CreatingPill = WidgetTree->ConstructWidget<UBorder>();
    const FLinearColor CreatingFill(FColor(165,164,164));
    FigmaUI::Panel(CreatingPill,CreatingFill,CreatingFill,19,FMargin(20,9));
    auto* CreatingRow = WidgetTree->ConstructWidget<UHorizontalBox>();CreatingPill->SetContent(CreatingRow);
    auto* CreatingIcon = CreatingRow->AddChildToHorizontalBox(FigmaUI::Icon(WidgetTree,TEXT("plus-black"),FVector2D(12,12)));
    CreatingIcon->SetVerticalAlignment(VAlign_Center);CreatingIcon->SetPadding(FMargin(2,0,10,0));
    FigmaUI::Text(Label(WidgetTree,CreatingRow,T(TEXT("Figma.Plans.NewPlan")),14),14,FLinearColor::Black);
    auto* CreatingSlot=NewPlanLayer->AddChildToOverlay(CreatingPill);
    CreatingSlot->SetHorizontalAlignment(HAlign_Right);CreatingSlot->SetVerticalAlignment(VAlign_Top);CreatingSlot->SetPadding(FMargin(0,13,0,0));
    CreatingPill->SetVisibility(ESlateVisibility::HitTestInvisible);
    NewPlanDialog = WidgetTree->ConstructWidget<UBorder>();
    FigmaUI::Panel(NewPlanDialog, FigmaUI::Surface, FigmaUI::Outline, 12, FMargin(24));
    auto* DialogSize = WidgetTree->ConstructWidget<USizeBox>();
    DialogSize->SetWidthOverride(590);
    DialogSize->SetMinDesiredHeight(444);
    DialogSize->SetContent(NewPlanDialog);
    auto* DialogSlot = NewPlanLayer->AddChildToOverlay(DialogSize);
    DialogSlot->SetHorizontalAlignment(HAlign_Center);
    DialogSlot->SetVerticalAlignment(VAlign_Center);
    DialogSlot->SetPadding(FMargin(0,30,0,0));
    NewPlanContent = WidgetTree->ConstructWidget<UVerticalBox>();
    NewPlanDialog->SetContent(NewPlanContent);

    auto* Heading = WidgetTree->ConstructWidget<UHorizontalBox>();
    NewPlanContent->AddChild(Heading);
    auto* Title = Label(WidgetTree, Heading, T(TEXT("Figma.Plans.NewPlan")), 29);
    FigmaUI::Text(Title,29,FigmaUI::PrimaryText,true);
    Cast<UHorizontalBoxSlot>(Title->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* Close = WidgetTree->ConstructWidget<UCommandActionButton>();
    Close->Configure(TEXT("cancel_new"));
    Close->OnAction.AddDynamic(this,&USecurityPlanWorkspaceWidget::Action);
    Close->SetToolTipText(T(TEXT("Common.Cancel")));
    FigmaUI::Button(Close,false,true);
    auto CloseStyle = Close->GetStyle();
    CloseStyle.SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent,4));
    CloseStyle.SetNormalPadding(FMargin(4));
    CloseStyle.SetPressedPadding(FMargin(4));
    Close->SetStyle(CloseStyle);
    Close->SetContent(FigmaUI::Icon(WidgetTree,TEXT("close"),FVector2D(30,30)));
    Heading->AddChildToHorizontalBox(Close)->SetVerticalAlignment(VAlign_Center);
    Actions.Add(TEXT("close_new"),Close);
    auto* Hint = Label(WidgetTree,NewPlanContent,T(TEXT("Figma.Plans.NewPlanHint")),16);
    FigmaUI::Text(Hint,16,FigmaUI::Muted);
    Cast<UVerticalBoxSlot>(Hint->Slot)->SetPadding(FMargin(0,8,0,27));

    auto* NameHeight = WidgetTree->ConstructWidget<USizeBox>();
    NameHeight->SetHeightOverride(42);
    PlanName->RemoveFromParent();
    NameHeight->SetContent(PlanName);
    BasicPage->InsertChildAt(2,NameHeight);
    Cast<UVerticalBoxSlot>(NameHeight->Slot)->SetPadding(FMargin(0,8,0,22));
    if(auto* DescriptionHeight = Cast<USizeBox>(DescriptionEditor->GetParent()))
        Cast<UVerticalBoxSlot>(DescriptionHeight->Slot)->SetPadding(FMargin(0,8,0,0));
    auto* BasicActions = Cast<UHorizontalBox>(Actions[TEXT("create_plan")]->GetParent());
    Cast<UVerticalBoxSlot>(BasicActions->Slot)->SetPadding(FMargin(0,34,0,0));
    for(const TCHAR* Key : {TEXT("cancel_new"),TEXT("create_plan")})
    {
        auto* Button = Actions[Key].Get();
        auto Style = Button->GetStyle();
        Style.SetNormalPadding(FMargin(23,10));Style.SetPressedPadding(FMargin(23,10));
        Button->SetStyle(Style);
        FigmaUI::Text(Cast<UTextBlock>(Button->GetContent()),16,FigmaUI::PrimaryText,true);
    }
    auto CancelStyle = Actions[TEXT("cancel_new")]->GetStyle();
    CancelStyle.SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent,22,FigmaUI::PrimaryText,1));
    Actions[TEXT("cancel_new")]->SetStyle(CancelStyle);
    Cast<UHorizontalBoxSlot>(Actions[TEXT("create_plan")]->Slot)->SetPadding(FMargin(10,0,0,0));

    // The guard must be above the new-plan dialog; otherwise canceling a dirty
    // form would expose an inaccessible confirmation behind the dialog.
    ConfigurationGuard->RemoveFromParent();
    ConfigurationGuardLayer = MakeLayer();
    auto* GuardCard = WidgetTree->ConstructWidget<UBorder>();
    FigmaUI::Panel(GuardCard,FigmaUI::Surface,FigmaUI::Outline,12,FMargin(24));
    GuardCard->SetContent(ConfigurationGuard);
    auto* GuardSize = WidgetTree->ConstructWidget<USizeBox>();
    GuardSize->SetWidthOverride(650);GuardSize->SetContent(GuardCard);
    auto* GuardSlot = ConfigurationGuardLayer->AddChildToOverlay(GuardSize);
    GuardSlot->SetHorizontalAlignment(HAlign_Center);GuardSlot->SetVerticalAlignment(VAlign_Center);
    for(auto* Child : ConfigurationGuard->GetAllChildren())
        Cast<UVerticalBoxSlot>(Child->Slot)->SetPadding(FMargin(0,8));

    DeletePlanLayer=MakeLayer();
    auto* DeleteCard=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(DeleteCard,FigmaUI::Surface,FigmaUI::Outline,12,FMargin(24));
    auto* DeleteSize=WidgetTree->ConstructWidget<USizeBox>();DeleteSize->SetWidthOverride(590);DeleteSize->SetContent(DeleteCard);
    auto* DeleteSlot=DeletePlanLayer->AddChildToOverlay(DeleteSize);DeleteSlot->SetHorizontalAlignment(HAlign_Center);DeleteSlot->SetVerticalAlignment(VAlign_Center);
    DeletePlanContent=WidgetTree->ConstructWidget<UVerticalBox>();DeleteCard->SetContent(DeletePlanContent);
    FigmaUI::Text(Label(WidgetTree,DeletePlanContent,T(TEXT("Plan.Delete.Action")),28),28,FigmaUI::PrimaryText,true);
    DeletePlanPrompt=Label(WidgetTree,DeletePlanContent,FText::GetEmpty(),16);FigmaUI::Text(DeletePlanPrompt,16,FigmaUI::Muted);DeletePlanPrompt->SetAutoWrapText(true);
    Cast<UVerticalBoxSlot>(DeletePlanPrompt->Slot)->SetPadding(FMargin(0,16));
    DeletePlanNameInput=WidgetTree->ConstructWidget<UEditableTextBox>();FigmaUI::Input(DeletePlanNameInput);DeletePlanNameInput->SetHintText(T(TEXT("Plan.Delete.NameHint")));
    DeletePlanNameInput->OnTextChanged.AddDynamic(this,&USecurityPlanWorkspaceWidget::DeleteConfirmationChanged);DeletePlanContent->AddChild(DeletePlanNameInput);
    auto* DeleteActions=WidgetTree->ConstructWidget<UHorizontalBox>();DeletePlanContent->AddChildToVerticalBox(DeleteActions)->SetPadding(FMargin(0,24,0,0));
    for(const TCHAR* Key:{TEXT("cancel_delete"),TEXT("refresh_delete"),TEXT("confirm_delete")}){
        auto* ActionButton=Actions[Key].Get();ActionButton->RemoveFromParent();DeleteActions->AddChildToHorizontalBox(ActionButton)->SetPadding(FMargin(0,0,10,0));
    }
    FigmaUI::Text(Cast<UTextBlock>(Actions[TEXT("confirm_delete")]->GetContent()),14,FLinearColor(FColor(231,102,110)),true);
}

void USecurityPlanWorkspaceWidget::RefreshPlanPresentation()
{
    const bool Creating = bCreating;
    const bool Reviewing=ReviewPage->GetVisibility()!=ESlateVisibility::Collapsed;
    DetailSidebar->SetVisibility(Reviewing?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
    DeploymentActions->SetVisibility(Reviewing?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    CastChecked<UHorizontalBoxSlot>(Workspace->Slot)->SetPadding(Reviewing?FMargin(0,0,16,0):FMargin(0));
    const auto Plan=Find(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),TEXT("plans"),PlanId);
    const bool Deployed=Field(Plan,TEXT("status"))==TEXT("DEPLOYED");
    WorkspaceTitle->SetToolTipText(WorkspaceTitle->GetText());
    PlanStatusCaption->SetText(ProductText::Get(TEXT("Plan.")+Field(Plan,TEXT("status"))));
    FigmaUI::Text(PlanStatusCaption,12,Deployed?FigmaUI::Online:FigmaUI::Muted);
    ReviewHeading->SetText(T(Deployed?TEXT("Figma.PlanDetail.DeploymentInfo"):TEXT("Workflow.PreDeployReview")));
    Steps->SetVisibility(ESlateVisibility::Collapsed);
    for(int I=0;I<StepMarkers.Num();++I)FigmaUI::Panel(StepMarkers[I],I==CurrentStep?FigmaUI::Accent:FLinearColor::Transparent,FLinearColor::Transparent,18,FMargin(0));
    Actions[TEXT("new_plan")]->SetVisibility(Creating ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
    NewPlanLayer->SetVisibility(Creating ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    NewPlanDialog->SetVisibility(Creating ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ConfigurationGuardLayer->SetVisibility(bConfigurationGuard ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    const bool Deleting=!PendingDeletePlan.IsEmpty();
    DeletePlanLayer->SetVisibility(Deleting?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    DeletePlanPrompt->SetText(FText::Format(T(TEXT("Plan.Delete.Prompt")),User(PendingDeleteName)));
    DeletePlanNameInput->SetIsReadOnly(bPending);
    DeleteConfirmationChanged(DeletePlanNameInput->GetText());
    Actions[TEXT("cancel_delete")]->SetIsEnabled(!bPending);
    Actions[TEXT("refresh_delete")]->SetVisibility(Deleting && !ErrorCode.IsEmpty()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    Actions[TEXT("refresh_delete")]->SetIsEnabled(!bPending);
    NewPlanDialog->SetIsEnabled(!bConfigurationGuard);
    EmptyPlanState->SetVisibility(PlanIds.IsEmpty() && !Creating ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if(Creating && BasicPage->GetParent()!=NewPlanContent)
    {
        BasicPage->RemoveFromParent();NewPlanContent->AddChild(BasicPage);
    }
    else if(!Creating && BasicPage->GetParent()!=BasicPageHost)
    {
        BasicPage->RemoveFromParent();BasicPageHost->AddChild(BasicPage);
    }
    UVerticalBox* ResultParent=Deleting?DeletePlanContent.Get():bConfigurationGuard?ConfigurationGuard.Get():Creating?NewPlanContent.Get():Content.Get();
    if(Result->GetParent()!=ResultParent){Result->RemoveFromParent();ResultParent->AddChildToVerticalBox(Result)->SetPadding(FMargin(0,8,0,0));}
    FigmaUI::Text(Result,14,ErrorCode.IsEmpty()?FigmaUI::Muted:FLinearColor(FColor(231,102,110)));
    if(!DescriptionEditor->GetText().EqualTo(Description->GetText()))DescriptionEditor->SetText(Description->GetText());
    DescriptionEditor->SetIsReadOnly(Description->GetIsReadOnly());
    Actions[TEXT("close_new")]->SetIsEnabled(!bPending && !bConfigurationGuard);
    Actions[TEXT("cancel_new")]->SetIsEnabled(!bPending && !bConfigurationGuard);
    auto* CreateCaption=Cast<UTextBlock>(Actions[TEXT("create_plan")]->GetContent());
    FigmaUI::Text(CreateCaption,16,Actions[TEXT("create_plan")]->GetIsEnabled()?FLinearColor::Black:FLinearColor(FColor(92,95,105)),true);
}

void USecurityPlanWorkspaceWidget::PlanNameChanged(const FText&)
{
    if(!Actions.Contains(TEXT("create_plan")) || !GetGameInstance())return;
    const auto* Sync = GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    const bool Enabled = !bPending && !bConfigurationGuard && Sync->IsReady() && CanEditConfiguration()
        && !PlanName->GetText().ToString().TrimStartAndEnd().IsEmpty();
    Actions[TEXT("create_plan")]->SetIsEnabled(Enabled);
    FigmaUI::Text(Cast<UTextBlock>(Actions[TEXT("create_plan")]->GetContent()),16,
        Enabled?FLinearColor::Black:FLinearColor(FColor(92,95,105)),true);
}

void USecurityPlanWorkspaceWidget::DescriptionChanged(const FText& Text)
{
    if(Description && !Description->GetText().EqualTo(Text))Description->SetText(Text);
}

void USecurityPlanWorkspaceWidget::RequestPlanDeletion(const FString& Id)
{
    if(IsMissionNavigationBlocked() || Id.IsEmpty())return;
    auto Open=[this,Id]{PendingDeletePlan=Id;RefreshDeleteConfirmation();};
    // Reuse the save/discard guard and exact owned lease release before deleting.
    // A different client's lease remains authoritative and is rejected by Backend.
    if(CanEditConfiguration())RequestFinishConfiguration(Open);
    else GuardConfigurationChange(Open);
}

void USecurityPlanWorkspaceWidget::RefreshDeleteConfirmation()
{
    if(bPending || PendingDeletePlan.IsEmpty())return;
    const auto Plan=Find(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),TEXT("plans"),PendingDeletePlan);
    if(!Plan){ErrorCode=TEXT("PLAN_NOT_FOUND");Refresh();return;}
    // Freeze the reviewed identity and content revision. Refresh is an explicit
    // action that clears typed consent, never an automatic snapshot replacement.
    PendingDeleteName=Field(Plan,TEXT("name"));PendingDeleteRevision=Plan->GetNumberField(TEXT("content_revision"));
    DeletePlanNameInput->SetText(FText::GetEmpty());ErrorCode.Empty();ExecutionErrorDetails=FText::GetEmpty();Refresh();
}

void USecurityPlanWorkspaceWidget::DeleteConfirmationChanged(const FText& Text)
{
    if(!DeletePlanNameInput || !Actions.Contains(TEXT("confirm_delete")))return;
    Actions[TEXT("confirm_delete")]->SetIsEnabled(!bPending && !PendingDeletePlan.IsEmpty() && !PendingDeleteName.IsEmpty()
        && Text.ToString()==PendingDeleteName
        && GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->IsReady());
}
