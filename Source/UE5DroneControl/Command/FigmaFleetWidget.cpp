#include "Command/FigmaFleetWidget.h"
#include "Command/FigmaCommandStyle.h"
#include "Command/CommandActionButton.h"
#include "Command/GroupControlWidget.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Shared/ProductText.h"
#include "Shared/PlanWidgetSupport.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"

namespace
{
FText T(const TCHAR* Key) { return ProductText::Get(Key); }
FText User(const FString& Value) { return FText::AsCultureInvariant(Value); }
FText Connection(EDroneAvailability State)
{
    return T(State == EDroneAvailability::Online ? TEXT("Fleet.Online") : State == EDroneAvailability::Lost ? TEXT("Fleet.Lost") : TEXT("Fleet.Offline"));
}
FText Task(EDroneTaskState State)
{
    switch (State)
    {
    case EDroneTaskState::Assembling: return T(TEXT("Figma.Fleet.Task.Assembling"));
    case EDroneTaskState::Moving: return T(TEXT("Figma.Fleet.Task.Moving"));
    case EDroneTaskState::Scouting: return T(TEXT("Figma.Fleet.Task.Scouting"));
    case EDroneTaskState::Patrolling: return T(TEXT("Figma.Fleet.Task.Patrolling"));
    case EDroneTaskState::Attacking: return T(TEXT("Figma.Fleet.Task.Attacking"));
    case EDroneTaskState::Paused: return T(TEXT("Figma.Fleet.Task.Paused"));
    case EDroneTaskState::Avoiding: return T(TEXT("Figma.Fleet.Task.Avoiding"));
    case EDroneTaskState::Completed: return T(TEXT("Figma.Fleet.Task.Completed"));
    case EDroneTaskState::Error: return T(TEXT("Figma.Fleet.Task.Error"));
    default: return T(TEXT("Fleet.Idle"));
    }
}
FSlateChildSize Fill(float Value = 1.f) { FSlateChildSize Size(ESlateSizeRule::Fill); Size.Value = Value; return Size; }
USizeBox* Sized(UWidgetTree* Tree, UWidget* Child, float Width, float Height)
{
    auto* Box = Tree->ConstructWidget<USizeBox>();
    if (Width > 0) Box->SetWidthOverride(Width);
    if (Height > 0) Box->SetHeightOverride(Height);
    Box->SetContent(Child); return Box;
}
const FLinearColor LabelColors[] = {
    FLinearColor(FColor(217, 217, 217)), FLinearColor(FColor(112, 114, 118)),
    FLinearColor(FColor(56, 104, 148)), FLinearColor(FColor(119, 72, 144)),
    FLinearColor(FColor(144, 130, 54)), FLinearColor(FColor(44, 136, 104))
};
}

UTextBlock* UFigmaFleetWidget::MakeText(const FText& Value, int32 Size, bool Bold)
{
    auto* Text = WidgetTree->ConstructWidget<UTextBlock>();
    FigmaUI::Text(Text, Size, FigmaUI::PrimaryText, Bold); Text->SetText(Value); return Text;
}

UCommandActionButton* UFigmaFleetWidget::MakeAction(FName Name, const TCHAR* Key, int32 Id, const FString& Icon)
{
    auto* Button = WidgetTree->ConstructWidget<UCommandActionButton>();
    Button->Configure(Name, Id); Button->OnAction.AddDynamic(this, &UFigmaFleetWidget::Action);
    FigmaUI::Button(Button, false, true);
    if (Name == TEXT("toggle") || Name == TEXT("label") || Name == TEXT("label_cancel"))
    {
        auto Style = Button->GetStyle(); Style.SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent, 5));
        Style.SetNormalPadding(FMargin(6)); Style.SetPressedPadding(FMargin(6)); Button->SetStyle(Style);
    }
    auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>(); Button->SetContent(Row);
    if (!Icon.IsEmpty()) Row->AddChildToHorizontalBox(FigmaUI::Icon(WidgetTree, Icon, FVector2D(19, 19)))->SetVerticalAlignment(VAlign_Center);
    if (Key && *Key)
    {
        auto* Label = MakeText(T(Key), 12);
        auto* ActionLabelSlot = Row->AddChildToHorizontalBox(Label); ActionLabelSlot->SetPadding(FMargin(Icon.IsEmpty() ? 0 : 8, 0, 0, 0)); ActionLabelSlot->SetVerticalAlignment(VAlign_Center);
    }
    return Button;
}

void UFigmaFleetWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    RootOverlay = WidgetTree->ConstructWidget<UOverlay>(); WidgetTree->RootWidget = RootOverlay;
    auto* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    auto* ContentSlot = RootOverlay->AddChildToOverlay(Content); ContentSlot->SetHorizontalAlignment(HAlign_Fill); ContentSlot->SetVerticalAlignment(VAlign_Fill);
    Content->AddChildToVerticalBox(MakeText(T(TEXT("Figma.Fleet.Title")), 34, true))->SetPadding(FMargin(0, 0, 0, 18));
    auto* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(); Content->AddChildToVerticalBox(Columns)->SetSize(Fill());
    auto* Left = WidgetTree->ConstructWidget<UVerticalBox>();
    auto* LeftSlot = Columns->AddChildToHorizontalBox(Left); LeftSlot->SetSize(Fill(0.67f)); LeftSlot->SetPadding(FMargin(0, 0, 12, 0));
    auto* Right = WidgetTree->ConstructWidget<UVerticalBox>(); Columns->AddChildToHorizontalBox(Right)->SetSize(Fill(0.33f));

    auto* ListCard = WidgetTree->ConstructWidget<UBorder>(); FigmaUI::Panel(ListCard, FigmaUI::Inset);
    auto* List = WidgetTree->ConstructWidget<UVerticalBox>(); ListCard->SetContent(List);
    List->AddChildToVerticalBox(MakeText(T(TEXT("Figma.Fleet.List")), 20, true))->SetPadding(FMargin(0, 0, 0, 14));
    DroneScroll = WidgetTree->ConstructWidget<UScrollBox>();
    List->AddChildToVerticalBox(DroneScroll)->SetSize(Fill());
    EmptyList = MakeText(T(TEXT("Figma.Fleet.Empty")), 15); EmptyList->SetAutoWrapText(true);
    SelectionStatus = MakeText(FText::GetEmpty(), 12); SelectionStatus->SetAutoWrapText(true);
    SelectionStatus->SetVisibility(ESlateVisibility::Collapsed);
    FigmaUI::Text(SelectionStatus, 12, FigmaUI::Muted);
    List->AddChildToVerticalBox(SelectionStatus)->SetPadding(FMargin(0, 6, 0, 0));
    auto* ListSlot = Left->AddChildToVerticalBox(ListCard); ListSlot->SetSize(Fill()); ListSlot->SetPadding(FMargin(0, 0, 0, 10));

    auto* TelemetryCard = WidgetTree->ConstructWidget<UBorder>(); FigmaUI::Panel(TelemetryCard, FigmaUI::Inset);
    Left->AddChildToVerticalBox(Sized(WidgetTree, TelemetryCard, 0, 153));
    auto* Telemetry = WidgetTree->ConstructWidget<UVerticalBox>(); TelemetryCard->SetContent(Telemetry);
    auto* TelemetryHeader = WidgetTree->ConstructWidget<UHorizontalBox>(); Telemetry->AddChildToVerticalBox(TelemetryHeader)->SetPadding(FMargin(0, 0, 0, 12));
    TelemetryHeader->AddChildToHorizontalBox(MakeText(T(TEXT("Figma.Fleet.Telemetry")), 19, true))->SetSize(Fill());
    TelemetryState = MakeText(FText::GetEmpty(), 12); TelemetryHeader->AddChildToHorizontalBox(TelemetryState)->SetVerticalAlignment(VAlign_Center);
    Telemetry->AddChildToVerticalBox(Sized(WidgetTree, FigmaUI::Icon(WidgetTree, TEXT("fleet-divider"), FVector2D(880, 1)), 880, 1))->SetPadding(FMargin(0, 0, 0, 12));
    auto* Metrics = WidgetTree->ConstructWidget<UHorizontalBox>(); Telemetry->AddChildToVerticalBox(Metrics)->SetSize(Fill());
    const TCHAR* MetricKeys[] = {TEXT("Figma.Fleet.Altitude"),TEXT("Figma.Fleet.Speed"),TEXT("Figma.Fleet.Battery"),TEXT("Figma.Fleet.Link"),TEXT("Figma.Fleet.Task")};
    const TCHAR* MetricIcons[] = {TEXT("altitude"), TEXT("speed"), TEXT("battery"), TEXT("link"), TEXT("task")};
    for (int32 I = 0; I < 5; ++I)
    {
        if (I > 0)
        {
            auto* DividerCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
            auto* DividerImage = FigmaUI::Icon(WidgetTree, TEXT("metric-divider"), FVector2D(1, 46));
            auto* DividerSlot = DividerCanvas->AddChildToCanvas(DividerImage);
            DividerSlot->SetSize(FVector2D(46, 1)); DividerSlot->SetPosition(FVector2D(-22.5, 22.5));
            Metrics->AddChildToHorizontalBox(Sized(WidgetTree, DividerCanvas, 1, 46))->SetVerticalAlignment(VAlign_Center);
        }
        auto* Metric = WidgetTree->ConstructWidget<UHorizontalBox>(); auto* MetricSlot = Metrics->AddChildToHorizontalBox(Metric); MetricSlot->SetSize(Fill()); MetricSlot->SetPadding(FMargin(I ? 12 : 0, 0, 4, 0));
        Metric->AddChildToHorizontalBox(FigmaUI::Icon(WidgetTree, MetricIcons[I], FVector2D(32, 32)))->SetVerticalAlignment(VAlign_Center);
        auto* Texts = WidgetTree->ConstructWidget<UVerticalBox>(); Metric->AddChildToHorizontalBox(Texts)->SetPadding(FMargin(12, 0, 0, 0));
        auto* Caption = MakeText(T(MetricKeys[I]), 12); FigmaUI::Text(Caption, 12, FigmaUI::Muted); Texts->AddChildToVerticalBox(Caption);
        auto* Value = MakeText(User(TEXT("N/A")), 15, true); Texts->AddChildToVerticalBox(Value)->SetPadding(FMargin(0, 5, 0, 0)); TelemetryValues.Add(Value);
    }

    auto* GroupCard = WidgetTree->ConstructWidget<UBorder>(); FigmaUI::Panel(GroupCard, FigmaUI::Inset, FigmaUI::Outline, 10, FMargin(14));
    GroupControls = CreateWidget<UGroupControlWidget>(GetOwningPlayer()); GroupControls->ConfigureFigmaLayout(); GroupCard->SetContent(GroupControls);
    Right->AddChildToVerticalBox(Sized(WidgetTree, GroupCard, 0, 292))->SetPadding(FMargin(0, 0, 0, 10));

    auto* VideoCard = WidgetTree->ConstructWidget<UBorder>(); FigmaUI::Panel(VideoCard, FigmaUI::Inset, FigmaUI::Outline, 10, FMargin(14));
    Right->AddChildToVerticalBox(VideoCard)->SetSize(Fill());
    auto* Video = WidgetTree->ConstructWidget<UVerticalBox>(); VideoCard->SetContent(Video);
    auto* VideoHeader = WidgetTree->ConstructWidget<UHorizontalBox>(); Video->AddChildToVerticalBox(VideoHeader)->SetPadding(FMargin(0, 0, 0, 12));
    VideoHeader->AddChildToHorizontalBox(MakeText(T(TEXT("Figma.Fleet.Video")), 19, true))->SetSize(Fill());
    VideoChoice = WidgetTree->ConstructWidget<UComboBoxString>(); VideoChoice->SetContentPadding(FMargin(8, 4));
    VideoChoice->OnGenerateWidgetEvent.BindDynamic(this, &UFigmaFleetWidget::GenerateVideoOption);
    auto ComboStyle = VideoChoice->GetWidgetStyle(); ComboStyle.ComboButtonStyle.ButtonStyle.SetNormal(FSlateRoundedBoxBrush(FigmaUI::Inset, 5, FigmaUI::Outline, 1));
    ComboStyle.ComboButtonStyle.ButtonStyle.SetHovered(FSlateRoundedBoxBrush(FigmaUI::Inset, 5, FigmaUI::Muted, 1)); VideoChoice->SetWidgetStyle(ComboStyle);
    VideoHeader->AddChildToHorizontalBox(Sized(WidgetTree, VideoChoice, 145, 28))->SetPadding(FMargin(8, 0));
    VideoOpen = MakeAction(TEXT("video"), TEXT(""), 0, TEXT("fullscreen")); VideoOpen->SetToolTipText(T(TEXT("Video.Open"))); VideoHeader->AddChildToHorizontalBox(VideoOpen);
    auto* VideoMatte = WidgetTree->ConstructWidget<UBorder>(); FigmaUI::Panel(VideoMatte, FLinearColor(FColor(13, 16, 23)), FigmaUI::Outline, 5, FMargin(12));
    VideoMatte->SetHorizontalAlignment(HAlign_Center); VideoMatte->SetVerticalAlignment(VAlign_Center); Video->AddChildToVerticalBox(VideoMatte)->SetSize(Fill());
    auto* Empty = WidgetTree->ConstructWidget<UVerticalBox>(); VideoMatte->SetContent(Empty);
    VideoEmptyIcon = FigmaUI::Icon(WidgetTree, TEXT("no-video"), FVector2D(62, 62)); Empty->AddChildToVerticalBox(VideoEmptyIcon)->SetHorizontalAlignment(HAlign_Center);
    VideoTitle = MakeText(T(TEXT("Figma.Fleet.NoVideo")), 18, true); VideoTitle->SetJustification(ETextJustify::Center); Empty->AddChildToVerticalBox(VideoTitle)->SetPadding(FMargin(0, 8, 0, 6));
    VideoHint = MakeText(T(TEXT("Figma.Fleet.VideoHint")), 12); FigmaUI::Text(VideoHint, 12, FigmaUI::Muted); VideoHint->SetAutoWrapText(true); VideoHint->SetJustification(ETextJustify::Center); Empty->AddChildToVerticalBox(VideoHint);
    BuildLabelDialog(); Refresh();
}

void UFigmaFleetWidget::BuildRows(const TArray<FDroneDescriptor>& Descriptors)
{
    Rows.Empty(); DroneIds.Empty(); DroneScroll->ClearChildren();
    for (const auto& Descriptor : Descriptors)
    {
        const int32 Id = Descriptor.DroneId; DroneIds.Add(Id); FRow Row;
        Row.Card = WidgetTree->ConstructWidget<UBorder>(); FigmaUI::Panel(Row.Card, FigmaUI::Surface, FigmaUI::Outline, 8, FMargin(12, 9));
        auto* Body = WidgetTree->ConstructWidget<UHorizontalBox>(); Row.Card->SetContent(Body);
        Row.IconCard = WidgetTree->ConstructWidget<UBorder>(); FigmaUI::Panel(Row.IconCard, FigmaUI::Inset, FigmaUI::Outline, 7, FMargin(8));
        Row.IconCard->SetHorizontalAlignment(HAlign_Center); Row.IconCard->SetVerticalAlignment(VAlign_Center);
        Row.IconCard->SetContent(FigmaUI::Icon(WidgetTree, TEXT("drone"), FVector2D(37, 37))); Body->AddChildToHorizontalBox(Sized(WidgetTree, Row.IconCard, 58, 58))->SetVerticalAlignment(VAlign_Center);
        auto* Details = WidgetTree->ConstructWidget<UVerticalBox>(); auto* DetailsSlot = Body->AddChildToHorizontalBox(Details); DetailsSlot->SetSize(Fill()); DetailsSlot->SetPadding(FMargin(20, 2, 10, 0)); DetailsSlot->SetVerticalAlignment(VAlign_Center);
        auto* Top = WidgetTree->ConstructWidget<UHorizontalBox>(); Details->AddChildToVerticalBox(Top);
        Row.Name = MakeText(FText::GetEmpty(), 15); Row.Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis); Top->AddChildToHorizontalBox(Row.Name)->SetSize(Fill());
        Row.Badge = WidgetTree->ConstructWidget<UBorder>(); FigmaUI::Panel(Row.Badge, FLinearColor(FColor(44, 56, 76)), FLinearColor::Transparent, 12, FMargin(10, 2)); Row.State = MakeText(FText::GetEmpty(), 10); Row.Badge->SetContent(Row.State); Top->AddChildToHorizontalBox(Row.Badge)->SetPadding(FMargin(8, 0));
        Row.Detail = MakeText(FText::GetEmpty(), 12); FigmaUI::Text(Row.Detail, 12, FigmaUI::Muted); Details->AddChildToVerticalBox(Row.Detail)->SetPadding(FMargin(0, 12, 0, 0));
        Row.Toggle = MakeAction(TEXT("toggle"), TEXT("Figma.Fleet.Join"), Id);
        auto* ToggleContent = Cast<UHorizontalBox>(Row.Toggle->GetContent());
        Row.Check = FigmaUI::Icon(WidgetTree, TEXT("check"), FVector2D(18, 18));
        Row.CheckBox = WidgetTree->ConstructWidget<UBorder>(); FigmaUI::Panel(Row.CheckBox, FLinearColor::Transparent, FigmaUI::Muted, 2, FMargin(1)); Row.CheckBox->SetContent(Row.Check);
        ToggleContent->InsertChildAt(0, Sized(WidgetTree, Row.CheckBox, 21, 21));
        if (auto* TextSlot = Cast<UHorizontalBoxSlot>(ToggleContent->GetChildAt(1)->Slot)) TextSlot->SetPadding(FMargin(8, 0, 0, 0));
        Body->AddChildToHorizontalBox(Row.Toggle)->SetVerticalAlignment(VAlign_Center);
        Row.Primary = MakeAction(TEXT("primary"), TEXT("Figma.Fleet.SetPrimary"), Id); Row.PrimaryLabel = Cast<UTextBlock>(Cast<UHorizontalBox>(Row.Primary->GetContent())->GetChildAt(0));
        auto* PrimarySlot = Body->AddChildToHorizontalBox(Sized(WidgetTree, Row.Primary, 106, 36)); PrimarySlot->SetVerticalAlignment(VAlign_Center); PrimarySlot->SetPadding(FMargin(12, 0));
        Body->AddChildToHorizontalBox(MakeAction(TEXT("label"), TEXT("Figma.Fleet.Label"), Id, TEXT("edit")))->SetVerticalAlignment(VAlign_Center);
        auto* RowSlot = Cast<UScrollBoxSlot>(DroneScroll->AddChild(Sized(WidgetTree, Row.Card, 0, 78))); RowSlot->SetPadding(FMargin(0, 0, 9, 10)); Rows.Add(Id, Row);
    }
    if (Descriptors.IsEmpty()) DroneScroll->AddChild(EmptyList);
}

void UFigmaFleetWidget::RefreshVideoOptions(const TArray<FDroneDescriptor>& Descriptors)
{
    auto* Registry = GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    TMap<int32, FString> CurrentNames;
    for (const auto& Descriptor : Descriptors)
    {
        FDroneLabelSettings Label; Registry->GetDroneLabelSettings(Descriptor.DroneId, Label);
        const FString DisplayName = Label.DisplayName.IsEmpty() ? Descriptor.Name : Label.DisplayName;
        CurrentNames.Add(Descriptor.DroneId, DisplayName);
    }
    bool Changed = CurrentNames.Num() != VideoDisplayNames.Num();
    for (const auto& Entry : CurrentNames)
    {
        const FString* Previous = VideoDisplayNames.Find(Entry.Key);
        Changed |= !Previous || *Previous != Entry.Value;
    }
    if (!Changed) return;
    const int32 SelectedId = VideoOptions.FindRef(VideoChoice->GetSelectedOption());
    VideoDisplayNames = CurrentNames; VideoOptions.Empty(); VideoChoice->ClearOptions();
    FString SelectedOption;
    for (const auto& Descriptor : Descriptors)
    {
        const FString& DisplayName = CurrentNames[Descriptor.DroneId];
        // A stable leading identity prevents an editable name from colliding with another option.
        const FString Option = FString::Printf(TEXT("UAV-%02d · %s"), Descriptor.DroneId, *DisplayName);
        VideoOptions.Add(Option, Descriptor.DroneId); VideoChoice->AddOption(Option);
        if (Descriptor.DroneId == SelectedId) SelectedOption = Option;
    }
    if (!SelectedOption.IsEmpty()) VideoChoice->SetSelectedOption(SelectedOption);
    else if (!Descriptors.IsEmpty()) VideoChoice->SetSelectedIndex(0);
}

void UFigmaFleetWidget::SetSelectionStatus(const FText& Text)
{
    SelectionStatus->SetText(Text);
    SelectionStatus->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UFigmaFleetWidget::Refresh()
{
    if (!DroneScroll || !GetGameInstance()) return;
    auto* Registry = GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>(); auto* Sync = GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    if (!Registry || !Sync) return;
    auto Descriptors = Registry->GetFriendlyDroneDescriptors(); Descriptors.Sort([](const FDroneDescriptor& A, const FDroneDescriptor& B) { return (A.Slot > 0 ? A.Slot : A.DroneId) < (B.Slot > 0 ? B.Slot : B.DroneId); });
    TArray<int32> CurrentIds; for (const auto& Descriptor : Descriptors) CurrentIds.Add(Descriptor.DroneId);
    if (CurrentIds != DroneIds || (Rows.IsEmpty() && DroneScroll->GetChildrenCount() == 0)) BuildRows(Descriptors);
    RefreshVideoOptions(Descriptors);
    const int32 Primary = Registry->GetPrimarySelectedDrone();
    if (bSelectionPending)
    {
        auto AcceptedSelection = Registry->GetMultiSelectedDrones(); auto ExpectedSelection = PendingSelection; auto PreviousSelection = SelectionBefore;
        AcceptedSelection.Sort(); ExpectedSelection.Sort(); PreviousSelection.Sort();
        if (Primary == PendingPrimary && AcceptedSelection == ExpectedSelection) { bSelectionPending = false; SetSelectionStatus(FText::GetEmpty()); }
        else if (!Sync->IsReady()) { bSelectionPending = false; SetSelectionStatus(T(TEXT("Errors.SYNC_OFFLINE"))); }
        else if (Sync->GetVersion() > SelectionRequestVersion && (Primary != SelectionBeforePrimary || AcceptedSelection != PreviousSelection))
        {
            bSelectionPending = false; SetSelectionStatus(T(TEXT("Figma.Fleet.SelectionSuperseded")));
        }
        else if (FPlatformTime::Seconds() - SelectionRequestedAt > 10.)
        {
            // SetLocalSelection does not expose non-null rejection replies. Keep the accepted
            // registry snapshot and allow retry instead of locking these controls indefinitely.
            bSelectionPending = false; SetSelectionStatus(T(TEXT("Errors.SYNC_FAILED")));
        }
    }
    const bool CanSelect = (!Sync->IsEnabled() || Sync->IsHydrated()) && !bSelectionPending;
    for (const auto& Descriptor : Descriptors)
    {
        auto* Row = Rows.Find(Descriptor.DroneId); if (!Row) continue;
        FDroneTelemetrySnapshot Telemetry; Registry->GetTelemetry(Descriptor.DroneId, Telemetry);
        FDroneLabelSettings Label; Registry->GetDroneLabelSettings(Descriptor.DroneId, Label);
        Row->Name->SetText(User(Label.DisplayName.IsEmpty() ? Descriptor.Name : Label.DisplayName));
        const bool IsPrimary = Primary == Descriptor.DroneId, InGroup = Registry->IsDroneSelected(Descriptor.DroneId);
        FigmaUI::Panel(Row->Card, IsPrimary ? FLinearColor(FColor(35, 27, 65)) : FigmaUI::Inset, IsPrimary ? FigmaUI::Accent : FigmaUI::Outline, 8, FMargin(12, 9));
        FigmaUI::Panel(Row->IconCard, IsPrimary ? FigmaUI::Accent : FLinearColor(FColor(16, 25, 34)), IsPrimary ? FigmaUI::Accent : FigmaUI::Outline, 7, FMargin(8));
        FigmaUI::Panel(Row->Badge, IsPrimary ? FigmaUI::Accent : FLinearColor(FColor(44, 56, 76)), FLinearColor::Transparent, 12, FMargin(10, 2));
        Row->State->SetText(IsPrimary ? T(TEXT("Fleet.Primary")) : Connection(Telemetry.Availability));
        const FText Battery = Telemetry.BatteryPercent >= 0 ? User(FString::Printf(TEXT("%d%%"), Telemetry.BatteryPercent)) : User(TEXT("N/A"));
        Row->Detail->SetText(FText::Format(T(TEXT("Figma.Fleet.RowDetail")), Battery, Task(Telemetry.TaskState)));
        Row->Check->SetVisibility(InGroup ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
        FigmaUI::Panel(Row->CheckBox, InGroup ? FigmaUI::Accent : FLinearColor::Transparent, InGroup ? FigmaUI::Accent : FigmaUI::Muted, 2, FMargin(1));
        Row->Toggle->SetIsEnabled(CanSelect); Row->Primary->SetIsEnabled(CanSelect);
        FigmaUI::Button(Row->Primary, false, true);
        auto PrimaryStyle = Row->Primary->GetStyle(); PrimaryStyle.SetNormal(FSlateRoundedBoxBrush(IsPrimary ? FigmaUI::Accent : FLinearColor(FColor(30, 36, 51)), 6)); Row->Primary->SetStyle(PrimaryStyle);
    }
    FDroneTelemetrySnapshot Current; const bool HasTelemetry = Registry->GetTelemetry(Primary, Current);
    const bool Fresh = HasTelemetry && Current.Availability == EDroneAvailability::Online;
    TelemetryState->SetText(Primary > 0 ? Connection(Current.Availability) : T(TEXT("Figma.Fleet.NoSelection")));
    if (TelemetryValues.Num() == 5)
    {
        TelemetryValues[0]->SetText(Fresh && Current.bGpsFix ? User(FString::Printf(TEXT("%.1f m"), Current.GpsAltitude)) : User(TEXT("N/A")));
        TelemetryValues[1]->SetText(Fresh ? User(FString::Printf(TEXT("%.1f m/s"), Current.Velocity.Size())) : User(TEXT("N/A")));
        TelemetryValues[2]->SetText(Fresh && Current.BatteryPercent >= 0 ? User(FString::Printf(TEXT("%d%%"), Current.BatteryPercent)) : User(TEXT("N/A")));
        TelemetryValues[3]->SetText(HasTelemetry ? Connection(Current.Availability) : User(TEXT("N/A")));
        TelemetryValues[4]->SetText(HasTelemetry ? Task(Current.TaskState) : User(TEXT("N/A")));
    }
    GroupControls->SetSelectionPending(bSelectionPending); GroupControls->Refresh();
    const int32* Candidate = VideoOptions.Find(VideoChoice->GetSelectedOption());
    VideoOpen->SetIsEnabled(Candidate && Sync->IsHydrated());
    const FString Target = PlanUI::Field(Sync->GetVideoView(), TEXT("video_target_uav_id"));
    FDroneTelemetrySnapshot VideoTelemetry; bool KnownTarget = false;
    for (const auto& Descriptor : Descriptors)
        if (FString::Printf(TEXT("UAV-%02d"), Descriptor.DroneId) == Target) { KnownTarget = Registry->GetTelemetry(Descriptor.DroneId, VideoTelemetry); break; }
    VideoEmptyIcon->SetVisibility(Target.IsEmpty() || (KnownTarget && VideoTelemetry.Availability != EDroneAvailability::Online) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (Target.IsEmpty())
    {
        VideoTitle->SetText(T(TEXT("Figma.Fleet.NoVideo"))); VideoHint->SetText(T(TEXT("Figma.Fleet.VideoHint")));
    }
    else if (KnownTarget && VideoTelemetry.Availability != EDroneAvailability::Online)
    {
        VideoTitle->SetText(T(TEXT("Figma.Fleet.NoVideo"))); VideoHint->SetText(T(TEXT("Figma.Fleet.VideoOffline")));
    }
    else
    {
        // Command owns the target selector, not the Video client's media player or signal state.
        VideoTitle->SetText(T(TEXT("Figma.Fleet.VideoClient"))); VideoHint->SetText(FText::Format(T(TEXT("Figma.Fleet.VideoTargetHint")), User(Target)));
    }
    if (!Sync->ViewSyncError.IsEmpty()) VideoHint->SetText(ProductText::Get(TEXT("Errors.") + Sync->ViewSyncError));
}

void UFigmaFleetWidget::Action(FName Name, int32 Id)
{
    auto* Registry = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>() : nullptr;
    auto* Sync = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>() : nullptr;
    if (!Registry || !Sync) return;
    if (Name == TEXT("label")) { OpenLabelDialog(Id); return; }
    if (Name == TEXT("label_cancel")) { LabelLayer->SetVisibility(ESlateVisibility::Collapsed); EditingId = 0; return; }
    if (Name == TEXT("color")) { if (Id >= 0 && Id < UE_ARRAY_COUNT(LabelColors)) DraftLabel.LabelColor = LabelColors[Id]; RefreshLabelPreview(); return; }
    if (Name == TEXT("label_confirm"))
    {
        DraftLabel.DisplayName = LabelName->GetText().ToString().TrimStartAndEnd();
        if (EditingId > 0 && Registry->IsDroneRegistered(EditingId) && !DraftLabel.DisplayName.IsEmpty())
        {
            Registry->SetDroneLabelSettings(EditingId, DraftLabel);
            LabelLayer->SetVisibility(ESlateVisibility::Collapsed); EditingId = 0; Refresh();
        }
        return;
    }
    if (Name == TEXT("video"))
    {
        if (const int32* Candidate = VideoOptions.Find(VideoChoice->GetSelectedOption()))
            if (Sync->IsHydrated()) Sync->OpenVideo(FString::Printf(TEXT("UAV-%02d"), *Candidate));
        Refresh(); return;
    }
    FDroneDescriptor Descriptor;
    if ((Name != TEXT("primary") && Name != TEXT("toggle")) || !Registry->GetDroneDescriptor(Id, Descriptor) || Descriptor.bIsEnemyTarget) return;
    if (bSelectionPending) return;
    if (Sync->IsEnabled() && !Sync->IsHydrated()) { SetSelectionStatus(T(TEXT("Errors.SYNC_OFFLINE"))); return; }
    auto Selected = Registry->GetMultiSelectedDrones(); int32 Primary = Registry->GetPrimarySelectedDrone();
    if (Name == TEXT("primary")) { Selected.AddUnique(Id); Primary = Id; }
    else if (Selected.Contains(Id)) { Selected.Remove(Id); if (Primary == Id) Primary = Selected.IsEmpty() ? 0 : Selected[0]; }
    else { Selected.Add(Id); if (Primary <= 0) Primary = Id; }
    // Selection commits through the existing synchronization API; it never changes video_target_uav_id.
    if (Sync->IsEnabled())
    {
        SelectionBeforePrimary = Registry->GetPrimarySelectedDrone(); SelectionBefore = Registry->GetMultiSelectedDrones();
        SelectionRequestVersion = Sync->GetVersion(); SelectionRequestedAt = FPlatformTime::Seconds();
        bSelectionPending = Sync->SetLocalSelection(Primary, Selected); PendingPrimary = Primary; PendingSelection = Selected;
        SetSelectionStatus(T(bSelectionPending ? TEXT("Fleet.SelectionPending") : TEXT("Errors.SYNC_OFFLINE")));
    }
    else { Registry->SetMultiSelectedDrones(Selected); Registry->SetPrimarySelectedDrone(Primary); SetSelectionStatus(FText::GetEmpty()); }
    Refresh();
}

void UFigmaFleetWidget::BuildLabelDialog()
{
    LabelLayer = WidgetTree->ConstructWidget<UBorder>(); LabelLayer->SetBrushColor(FLinearColor(0, 0, 0, .73f)); LabelLayer->SetPadding(FMargin(0));
    LabelLayer->SetHorizontalAlignment(HAlign_Center); LabelLayer->SetVerticalAlignment(VAlign_Center);
    auto* LayerSlot = RootOverlay->AddChildToOverlay(LabelLayer); LayerSlot->SetHorizontalAlignment(HAlign_Fill); LayerSlot->SetVerticalAlignment(VAlign_Fill);
    auto* Card = WidgetTree->ConstructWidget<UBorder>(); FigmaUI::Panel(Card, FLinearColor(FColor(25, 27, 35)), FigmaUI::Outline, 12, FMargin(30, 22));
    LabelLayer->SetContent(Sized(WidgetTree, Card, 520, 594));
    auto* Body = WidgetTree->ConstructWidget<UVerticalBox>(); Card->SetContent(Body);
    auto* Header = WidgetTree->ConstructWidget<UHorizontalBox>(); Body->AddChildToVerticalBox(Header)->SetPadding(FMargin(0, 0, 0, 26));
    Header->AddChildToHorizontalBox(MakeText(T(TEXT("Figma.Fleet.LabelTitle")), 23, true))->SetSize(Fill());
    Header->AddChildToHorizontalBox(MakeAction(TEXT("label_cancel"), TEXT(""), 0, TEXT("close")))->SetVerticalAlignment(VAlign_Center);
    Body->AddChildToVerticalBox(MakeText(T(TEXT("Figma.Fleet.LabelName")), 15, true))->SetPadding(FMargin(0, 0, 0, 10));
    LabelName = WidgetTree->ConstructWidget<UEditableTextBox>(); FigmaUI::Input(LabelName); LabelName->OnTextChanged.AddDynamic(this, &UFigmaFleetWidget::LabelNameChanged);
    Body->AddChildToVerticalBox(Sized(WidgetTree, LabelName, 0, 38))->SetPadding(FMargin(0, 0, 0, 24));
    Body->AddChildToVerticalBox(MakeText(T(TEXT("Figma.Fleet.LabelColor")), 15, true))->SetPadding(FMargin(0, 0, 0, 20));
    auto* Swatches = WidgetTree->ConstructWidget<UHorizontalBox>(); Body->AddChildToVerticalBox(Swatches)->SetPadding(FMargin(0, 0, 0, 26));
    for (int32 I = 0; I < UE_ARRAY_COUNT(LabelColors); ++I)
    {
        auto* Button = MakeAction(TEXT("color"), TEXT(""), I); auto Style = Button->GetStyle();
        Style.SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent, 0)); Style.SetHovered(Style.Normal); Style.SetPressed(Style.Normal); Style.SetNormalPadding(FMargin(0)); Style.SetPressedPadding(FMargin(0)); Button->SetStyle(Style);
        auto* Swatch = WidgetTree->ConstructWidget<UOverlay>(); Button->SetContent(Swatch);
        auto* Color = FigmaUI::Icon(WidgetTree, FString::Printf(TEXT("label-color-%d"), I), FVector2D(53, 53));
        auto* ColorSlot = Swatch->AddChildToOverlay(Color); ColorSlot->SetHorizontalAlignment(HAlign_Center); ColorSlot->SetVerticalAlignment(VAlign_Center);
        auto* Ring = FigmaUI::Icon(WidgetTree, TEXT("label-color-ring"), FVector2D(59, 59)); ColorRings.Add(Ring); auto* RingSlot = Swatch->AddChildToOverlay(Ring); RingSlot->SetHorizontalAlignment(HAlign_Center); RingSlot->SetVerticalAlignment(VAlign_Center);
        auto* Check = FigmaUI::Icon(WidgetTree, TEXT("check-dark"), FVector2D(24, 24)); ColorChecks.Add(Check);
        auto* CheckSlot = Swatch->AddChildToOverlay(Check); CheckSlot->SetHorizontalAlignment(HAlign_Center); CheckSlot->SetVerticalAlignment(VAlign_Center);
        auto* SwatchSlot = Swatches->AddChildToHorizontalBox(Sized(WidgetTree, Button, 57, 57)); SwatchSlot->SetPadding(FMargin(0, 0, I < 5 ? 23 : 0, 0));
    }
    auto* SizeHeader = WidgetTree->ConstructWidget<UHorizontalBox>(); Body->AddChildToVerticalBox(SizeHeader);
    SizeHeader->AddChildToHorizontalBox(MakeText(T(TEXT("Figma.Fleet.LabelSize")), 15, true))->SetSize(Fill()); LabelSizeValue = MakeText(FText::GetEmpty(), 15); SizeHeader->AddChildToHorizontalBox(LabelSizeValue);
    auto* Scope = MakeText(T(TEXT("Figma.Fleet.LabelScope")), 12); FigmaUI::Text(Scope, 12, FigmaUI::Muted); Body->AddChildToVerticalBox(Scope)->SetPadding(FMargin(0, 6, 0, 10));
    auto* SliderStack = WidgetTree->ConstructWidget<UOverlay>();
    LabelSizeProgress = WidgetTree->ConstructWidget<UProgressBar>(); LabelSizeProgress->SetFillColorAndOpacity(FigmaUI::Accent); auto ProgressStyle = LabelSizeProgress->GetWidgetStyle();
    ProgressStyle.BackgroundImage = FSlateRoundedBoxBrush(FLinearColor(FColor(61, 74, 94)), 3); ProgressStyle.FillImage = FSlateRoundedBoxBrush(FLinearColor::White, 3); LabelSizeProgress->SetWidgetStyle(ProgressStyle);
    auto* ProgressSlot = SliderStack->AddChildToOverlay(Sized(WidgetTree, LabelSizeProgress, 0, 6)); ProgressSlot->SetVerticalAlignment(VAlign_Center); ProgressSlot->SetHorizontalAlignment(HAlign_Fill); ProgressSlot->SetPadding(FMargin(10, 0));
    LabelSize = WidgetTree->ConstructWidget<USlider>(); LabelSize->SetMinValue(8); LabelSize->SetMaxValue(72); LabelSize->SetStepSize(2); LabelSize->SetSliderBarColor(FLinearColor::Transparent);
    const auto Thumb = FigmaUI::Icon(WidgetTree, TEXT("label-slider-thumb"), FVector2D(21, 21))->GetBrush();
    auto SliderStyle = LabelSize->GetWidgetStyle(); SliderStyle.NormalThumbImage = Thumb; SliderStyle.HoveredThumbImage = Thumb; SliderStyle.DisabledThumbImage = Thumb;
    LabelSize->SetWidgetStyle(SliderStyle);
    LabelSize->OnValueChanged.AddDynamic(this, &UFigmaFleetWidget::LabelSizeChanged); SliderStack->AddChildToOverlay(LabelSize)->SetHorizontalAlignment(HAlign_Fill);
    Body->AddChildToVerticalBox(SliderStack)->SetPadding(FMargin(0, 0, 0, 24));
    Body->AddChildToVerticalBox(Sized(WidgetTree, FigmaUI::Icon(WidgetTree, TEXT("label-separator"), FVector2D(460, 1)), 460, 1))->SetPadding(FMargin(0, 0, 0, 18));
    Body->AddChildToVerticalBox(MakeText(T(TEXT("Figma.Fleet.LabelPreview")), 15, true))->SetPadding(FMargin(0, 0, 0, 14));
    auto* Preview = WidgetTree->ConstructWidget<UHorizontalBox>(); Body->AddChildToVerticalBox(Preview)->SetSize(Fill());
    auto* IconCard = WidgetTree->ConstructWidget<UBorder>(); FigmaUI::Panel(IconCard, FLinearColor::Transparent, FigmaUI::Outline, 7, FMargin(7));
    IconCard->SetHorizontalAlignment(HAlign_Center); IconCard->SetVerticalAlignment(VAlign_Center);
    IconCard->SetContent(FigmaUI::Icon(WidgetTree, TEXT("drone-preview"), FVector2D(38, 38)));
    Preview->AddChildToHorizontalBox(Sized(WidgetTree, IconCard, 55, 55))->SetVerticalAlignment(VAlign_Top);
    LabelPreview = MakeText(FText::GetEmpty(), 16); auto* PreviewSlot = Preview->AddChildToHorizontalBox(LabelPreview); PreviewSlot->SetPadding(FMargin(22, 10, 0, 0)); PreviewSlot->SetSize(Fill()); LabelPreview->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    Body->AddChildToVerticalBox(Sized(WidgetTree, FigmaUI::Icon(WidgetTree, TEXT("label-separator"), FVector2D(460, 1)), 460, 1));
    auto* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(); Body->AddChildToVerticalBox(Actions)->SetPadding(FMargin(0, 14, 0, 0));
    auto* Cancel = MakeAction(TEXT("label_cancel"), TEXT("Common.Cancel"));
    auto CancelStyle = Cancel->GetStyle(); CancelStyle.SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent, 7, FigmaUI::Muted, 1)); CancelStyle.SetHovered(FSlateRoundedBoxBrush(FigmaUI::Surface, 7, FigmaUI::PrimaryText, 1)); Cancel->SetStyle(CancelStyle);
    Actions->AddChildToHorizontalBox(Sized(WidgetTree, Cancel, 192, 48));
    auto* Space = WidgetTree->ConstructWidget<USpacer>(); Actions->AddChildToHorizontalBox(Space)->SetSize(Fill());
    LabelConfirm = MakeAction(TEXT("label_confirm"), TEXT("Figma.Fleet.Confirm")); FigmaUI::Button(LabelConfirm, true);
    auto ConfirmStyle = LabelConfirm->GetStyle(); ConfirmStyle.SetNormal(FSlateRoundedBoxBrush(FigmaUI::PrimaryText, 7)); ConfirmStyle.SetHovered(FSlateRoundedBoxBrush(FigmaUI::PrimaryText, 7, FigmaUI::Accent, 1)); LabelConfirm->SetStyle(ConfirmStyle);
    Actions->AddChildToHorizontalBox(Sized(WidgetTree, LabelConfirm, 192, 48));
    LabelLayer->SetVisibility(ESlateVisibility::Collapsed);
}

void UFigmaFleetWidget::OpenLabelDialog(int32 Id)
{
    auto* Registry = GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    if (!Registry || !Registry->IsDroneRegistered(Id)) return;
    EditingId = Id; Registry->GetDroneLabelSettings(Id, DraftLabel); DraftLabel.FontSize = FMath::Clamp(DraftLabel.FontSize, 8, 72);
    LabelName->SetText(User(DraftLabel.DisplayName)); LabelSize->SetValue(DraftLabel.FontSize); RefreshLabelPreview(); LabelLayer->SetVisibility(ESlateVisibility::Visible); LabelName->SetKeyboardFocus();
}

void UFigmaFleetWidget::LabelNameChanged(const FText&) { RefreshLabelPreview(); }
UWidget* UFigmaFleetWidget::GenerateVideoOption(FString Option) { auto* Label = MakeText(User(Option), 11); Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis); Label->SetToolTipText(User(Option)); return Label; }
void UFigmaFleetWidget::LabelSizeChanged(float Value) { DraftLabel.FontSize = FMath::Clamp(FMath::RoundToInt(Value / 2.f) * 2, 8, 72); RefreshLabelPreview(); }
void UFigmaFleetWidget::RefreshLabelPreview()
{
    if (!LabelPreview || !LabelName || !LabelSizeValue || !LabelConfirm) return;
    const FString Name = LabelName->GetText().ToString().TrimStartAndEnd(); LabelPreview->SetText(User(Name)); FigmaUI::Text(LabelPreview, DraftLabel.FontSize, DraftLabel.LabelColor);
    LabelSizeValue->SetText(User(FString::Printf(TEXT("%d PX"), DraftLabel.FontSize))); LabelConfirm->SetIsEnabled(!Name.IsEmpty());
    if (LabelSizeProgress) LabelSizeProgress->SetPercent((DraftLabel.FontSize - 8.f) / 64.f);
    for (int32 I = 0; I < ColorRings.Num(); ++I)
    {
        const bool Selected = LabelColors[I].Equals(DraftLabel.LabelColor, .01f);
        ColorRings[I]->SetVisibility(Selected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden); ColorChecks[I]->SetVisibility(Selected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
}
