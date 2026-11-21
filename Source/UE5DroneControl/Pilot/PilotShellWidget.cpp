#include "Pilot/PilotShellWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Command/FigmaCommandStyle.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/PilotControlSubsystem.h"
#include "WebBrowser.h"
#include "Widgets/SWindow.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <mmsystem.h>
#include <Xinput.h>
#include "Windows/HideWindowsPlatformTypes.h"
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "xinput.lib")
#endif

#define LOCTEXT_NAMESPACE "PilotShell"

namespace
{
// Used only by the Mock desktop client. Physical flight limits are set separately.
constexpr double MockFrameFreshness = 2.0;
constexpr double VideoProbePeriod = 0.35;
constexpr double InputPollPeriod = 0.05;
constexpr double MockForwardPulse = 0.25;
constexpr float NeutralDeadZone = 0.12f;

FString NormalizeVideoUrl(FString Url)
{
    Url.TrimStartAndEndInline();
    Url.RemoveFromEnd(TEXT("/"));
    return Url;
}

FText StatusText(const FString& Code)
{
    if (Code == TEXT("NOT_CONFIGURED")) return LOCTEXT("NotConfigured", "驾驶员身份未配置");
    if (Code == TEXT("CREDENTIALS_INVALID")) return LOCTEXT("Credentials", "驾驶员凭据无效");
    if (Code == TEXT("BACKEND_UNAVAILABLE")) return LOCTEXT("Backend", "后端连接中断");
    if (Code == TEXT("BACKEND_NOT_MOCK")) return LOCTEXT("BackendNotMock", "后端尚未启用驾驶员 Mock");
    if (Code == TEXT("CONNECTING")) return LOCTEXT("Connecting", "正在连接后端");
    if (Code == TEXT("REQUEST_PENDING")) return LOCTEXT("Pending", "等待后端确认");
    if (Code == TEXT("REQUEST_UNCERTAIN")) return LOCTEXT("Uncertain", "请求结果待确认");
    if (Code == TEXT("GRANT_REVOKED")) return LOCTEXT("Revoked", "控制权已收回");
    if (Code == TEXT("GRANT_CHANGED")) return LOCTEXT("GrantChanged", "控制权已变更，请重新选择");
    if (Code == TEXT("INPUT_TIMEOUT") || Code == TEXT("INPUT_REJECTED") || Code == TEXT("SESSION_EXPIRED")) return LOCTEXT("Timeout", "输入会话已失效，请重新启用");
    if (Code == TEXT("VIDEO_STALE")) return LOCTEXT("VideoLost", "图传停止更新，已退出操控");
    if (Code == TEXT("DEVICE_LOST")) return LOCTEXT("DeviceLost", "输入设备断开，已退出操控");
    if (Code == TEXT("WINDOW_INACTIVE")) return LOCTEXT("FocusLost", "驾驶窗口失去焦点，已退出操控");
    if (Code == TEXT("PILOT_BUSY")) return LOCTEXT("PilotBusy", "此驾驶员已有操控会话");
    if (Code == TEXT("UAV_BUSY")) return LOCTEXT("AircraftBusy", "飞机被其他任务占用");
    if (Code == TEXT("UAV_NOT_AVAILABLE") || Code == TEXT("MOCK_UAV_UNAVAILABLE")) return LOCTEXT("AircraftUnavailable", "飞机当前不可用");
    if (Code == TEXT("STALE_STATE")) return LOCTEXT("StaleState", "飞机状态已更新，请检查后重试");
    if (Code == TEXT("OPERATOR_STOP") || Code == TEXT("STOPPED")) return LOCTEXT("OperatorStop", "已停止操控，可选择下一架飞机");
    if (Code == TEXT("LINK_LOST")) return LOCTEXT("LinkLost", "原操控会话已断开，请重新启用");
    return FText::AsCultureInvariant(Code);
}

#if PLATFORM_WINDOWS
bool ReadGamepad(int32 Index, FPilotControlAxes& Axes)
{
    XINPUT_STATE State{};
    if (XInputGetState(Index, &State) != ERROR_SUCCESS) return false;
    const auto Normalize = [](int16 Value) { return Value / (Value < 0 ? 32768.f : 32767.f); };
    const auto& Pad = State.Gamepad;
    Axes = {Normalize(Pad.sThumbRY), Normalize(Pad.sThumbRX), Normalize(Pad.sThumbLY), Normalize(Pad.sThumbLX)};
    return true;
}

bool ReadAxis(const FString& Name, const JOYCAPSW& Caps, const JOYINFOEX& Data, float& Result)
{
    uint32 Value = 0, Minimum = 0, Maximum = 0;
    if (Name == TEXT("X")) { Value = Data.dwXpos; Minimum = Caps.wXmin; Maximum = Caps.wXmax; }
    else if (Name == TEXT("Y")) { Value = Data.dwYpos; Minimum = Caps.wYmin; Maximum = Caps.wYmax; }
    else if (Name == TEXT("Z") && (Caps.wCaps & JOYCAPS_HASZ)) { Value = Data.dwZpos; Minimum = Caps.wZmin; Maximum = Caps.wZmax; }
    else if (Name == TEXT("R") && (Caps.wCaps & JOYCAPS_HASR)) { Value = Data.dwRpos; Minimum = Caps.wRmin; Maximum = Caps.wRmax; }
    else if (Name == TEXT("U") && (Caps.wCaps & JOYCAPS_HASU)) { Value = Data.dwUpos; Minimum = Caps.wUmin; Maximum = Caps.wUmax; }
    else if (Name == TEXT("V") && (Caps.wCaps & JOYCAPS_HASV)) { Value = Data.dwVpos; Minimum = Caps.wVmin; Maximum = Caps.wVmax; }
    if (Maximum <= Minimum) return false;
    const float Normalized = 2.0f * (static_cast<float>(Value) - Minimum) / (Maximum - Minimum) - 1.0f;
    Result = FMath::Clamp(Normalized, -1.0f, 1.0f);
    return true;
}
#endif
}

class FPilotKeyboardInputProcessor final : public IInputProcessor
{
public:
    explicit FPilotKeyboardInputProcessor(UPilotShellWidget* InWidget) : Widget(InWidget) {}

    void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override
    {
        if (const auto* Pilot = Widget.Get(); Pilot && Pilot->IsWindowActive()) return;
        Pressed.Reset();
        if (auto* Pilot = Widget.Get()) Pilot->RequestStop(TEXT("WINDOW_INACTIVE"));
    }

    bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
    {
        const auto* Pilot = Widget.Get();
        const FKey Key = Event.GetKey();
        if (!Pilot || !Pilot->IsWindowActive() || !IsAxisKey(Key)) return false;
        if (!Event.IsRepeat()) Pressed.Add(Key);
        return IsFlying(*Pilot);
    }

    bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& Event) override
    {
        const auto* Pilot = Widget.Get();
        const bool bTracked = Pressed.Remove(Event.GetKey()) > 0;
        return bTracked && Pilot && Pilot->IsWindowActive() && IsFlying(*Pilot);
    }

    bool IsNeutral() const { return Pressed.IsEmpty(); }

    FPilotControlAxes GetAxes() const
    {
        return {Axis(EKeys::W, EKeys::S), Axis(EKeys::D, EKeys::A), Axis(EKeys::SpaceBar, EKeys::LeftControl), Axis(EKeys::E, EKeys::Q)};
    }

private:
    static bool IsFlying(const UPilotShellWidget& Pilot)
    {
        return Pilot.Control && !Pilot.SelectedUavId.IsEmpty() && Pilot.Control->GetActiveUavId() == Pilot.SelectedUavId;
    }

    static bool IsAxisKey(const FKey& Key)
    {
        return Key == EKeys::W || Key == EKeys::S || Key == EKeys::A || Key == EKeys::D || Key == EKeys::Q || Key == EKeys::E ||
            Key == EKeys::SpaceBar || Key == EKeys::LeftControl;
    }

    float Axis(const FKey& Positive, const FKey& Negative) const
    {
        return (Pressed.Contains(Positive) ? 1.f : 0.f) - (Pressed.Contains(Negative) ? 1.f : 0.f);
    }

    TWeakObjectPtr<UPilotShellWidget> Widget;
    TSet<FKey> Pressed;
};

TSharedRef<SWidget> UPilotShellWidget::RebuildWidget()
{
    if (!WidgetTree->RootWidget) BuildPresentation();
    return Super::RebuildWidget();
}

void UPilotShellWidget::NativeConstruct()
{
    Super::NativeConstruct();
    bMockInput = FParse::Param(FCommandLine::Get(), TEXT("PilotMockInput"));
    if (FParse::Param(FCommandLine::Get(), TEXT("PilotKeyboardInput")) && FSlateApplication::IsInitialized())
    {
        KeyboardInput = MakeShared<FPilotKeyboardInputProcessor>(this);
        FSlateApplication::Get().RegisterInputPreProcessor(KeyboardInput);
        bMockInput = false;
    }
    MockForwardButton->SetVisibility(bMockInput ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (KeyboardInput)
    {
        InputLabel->SetToolTipText(LOCTEXT("KeyboardHint", "W/S 前后，A/D 左右，空格/左 Ctrl 升降，Q/E 转向"));
    }
    ProbePrefix = TEXT("PILOT-") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(":");
    if (GConfig)
    {
        const TCHAR* Section = TEXT("PilotJoystick");
        bPreferWinMM = GConfig->GetInt(Section, TEXT("DeviceIndex"), JoystickIndex, GGameIni);
        const TCHAR* NameKeys[] = {TEXT("ForwardAxis"), TEXT("RightAxis"), TEXT("VerticalAxis"), TEXT("YawAxis")};
        const TCHAR* InvertKeys[] = {TEXT("InvertForward"), TEXT("InvertRight"), TEXT("InvertVertical"), TEXT("InvertYaw")};
        for (int32 Index = 0; Index < 4; ++Index)
        {
            bPreferWinMM |= GConfig->GetString(Section, NameKeys[Index], AxisNames[Index], GGameIni);
            AxisNames[Index].TrimStartAndEndInline();
            AxisNames[Index].ToUpperInline();
            bool bInverted = InvertAxis[Index];
            bPreferWinMM |= GConfig->GetBool(Section, InvertKeys[Index], bInverted, GGameIni);
            InvertAxis[Index] = bInverted;
        }
    }
    if (const UGameInstance* GameInstance = GetGameInstance())
    {
        Control = GameInstance->GetSubsystem<UPilotControlSubsystem>();
        if (Control) Control->OnChanged.AddUObject(this, &UPilotShellWidget::RefreshAircraft);
    }
    RefreshAircraft();
}

void UPilotShellWidget::NativeDestruct()
{
    if (KeyboardInput && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(KeyboardInput);
    KeyboardInput.Reset();
    SessionInput = {};
    if (Control)
    {
        Control->OnChanged.RemoveAll(this);
        RequestStop(TEXT("WINDOW_CLOSED"));
    }
    SelectedUavId.Reset();
    OpenFeed(FString());
    Super::NativeDestruct();
}

UTextBlock* UPilotShellWidget::MakeLabel(const FText& Text, int32 Size, bool bBold)
{
    auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
    FigmaUI::Text(Label, Size, FigmaUI::PrimaryText, bBold);
    Label->SetText(Text);
    return Label;
}

UButton* UPilotShellWidget::MakeButton(const FText& Text, bool bPrimary)
{
    auto* Button = WidgetTree->ConstructWidget<UButton>();
    Button->SetContent(MakeLabel(Text, 14, true));
    FigmaUI::Button(Button, bPrimary);
    return Button;
}

UWidget* UPilotShellWidget::MakeAircraftOption(FString Item)
{
    return MakeLabel(FText::AsCultureInvariant(Item), 14);
}

void UPilotShellWidget::BuildPresentation()
{
    auto* Root = WidgetTree->ConstructWidget<UBorder>();
    WidgetTree->RootWidget = Root;
    Root->SetBrushColor(FigmaUI::Background);
    Root->SetPadding(FMargin(16));
    auto* Column = WidgetTree->ConstructWidget<UVerticalBox>();
    Root->SetContent(Column);

    auto* Header = WidgetTree->ConstructWidget<UBorder>();
    FigmaUI::Panel(Header);
    Column->AddChildToVerticalBox(Header)->SetPadding(FMargin(0, 0, 0, 12));
    auto* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    Header->SetContent(HeaderRow);
    HeaderRow->AddChildToHorizontalBox(MakeLabel(LOCTEXT("Title", "驾驶员端"), 22, true))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    LinkLabel = MakeLabel(LOCTEXT("Connecting", "正在连接后端"), 14);
    HeaderRow->AddChildToHorizontalBox(LinkLabel)->SetVerticalAlignment(VAlign_Center);

    auto* Workspace = WidgetTree->ConstructWidget<UHorizontalBox>();
    Column->AddChildToVerticalBox(Workspace)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* SidePanel = WidgetTree->ConstructWidget<UBorder>();
    FigmaUI::Panel(SidePanel);
    auto* SideSize = WidgetTree->ConstructWidget<USizeBox>();
    SideSize->SetWidthOverride(270);
    SideSize->SetContent(SidePanel);
    Workspace->AddChildToHorizontalBox(SideSize)->SetPadding(FMargin(0, 0, 12, 0));
    auto* Side = WidgetTree->ConstructWidget<UVerticalBox>();
    SidePanel->SetContent(Side);

    Side->AddChildToVerticalBox(MakeLabel(LOCTEXT("Assignments", "我的无人机"), 18, true))->SetPadding(FMargin(0, 0, 0, 10));
    AircraftPicker = WidgetTree->ConstructWidget<UComboBoxString>();
    AircraftPicker->SetToolTipText(LOCTEXT("SelectAircraft", "选择一架已分配的无人机"));
    auto PickerStyle = AircraftPicker->GetWidgetStyle();
    auto& PickerButton = PickerStyle.ComboButtonStyle.ButtonStyle;
    PickerButton.SetNormal(FSlateRoundedBoxBrush(FigmaUI::Inset, 5, FigmaUI::Outline, 1));
    PickerButton.SetHovered(FSlateRoundedBoxBrush(FigmaUI::Inset, 5, FigmaUI::Accent, 1));
    AircraftPicker->SetWidgetStyle(PickerStyle);
    AircraftPicker->OnGenerateWidgetEvent.BindDynamic(this, &UPilotShellWidget::MakeAircraftOption);
    AircraftPicker->OnSelectionChanged.AddDynamic(this, &UPilotShellWidget::AircraftSelected);
    Side->AddChildToVerticalBox(AircraftPicker)->SetPadding(FMargin(0, 0, 0, 16));
    InputLabel = MakeLabel(FText::GetEmpty(), 14);
    FrameLabel = MakeLabel(FText::GetEmpty(), 14);
    Side->AddChildToVerticalBox(InputLabel)->SetPadding(FMargin(0, 0, 0, 8));
    Side->AddChildToVerticalBox(FrameLabel)->SetPadding(FMargin(0, 0, 0, 14));

    BeginButton = MakeButton(LOCTEXT("Start", "开始操控"), true);
    BeginButton->OnClicked.AddDynamic(this, &UPilotShellWidget::BeginControl);
    Side->AddChildToVerticalBox(BeginButton)->SetPadding(FMargin(0, 0, 0, 8));
    StopButton = MakeButton(LOCTEXT("Stop", "停止操控"));
    StopButton->OnClicked.AddDynamic(this, &UPilotShellWidget::StopControl);
    Side->AddChildToVerticalBox(StopButton)->SetPadding(FMargin(0, 0, 0, 8));
    MockForwardButton = MakeButton(LOCTEXT("MockForward", "模拟前进"));
    MockForwardButton->OnPressed.AddDynamic(this, &UPilotShellWidget::MockForwardPressed);
    MockForwardButton->SetVisibility(ESlateVisibility::Collapsed);
    Side->AddChildToVerticalBox(MockForwardButton)->SetPadding(FMargin(0, 0, 0, 12));

    ActionPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ActionPanel->SetVisibility(ESlateVisibility::Collapsed);
    Side->AddChildToVerticalBox(ActionPanel)->SetPadding(FMargin(0, 4, 0, 4));
    auto* FlightRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    auto* RecoveryRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    ActionPanel->AddChildToVerticalBox(FlightRow)->SetPadding(FMargin(0, 0, 0, 4));
    ActionPanel->AddChildToVerticalBox(RecoveryRow);
    const auto AddActionButton = [this](UHorizontalBox* Row, const FText& Text, bool bLast)
    {
        auto* Button = MakeButton(Text);
        auto* Slot = Row->AddChildToHorizontalBox(Button);
        Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        if (!bLast) Slot->SetPadding(FMargin(0, 0, 4, 0));
        Button->SetIsEnabled(false);
        ActionButtons.Add(Button);
        return Button;
    };
    AddActionButton(FlightRow, LOCTEXT("Takeoff", "起飞"), false)->OnClicked.AddDynamic(this, &UPilotShellWidget::Takeoff);
    AddActionButton(FlightRow, LOCTEXT("Land", "降落"), false)->OnClicked.AddDynamic(this, &UPilotShellWidget::Land);
    AddActionButton(FlightRow, LOCTEXT("Hold", "悬停"), true)->OnClicked.AddDynamic(this, &UPilotShellWidget::Hold);
    AddActionButton(RecoveryRow, LOCTEXT("GoHome", "返航"), false)->OnClicked.AddDynamic(this, &UPilotShellWidget::GoHome);
    auto* EmergencyButton = AddActionButton(RecoveryRow, LOCTEXT("EmergencyStop", "急停"), true);
    EmergencyButton->SetToolTipText(LOCTEXT("EmergencyStopHint", "冻结模拟机运动，停止操控并重新启用后恢复"));
    EmergencyButton->OnClicked.AddDynamic(this, &UPilotShellWidget::EmergencyStop);

    DetailLabel = MakeLabel(FText::GetEmpty(), 14);
    DetailLabel->SetAutoWrapText(true);
    Side->AddChildToVerticalBox(DetailLabel)->SetPadding(FMargin(0, 8, 0, 0));

    auto* CameraPanel = WidgetTree->ConstructWidget<UBorder>();
    FigmaUI::Panel(CameraPanel);
    Workspace->AddChildToHorizontalBox(CameraPanel)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* CameraColumn = WidgetTree->ConstructWidget<UVerticalBox>();
    CameraPanel->SetContent(CameraColumn);
    FeedTitle = MakeLabel(LOCTEXT("Video", "实时图传"), 18, true);
    CameraColumn->AddChildToVerticalBox(FeedTitle)->SetPadding(FMargin(0, 0, 0, 12));
    auto* CameraOverlay = WidgetTree->ConstructWidget<UOverlay>();
    CameraColumn->AddChildToVerticalBox(CameraOverlay)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Browser = WidgetTree->ConstructWidget<UWebBrowser>();
    Browser->OnUrlChanged.AddDynamic(this, &UPilotShellWidget::FeedUrlChanged);
    Browser->OnConsoleMessage.AddDynamic(this, &UPilotShellWidget::FeedConsoleMessage);
    auto* BrowserSlot = CameraOverlay->AddChildToOverlay(Browser);
    BrowserSlot->SetHorizontalAlignment(HAlign_Fill);
    BrowserSlot->SetVerticalAlignment(VAlign_Fill);

    FeedCover = WidgetTree->ConstructWidget<UBorder>();
    FeedCover->SetBrushColor(FigmaUI::Inset);
    FeedCover->SetHorizontalAlignment(HAlign_Center);
    FeedCover->SetVerticalAlignment(VAlign_Center);
    FeedCover->SetVisibility(ESlateVisibility::HitTestInvisible);
    FeedPlaceholder = MakeLabel(LOCTEXT("SelectAircraft", "选择一架已分配的无人机"), 16);
    FeedCover->SetContent(FeedPlaceholder);
    auto* CoverSlot = CameraOverlay->AddChildToOverlay(FeedCover);
    CoverSlot->SetHorizontalAlignment(HAlign_Fill);
    CoverSlot->SetVerticalAlignment(VAlign_Fill);
}

const FPilotControlAircraft* UPilotShellWidget::GetSelectedAircraft() const
{
    return Control ? Control->GetAircraft().FindByPredicate([this](const auto& Row) { return Row.UavId == SelectedUavId; }) : nullptr;
}

void UPilotShellWidget::RefreshAircraft()
{
    if (!AircraftPicker || !Browser) return;
    TArray<FString> NewIds;
    if (Control)
    {
        for (const auto& Row : Control->GetAircraft()) NewIds.Add(Row.UavId);
    }
    if (AircraftIds != NewIds)
    {
        TGuardValue<bool> Guard(bUpdatingPicker, true);
        AircraftIds = MoveTemp(NewIds);
        AircraftPicker->ClearOptions();
        for (const auto& Id : AircraftIds) AircraftPicker->AddOption(Id);
        if (AircraftIds.Contains(SelectedUavId)) AircraftPicker->SetSelectedOption(SelectedUavId);
        else SelectedUavId.Reset();
    }
    const auto* Row = GetSelectedAircraft();
    const FString Url = Row ? Row->VideoUrl : FString();
    if (FeedUavId != SelectedUavId || SourceUrl != Url) OpenFeed(Url);
}

void UPilotShellWidget::AircraftSelected(FString Item, ESelectInfo::Type SelectInfo)
{
    if (bUpdatingPicker || SelectInfo == ESelectInfo::Direct || !Control || !AircraftIds.Contains(Item)) return;
    if (Control->IsRequestPending() || !Control->GetActiveUavId().IsEmpty())
    {
        TGuardValue<bool> Guard(bUpdatingPicker, true);
        AircraftPicker->SetSelectedOption(SelectedUavId);
        return;
    }
    SelectedUavId = MoveTemp(Item);
    RefreshAircraft();
}

void UPilotShellWidget::OpenFeed(const FString& Url)
{
    FeedUavId = SelectedUavId;
    SourceUrl = Url;
    ++FeedGeneration;
    bPageReady = false;
    LastDecodedFrame = -1.0;
    LastFrameAt = LastProbeAt = 0.0;
    LastCompletedProbe = ProbeSerial;
    FeedOpenedAt = FPlatformTime::Seconds();
    if (!Browser || !FeedCover) return;
    FeedCover->SetVisibility(ESlateVisibility::HitTestInvisible);
    const bool bWebFeed = Url.StartsWith(TEXT("http://")) || Url.StartsWith(TEXT("https://"));
    Browser->LoadURL(bWebFeed ? Url : TEXT("about:blank"));
}

void UPilotShellWidget::FeedUrlChanged(const FText& Url)
{
    bPageReady = !SourceUrl.IsEmpty() && NormalizeVideoUrl(Url.ToString()) == NormalizeVideoUrl(SourceUrl);
    LastDecodedFrame = -1.0;
    LastFrameAt = 0.0;
    LastCompletedProbe = ProbeSerial;
}

bool UPilotShellWidget::HasFreshFrames(double Now) const
{
    return bPageReady && LastFrameAt > 0.0 && Now - LastFrameAt <= MockFrameFreshness;
}

void UPilotShellWidget::ProbeFeed(double Now)
{
    if (!bPageReady || Now - LastProbeAt < VideoProbePeriod) return;
    LastProbeAt = Now;
    Browser->ExecuteJavascript(FString::Printf(TEXT(
        "(()=>{const v=document.querySelector('video');const q=v&&v.getVideoPlaybackQuality?v.getVideoPlaybackQuality():null;"
        "console.log('%s'+JSON.stringify({g:%d,s:%d,url:location.href,"
        "f:q?q.totalVideoFrames-(q.droppedVideoFrames||0):v&&v.webkitDecodedFrameCount||0,"
        "p:!v||v.paused||v.ended||!v.videoWidth||!v.videoHeight||!!v.error}));})()"),
        *ProbePrefix, FeedGeneration, ++ProbeSerial));
}

void UPilotShellWidget::FeedConsoleMessage(const FString& Message, const FString& Source, int32 Line)
{
    if (!bPageReady || ProbePrefix.IsEmpty() || !Message.StartsWith(ProbePrefix)) return;
    TSharedPtr<FJsonObject> Payload;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Message.Mid(ProbePrefix.Len())), Payload) || !Payload) return;

    double Generation = 0.0, Serial = 0.0, Frames = 0.0;
    FString Url;
    bool bPaused = true;
    if (!Payload->TryGetNumberField(TEXT("g"), Generation) || Generation != FeedGeneration
        || !Payload->TryGetNumberField(TEXT("s"), Serial) || Serial != ProbeSerial || Serial <= LastCompletedProbe
        || !Payload->TryGetStringField(TEXT("url"), Url) || NormalizeVideoUrl(Url) != NormalizeVideoUrl(SourceUrl)
        || !Payload->TryGetNumberField(TEXT("f"), Frames) || !Payload->TryGetBoolField(TEXT("p"), bPaused)) return;
    const double Now = FPlatformTime::Seconds();
    if (Now - LastProbeAt > MockFrameFreshness) return;
    LastCompletedProbe = static_cast<int32>(Serial);
    if (bPaused) LastFrameAt = 0.0;
    else if (LastDecodedFrame >= 0.0 && Frames > LastDecodedFrame) LastFrameAt = Now;
    LastDecodedFrame = Frames;
}

bool UPilotShellWidget::IsWindowActive() const
{
    if (!FSlateApplication::IsInitialized()) return false;
    const auto Widget = GetCachedWidget();
    if (!Widget.IsValid()) return false;
    auto& Slate = FSlateApplication::Get();
    const auto Window = Slate.FindWidgetWindow(Widget.ToSharedRef());
    return Slate.IsActive() && Window.IsValid() && Window->IsActive();
}

UPilotShellWidget::EInputState UPilotShellWidget::SampleInput(FPilotControlAxes& Axes, FInputSource& Source) const
{
    Axes = {};
    if (Source.Device == EInputDevice::None)
    {
        if (KeyboardInput) Source.Device = EInputDevice::Keyboard;
        else if (bMockInput) Source.Device = EInputDevice::Mock;
    }
    if (Source.Device == EInputDevice::Mock) return EInputState::Neutral;
    if (Source.Device == EInputDevice::Keyboard)
    {
        if (!KeyboardInput) return EInputState::Disconnected;
        Axes = KeyboardInput->GetAxes();
        return KeyboardInput->IsNeutral() ? EInputState::Neutral : EInputState::OffCenter;
    }
#if PLATFORM_WINDOWS
    float* Fields[] = {&Axes.Forward, &Axes.Right, &Axes.Vertical, &Axes.Yaw};
    if (Source.Device == EInputDevice::None && !bPreferWinMM)
    {
        for (int32 Index = 0; Index < XUSER_MAX_COUNT; ++Index)
        {
            if (!ReadGamepad(Index, Axes)) continue;
            Source = {EInputDevice::XInput, Index};
            break;
        }
    }
    else if (Source.Device == EInputDevice::XInput && !ReadGamepad(Source.Index, Axes))
    {
        return EInputState::Disconnected;
    }

    if (Source.Device == EInputDevice::None) Source = {EInputDevice::WinMM, JoystickIndex};
    if (Source.Device == EInputDevice::WinMM)
    {
        const int32 DeviceIndex = Source.Index;
        if (DeviceIndex < 0 || static_cast<uint32>(DeviceIndex) >= joyGetNumDevs()) return EInputState::Disconnected;
        JOYINFOEX Data{};
        Data.dwSize = sizeof(Data);
        Data.dwFlags = JOY_RETURNALL;
        JOYCAPSW Caps{};
        if (joyGetPosEx(DeviceIndex, &Data) != JOYERR_NOERROR || joyGetDevCapsW(DeviceIndex, &Caps, sizeof(Caps)) != JOYERR_NOERROR)
            return EInputState::Disconnected;
        TSet<FString> UsedAxes;
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Fields); ++Index)
        {
            const FString& Name = AxisNames[Index];
            if (UsedAxes.Contains(Name) || !ReadAxis(Name, Caps, Data, *Fields[Index])) return EInputState::MissingAxes;
            UsedAxes.Add(Name);
            if (InvertAxis[Index]) *Fields[Index] *= -1.f;
        }
    }

    bool bNeutral = true;
    for (float* Field : Fields)
    {
        const float Raw = *Field;
        const float Magnitude = FMath::Abs(Raw);
        bNeutral &= Magnitude <= NeutralDeadZone;
        *Field = FMath::Sign(Raw) * FMath::Max(0.f, (Magnitude - NeutralDeadZone) / (1.f - NeutralDeadZone));
    }
    return bNeutral ? EInputState::Neutral : EInputState::OffCenter;
#else
    return EInputState::Disconnected;
#endif
}

void UPilotShellWidget::RequestStop(const FString& Reason)
{
    MockForwardUntil = 0.0;
    if (!Control) return;
    Control->ClearControlInput();
    const FString ActiveUav = Control->GetActiveUavId();
    if (ActiveUav.IsEmpty() || Control->IsRequestPending()) return;
    const auto* ActiveRow = Control->GetAircraft().FindByPredicate([&](const auto& Row) { return Row.UavId == ActiveUav; });
    if (!ActiveRow || ActiveRow->ControlSessionId == StopRequestedFor) return;
    StopRequestedFor = ActiveRow->ControlSessionId;
    Control->Stop(Reason);
}

void UPilotShellWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (!Browser || !Control) return;
    const double Now = FPlatformTime::Seconds();
    ProbeFeed(Now);
    if (Now < NextInputAt) return;
    NextInputAt = Now + InputPollPeriod;

    const FString ActiveId = Control->GetActiveUavId();
    if (ActiveId.IsEmpty() && !Control->IsRequestPending()) SessionInput = {};
    FInputSource Source = SessionInput;
    FPilotControlAxes Axes;
    const EInputState InputState = SampleInput(Axes, Source);
    const bool bWindowActive = IsWindowActive();
    if (!bWindowActive)
    {
        MockForwardUntil = 0.0;
        Control->ClearControlInput();
    }
    if (!ActiveId.IsEmpty())
    {
        if (!Control->IsReady() || ActiveId != SelectedUavId) RequestStop(TEXT("SELECTION_CHANGED"));
        else if (!HasFreshFrames(Now)) RequestStop(TEXT("VIDEO_STALE"));
        else if (!bWindowActive && SessionInput.Device != EInputDevice::Mock) RequestStop(TEXT("WINDOW_INACTIVE"));
        else if (SessionInput.Device == EInputDevice::None || InputState == EInputState::Disconnected || InputState == EInputState::MissingAxes)
            RequestStop(TEXT("DEVICE_LOST"));
        else if (!Control->IsRequestPending())
        {
            const auto* Row = GetSelectedAircraft();
            if (Row && Row->ControlSessionId != StopRequestedFor)
            {
                if (bMockInput && bWindowActive && (MockForwardButton->IsPressed() || Now < MockForwardUntil)) Axes.Forward = 0.65f;
                Control->SetControlInput(Row->bInputFrozen ? FPilotControlAxes{} : Axes);
            }
        }
    }
    UpdateStatus(Now, Source, InputState, bWindowActive);
}

void UPilotShellWidget::BeginControl()
{
    const auto* Row = GetSelectedAircraft();
    if (!Row || !Control->IsReady() || Control->IsRequestPending() || Row->ControlState != TEXT("IDLE")
        || !Row->ActivationReason.IsEmpty() || !Control->GetActiveUavId().IsEmpty() || !IsWindowActive()
        || !HasFreshFrames(FPlatformTime::Seconds())) return;
    FPilotControlAxes Axes;
    FInputSource Source;
    if (SampleInput(Axes, Source) == EInputState::Neutral && Control->Activate(SelectedUavId)) SessionInput = Source;
}

void UPilotShellWidget::StopControl()
{
    RequestStop(TEXT("OPERATOR_STOP"));
}

void UPilotShellWidget::MockForwardPressed()
{
    if (bMockInput && Control && Control->GetActiveUavId() == SelectedUavId && !SelectedUavId.IsEmpty())
        MockForwardUntil = FPlatformTime::Seconds() + MockForwardPulse;
}

bool UPilotShellWidget::CanSendAction(double Now) const
{
    const auto* Row = GetSelectedAircraft();
    return Control && Control->SupportsControlActions() && !Control->IsRequestPending() && Row &&
        Control->GetActiveUavId() == SelectedUavId && !Row->ControlSessionId.IsEmpty() &&
        Row->ControlSessionId != StopRequestedFor && !Row->bInputFrozen && HasFreshFrames(Now) && IsWindowActive();
}

void UPilotShellWidget::QueueAction(EPilotControlAction Action)
{
    if (!CanSendAction(FPlatformTime::Seconds()) || SessionInput.Device == EInputDevice::None) return;
    FPilotControlAxes Axes;
    FInputSource Source = SessionInput;
    const EInputState State = SampleInput(Axes, Source);
    if (State == EInputState::Disconnected || State == EInputState::MissingAxes) return;
    Control->QueueControlAction(Action, FPilotControlActionGuard::CreateUObject(this, &UPilotShellWidget::CanSendAction));
}

void UPilotShellWidget::Takeoff() { QueueAction(EPilotControlAction::Takeoff); }
void UPilotShellWidget::Land() { QueueAction(EPilotControlAction::Land); }
void UPilotShellWidget::Hold() { QueueAction(EPilotControlAction::Hold); }
void UPilotShellWidget::GoHome() { QueueAction(EPilotControlAction::GoHome); }
void UPilotShellWidget::EmergencyStop() { QueueAction(EPilotControlAction::EmergencyStop); }

void UPilotShellWidget::UpdateStatus(double Now, FInputSource Source, EInputState InputState, bool bWindowActive)
{
    const bool bReady = Control->IsReady();
    const bool bPending = Control->IsRequestPending();
    const bool bActive = !Control->GetActiveUavId().IsEmpty();
    const auto* Row = GetSelectedAircraft();
    const bool bFresh = HasFreshFrames(Now);
    const bool bNeutral = InputState == EInputState::Neutral;
    const bool bFrozen = bActive && Row && Row->bInputFrozen;
    const bool bBackendIdle = Row && Row->ControlState == TEXT("IDLE") && Row->ActivationReason.IsEmpty();
    const FString Status = Control->GetStatusCode();
    const FString LastExit = Control->GetLastExitReason();

    LinkLabel->SetText(bReady ? LOCTEXT("LinkReady", "控制链路已连接") : StatusText(Status));
    LinkLabel->SetColorAndOpacity(FSlateColor(bReady ? FigmaUI::Online : FigmaUI::Warning));
    if (Source.Device == EInputDevice::Mock) InputLabel->SetText(LOCTEXT("MockInput", "输入来源 · Mock 按钮"));
    else if (Source.Device == EInputDevice::Keyboard)
        InputLabel->SetText(bNeutral ? LOCTEXT("KeyboardNeutral", "键盘 · 等待输入") : LOCTEXT("KeyboardPressed", "键盘 · 按键输入"));
    else if (Source.Device == EInputDevice::XInput)
    {
        if (InputState == EInputState::Disconnected) InputLabel->SetText(LOCTEXT("GamepadDisconnected", "游戏手柄未连接"));
        else InputLabel->SetText(bNeutral ? LOCTEXT("GamepadNeutral", "游戏手柄 · 摇杆回中") : LOCTEXT("GamepadOffCenter", "游戏手柄 · 摇杆未回中"));
    }
    else switch (InputState)
    {
        case EInputState::MissingAxes: InputLabel->SetText(LOCTEXT("InputMissingAxes", "USB 遥控器轴映射不可用")); break;
        case EInputState::Disconnected: InputLabel->SetText(LOCTEXT("InputDisconnected", "USB 遥控器未连接")); break;
        case EInputState::OffCenter: InputLabel->SetText(LOCTEXT("InputOffCenter", "USB 已连接 · 摇杆未回中")); break;
        case EInputState::Neutral: InputLabel->SetText(LOCTEXT("InputNeutral", "USB 已连接 · 摇杆回中")); break;
    }

    FText FrameText;
    if (SelectedUavId.IsEmpty()) FrameText = LOCTEXT("SelectAircraft", "选择一架已分配的无人机");
    else if (SourceUrl.IsEmpty()) FrameText = LOCTEXT("NoFeed", "此飞机未配置图传");
    else if (bFresh) FrameText = LOCTEXT("FreshFrames", "图传正在更新");
    else if (LastFrameAt > 0.0 || Now - FeedOpenedAt > 5.0) FrameText = LOCTEXT("StaleFrames", "图传没有新帧");
    else FrameText = LOCTEXT("WaitingFrames", "等待图传画面");
    FrameLabel->SetText(FrameText);
    FrameLabel->SetColorAndOpacity(FSlateColor(bFresh ? FigmaUI::Online : FigmaUI::Warning));
    FeedPlaceholder->SetText(FrameText);
    FeedCover->SetVisibility(bFresh ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    FeedTitle->SetText(SelectedUavId.IsEmpty() ? LOCTEXT("Video", "实时图传") : FText::Format(LOCTEXT("VideoFor", "{0} 图传"), FText::AsCultureInvariant(SelectedUavId)));

    FText Detail;
    if (bPending) Detail = LOCTEXT("Pending", "等待后端确认");
    else if (bFrozen) Detail = LOCTEXT("EmergencyStopActive", "急停已生效。停止操控并重新启用后恢复。");
    else if (bActive) Detail = FText::Format(LOCTEXT("Active", "操控中 · {0}"), FText::AsCultureInvariant(Control->GetActiveUavId()));
    else if (Status != TEXT("READY") && Status != TEXT("STOPPED") && Status != TEXT("ACTIVE") && !Status.IsEmpty()) Detail = StatusText(Status);
    else if (!LastExit.IsEmpty() && (LastExit != TEXT("OPERATOR_STOP") || !Row)) Detail = StatusText(LastExit);
    else if (!AircraftIds.Num()) Detail = LOCTEXT("NoAircraft", "等待指挥端分配飞机");
    else if (!Row) Detail = LOCTEXT("SelectAircraft", "选择一架已分配的无人机");
    else if (!Row->ActivationReason.IsEmpty()) Detail = StatusText(Row->ActivationReason);
    else if (Row->ControlState != TEXT("IDLE")) Detail = LOCTEXT("OtherSession", "等待当前操控会话退出");
    else Detail = LOCTEXT("Standby", "已分配 · 待操控");
    DetailLabel->SetText(Detail);
    AircraftPicker->SetIsEnabled(bReady && !bPending && !bActive && !AircraftIds.IsEmpty());
    BeginButton->SetIsEnabled(bReady && !bPending && !bActive && bBackendIdle && bFresh && bNeutral && bWindowActive);
    StopButton->SetIsEnabled(bReady && !bPending && bActive);
    MockForwardButton->SetIsEnabled(bMockInput && bReady && !bPending && bActive && bFresh && !bFrozen);
    ActionPanel->SetVisibility(Control->SupportsControlActions() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    const bool bCanSendAction = CanSendAction(Now) && (bNeutral || InputState == EInputState::OffCenter);
    for (auto& Button : ActionButtons) Button->SetIsEnabled(bCanSendAction);
}

#undef LOCTEXT_NAMESPACE
