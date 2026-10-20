#include "Shared/SecurityPlanWorkspaceWidget.h"
#include "Shared/PlanWidgetSupport.h"
#include "Command/FigmaCommandStyle.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/Image.h"

using namespace PlanUI;

void USecurityPlanWorkspaceWidget::BuildPlanPresentation()
{
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
}

void USecurityPlanWorkspaceWidget::RefreshPlanPresentation()
{
    const bool Creating = bCreating;
    Actions[TEXT("new_plan")]->SetVisibility(Creating ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
    NewPlanLayer->SetVisibility(Creating ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    NewPlanDialog->SetVisibility(Creating ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ConfigurationGuardLayer->SetVisibility(bConfigurationGuard ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
    UVerticalBox* ResultParent=bConfigurationGuard?ConfigurationGuard.Get():Creating?NewPlanContent.Get():Content.Get();
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
