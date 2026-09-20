#include "Video/VideoShellWidget.h"
#include "Shared/Stage1HeaderWidget.h"
#include "Command/CommandTheme.h"
#include "Shared/ProductText.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Engine/GameInstance.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/ButtonSlot.h"
#include "Components/ScrollBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "WebBrowser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Brushes/SlateColorBrush.h"

void UVideoShellWidget::InitializeRegistry(UDroneRegistrySubsystem* InRegistry) { Registry = InRegistry; }

namespace
{
    const FLinearColor Cyan=CommandTheme::Cyan;
    const FLinearColor Card=CommandTheme::Surface;
    FButtonStyle FeedStyle(bool Selected)
    {
        FButtonStyle Style;
        FSlateBrush Brush;
        Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
        Brush.TintColor = Selected ? FLinearColor(0.025f, 0.12f, 0.17f) : Card;
        Brush.OutlineSettings.CornerRadii = FVector4(6, 6, 6, 6);
        Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
        Brush.OutlineSettings.Width = 1;
        Brush.OutlineSettings.Color = Selected ? Cyan : FLinearColor(0.09f, 0.15f, 0.2f);
        Style.SetNormal(Brush);
        Brush.OutlineSettings.Color = Cyan;
        Style.SetHovered(Brush); Style.SetPressed(Brush);
        Brush.TintColor = FLinearColor(0.018f, 0.026f, 0.038f);
        Style.SetDisabled(Brush);
        Style.SetNormalPadding(FMargin(12)); Style.SetPressedPadding(FMargin(12));
        return Style;
    }
}

void UVideoFeedButton::ActivateFeed() { if (Owner.IsValid()) Owner->SelectDrone(DroneId); }

UTextBlock* UVideoShellWidget::MakeText(const FString& Value, int32 Size)
{
    auto* W = WidgetTree->ConstructWidget<UTextBlock>();
    auto Font = W->GetFont(); Font.Size = Size; W->SetFont(Font);
    W->SetText(ProductText::Source(Value));
    W->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.86f, 0.92f)));
    W->SetAutoWrapText(true);
    return W;
}

UVideoFeedButton* UVideoShellWidget::MakeFeedButton()
{
    auto* B = WidgetTree->ConstructWidget<UVideoFeedButton>();
    B->Owner = this; B->SetStyle(FeedStyle(false));
    B->Label = MakeText(TEXT("RESERVED"), 12);
    auto* ContentSlot = Cast<UButtonSlot>(B->AddChild(B->Label));
    ContentSlot->SetHorizontalAlignment(HAlign_Fill); ContentSlot->SetVerticalAlignment(VAlign_Center);

    B->OnClicked.AddDynamic(B, &UVideoFeedButton::ActivateFeed);
    return B;
}

TSharedRef<SWidget> UVideoShellWidget::RebuildWidget()
{
    if (!WidgetTree->RootWidget)
    {
        auto* Background = WidgetTree->ConstructWidget<UBorder>();
        Background->SetBrushColor(CommandTheme::Background);
        Background->SetPadding(FMargin(0)); WidgetTree->RootWidget = Background;
        auto* Layout = WidgetTree->ConstructWidget<UVerticalBox>(); Background->SetContent(Layout);
        Layout->AddChildToVerticalBox(CreateWidget<UStage1HeaderWidget>(GetOwningPlayer()));
        SystemHeader = MakeText(TEXT("SYSTEM ONLINE: N/A"), 12);
        Layout->AddChildToVerticalBox(SystemHeader)->SetPadding(FMargin(12, 6, 12, 8));
        auto* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
        Layout->AddChildToVerticalBox(Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        auto Panel = [&](float Width)
        {
            auto* Size = WidgetTree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(Width);
            auto* Border = WidgetTree->ConstructWidget<UBorder>(); Border->SetBrushColor(Card); Border->SetPadding(FMargin(12)); Size->AddChild(Border);
            auto* Column = WidgetTree->ConstructWidget<UVerticalBox>(); Border->SetContent(Column);
            Body->AddChildToHorizontalBox(Size); return Column;
        };
        auto* Left = Panel(210);
        Left->AddChildToVerticalBox(MakeText(TEXT("AIRCRAFT\n飞机列表"), 15))->SetPadding(FMargin(0, 0, 0, 10));
        Left->AddChildToVerticalBox(MakeText(TEXT("SELECT PRIMARY FEED"), 10))->SetPadding(FMargin(0, 0, 0, 8));
        auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
        Left->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        AircraftList = WidgetTree->ConstructWidget<UVerticalBox>(); Scroll->AddChild(AircraftList);
        Left->AddChildToVerticalBox(MakeText(TEXT("MODEL N/A = no aircraft model field\nVisual presets are not aircraft specifications."), 10))->SetPadding(FMargin(0, 12, 0, 0));
        auto* Center = WidgetTree->ConstructWidget<UVerticalBox>();
        auto* CenterSlot = Body->AddChildToHorizontalBox(Center); CenterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); CenterSlot->SetPadding(FMargin(12, 0));
        auto* PrimaryTop = WidgetTree->ConstructWidget<UHorizontalBox>();
        Center->AddChildToVerticalBox(PrimaryTop)->SetPadding(FMargin(0, 0, 0, 8));
        Heading = MakeText(TEXT("PRIMARY VIDEO"), 17);
        PrimaryTop->AddChildToHorizontalBox(Heading)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Status = MakeText(TEXT("OFFLINE"), 12); Status->SetAutoWrapText(false); PrimaryTop->AddChildToHorizontalBox(Status)->SetPadding(FMargin(10, 0));
        auto* Surface = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("VideoSurface"));
        Center->AddChildToVerticalBox(Surface)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        auto Fill = [&](UWidget* W) { auto* FeedSlot = Surface->AddChildToOverlay(W); FeedSlot->SetHorizontalAlignment(HAlign_Fill); FeedSlot->SetVerticalAlignment(VAlign_Fill); };
        auto* Matte = WidgetTree->ConstructWidget<UBorder>(); Matte->SetBrushColor(FLinearColor::Black); Fill(Matte);
        Browser = WidgetTree->ConstructWidget<UWebBrowser>(UWebBrowser::StaticClass(), TEXT("VideoView"));
        Browser->OnUrlChanged.AddDynamic(this, &UVideoShellWidget::UrlChanged);
        Browser->OnConsoleMessage.AddDynamic(this, &UVideoShellWidget::ConsoleMessage); Fill(Browser);
        auto* Detection = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DetectionOverlay"));
        Detection->SetVisibility(ESlateVisibility::HitTestInvisible); Fill(Detection);
        auto* StatusLayer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("StatusOverlay"));
        StatusLayer->SetVisibility(ESlateVisibility::HitTestInvisible); Fill(StatusLayer);
        EmptyVideo = MakeText(TEXT("NO UAV SELECTED\nNO ACTIVE STREAM"), 24); EmptyVideo->SetJustification(ETextJustify::Center); EmptyVideo->SetAutoWrapText(false);
        MediaCover = WidgetTree->ConstructWidget<UBorder>(); MediaCover->SetBrushColor(FLinearColor::Black);
        MediaCover->SetHorizontalAlignment(HAlign_Center); MediaCover->SetVerticalAlignment(VAlign_Center); MediaCover->SetContent(EmptyVideo);
        auto* CoverSlot = StatusLayer->AddChildToOverlay(MediaCover); CoverSlot->SetHorizontalAlignment(HAlign_Fill); CoverSlot->SetVerticalAlignment(VAlign_Fill);
        auto* Interaction = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("UserInteractionLayer"));
        Interaction->SetVisibility(ESlateVisibility::SelfHitTestInvisible); Fill(Interaction);
        Details = MakeText(TEXT("Camera: N/A"), 11);
        Center->AddChildToVerticalBox(Details)->SetPadding(FMargin(0, 8));
        SecondaryArea = WidgetTree->ConstructWidget<UHorizontalBox>();
        auto* SecondarySize = WidgetTree->ConstructWidget<USizeBox>(); SecondarySize->SetHeightOverride(140); SecondarySize->AddChild(SecondaryArea);
        Center->AddChildToVerticalBox(SecondarySize);
        // Collapse the complete size container in Focus mode, including its height reservation.
        for (int32 I = 0; I < 5; ++I)
        {
            auto* Feed = MakeFeedButton(); SecondaryButtons.Add(Feed);
            auto* FeedSlot = SecondaryArea->AddChildToHorizontalBox(Feed); FeedSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); FeedSlot->SetPadding(FMargin(3, 0));
        }
        auto* Right = Panel(250);
        Right->AddChildToVerticalBox(MakeText(TEXT("AI DETECTION\n智能检测"), 15))->SetPadding(FMargin(0, 0, 0, 12));
        auto* AI = MakeText(TEXT("AI DETECTION NOT CONNECTED"), 12); AI->SetColorAndOpacity(FSlateColor(FLinearColor(1, 0.65f, 0.22f)));
        Right->AddChildToVerticalBox(AI)->SetPadding(FMargin(0, 0, 0, 14));
        Right->AddChildToVerticalBox(MakeText(TEXT("人员  /  PERSON\n\n车辆  /  VEHICLE\n\n无人机  /  UAV\n\n异常行为  /  BEHAVIOR\n\n其他目标  /  OTHER"), 12));
        Right->AddChildToVerticalBox(MakeText(TEXT("RECENT ALERTS\n最近告警"), 15))->SetPadding(FMargin(0, 26, 0, 12));
        Right->AddChildToVerticalBox(MakeText(TEXT("NO ACTIVE ALERTS"), 13))->SetPadding(FMargin(0, 0, 0, 12));
        Right->AddChildToVerticalBox(MakeText(TEXT("Reserved fields\nTitle · UAV · Target\nTimestamp · Severity"), 11));
        auto* Severity = WidgetTree->ConstructWidget<UHorizontalBox>(); Right->AddChildToVerticalBox(Severity)->SetPadding(FMargin(0, 8));
        const TCHAR* Names[] = {TEXT("INFO"), TEXT("WARNING"), TEXT("CRITICAL")};
        const FLinearColor Colors[] = {Cyan, FLinearColor(1, .65f, .22f), FLinearColor(.95f, .22f, .28f)};
        for (int32 I=0; I<3; ++I) { auto* T=MakeText(Names[I], 9); T->SetColorAndOpacity(FSlateColor(Colors[I])); Severity->AddChildToHorizontalBox(T)->SetPadding(FMargin(0,0,8,0)); }
        Right->AddChildToVerticalBox(MakeText(TEXT("UAV ONLINE: N/A\nAI ENGINE: NOT CONNECTED\nSTORAGE: N/A\nNETWORK: N/A"), 11))->SetPadding(FMargin(0, 20, 0, 0));
        auto* Toolbar = WidgetTree->ConstructWidget<UHorizontalBox>(); Layout->AddChildToVerticalBox(Toolbar)->SetPadding(FMargin(0, 14, 0, 0));
        auto Action = [&](const TCHAR* Label, bool Enabled)
        {
            auto* B = WidgetTree->ConstructWidget<UButton>(); B->SetStyle(FeedStyle(false)); auto* ActionText=MakeText(Label, 12); ActionText->SetAutoWrapText(false); B->AddChild(ActionText); B->SetIsEnabled(Enabled);
            if (!Enabled) B->SetToolTipText(ProductText::Source(TEXT("Reserved for future integration")));
            Toolbar->AddChildToHorizontalBox(B)->SetPadding(FMargin(0,0,6,0)); return B;
        };
        auto* Four=Action(TEXT("4路 / 4 VIEW"),true); Four->OnClicked.AddDynamic(this,&UVideoShellWidget::ViewFour); ModeButtons.Add(Four);
        auto* Six=Action(TEXT("6路 / 6 VIEW"),true); Six->OnClicked.AddDynamic(this,&UVideoShellWidget::ViewSix); ModeButtons.Add(Six);
        auto* Focus=Action(TEXT("FOCUS"),true); Focus->OnClicked.AddDynamic(this,&UVideoShellWidget::ViewFocus); ModeButtons.Add(Focus);
        auto* Zh=Action(TEXT("中文"),true);Zh->OnClicked.AddDynamic(this,&UVideoShellWidget::Chinese);
        auto* En=Action(TEXT("English"),true);En->OnClicked.AddDynamic(this,&UVideoShellWidget::English);
        auto* SetActive=Action(TEXT("SET AS ACTIVE UAV"),true); SetActive->OnClicked.AddDynamic(this,&UVideoShellWidget::PublishActive);
        Retry=Action(TEXT("REFRESH / RETRY"),true); Retry->OnClicked.AddDynamic(this,&UVideoShellWidget::RetrySource);
        Action(TEXT("RECORD"),false); Action(TEXT("PLAYBACK"),false); Action(TEXT("SCREENSHOT"),false); Action(TEXT("ALERT FILTER"),false);
        Layout->AddChildToVerticalBox(MakeText(TEXT("SINGLE ACTIVE PLAYER  ·  Secondary feeds are reserved slots  ·  No AI backend connected"), 10))->SetPadding(FMargin(0, 8, 0, 0));
        ProbeToken = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        SetViewMode(ViewMode);
    }
    return Super::RebuildWidget();
}

void UVideoShellWidget::SetViewMode(int32 Channels)
{
    if (Channels != 1 && Channels != 4 && Channels != 6) return;
    ViewMode = Channels;
    if (!SecondaryArea) return;
    SecondaryArea->GetParent()->SetVisibility(Channels == 1 ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    for (int32 I=0; I<SecondaryButtons.Num(); ++I) SecondaryButtons[I]->SetVisibility(I < Channels-1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    const int32 Modes[] = {4,6,1};
    for (int32 I=0; I<ModeButtons.Num(); ++I) ModeButtons[I]->SetStyle(FeedStyle(Channels == Modes[I]));
}

UVideoFeedButton* UVideoShellWidget::GetAircraftButton(int32 Id) const
{
    for (auto B : AircraftButtons) if (B->DroneId == Id) return B;
    return nullptr;
}
UVideoFeedButton* UVideoShellWidget::GetSecondaryButton(int32 Index) const { return SecondaryButtons.IsValidIndex(Index) ? SecondaryButtons[Index] : nullptr; }

void UVideoShellWidget::SelectDrone(int32 Id)
{
    if (bStopped || !Registry.IsValid()) return;
    FDroneDescriptor D;
    if (Id <= 0 || !Registry->GetDroneDescriptor(Id,D) || D.bIsEnemyTarget) return;
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    if(Sync->IsEnabled())Sync->OpenVideo(FString::Printf(TEXT("UAV-%02d"),Id));
    else LocalSourceId=Id;
    Refresh();
}

FString UVideoShellWidget::FeedDescription(int32 Id, bool Secondary) const
{
    FDroneDescriptor D;
    if (!Registry.IsValid() || !Registry->GetDroneDescriptor(Id,D)) return TEXT("RESERVED\n\nNO UAV ASSIGNED");
    FDroneTelemetrySnapshot T;
    FString Link=TEXT("N/A");
    if (Registry->GetTelemetry(Id,T) && T.LastUpdateTime > 0)
        Link = T.Availability == EDroneAvailability::Online ? TEXT("ONLINE") : T.Availability == EDroneAvailability::Lost ? TEXT("LOST") : TEXT("OFFLINE");
    return FText::Format(ProductText::Get(TEXT("Video.Feed")),FText::AsCultureInvariant(FString::Printf(TEXT("UAV-%02d"),Id)),ProductText::Source(Link),ProductText::Source(D.VideoUrl.IsEmpty()?TEXT("NO SOURCE"):TEXT("SOURCE SET")),Secondary?ProductText::Source(TEXT("VIDEO SLOT\nNO ACTIVE STREAM")):FText::GetEmpty()).ToString();
}

void UVideoShellWidget::UpdateFeeds()
{
    auto Drones=Registry->GetFriendlyDroneDescriptors();
    Drones.Sort([](const FDroneDescriptor& A,const FDroneDescriptor& B){return A.DroneId < B.DroneId;});
    TArray<int32> Ids; for(const auto& D:Drones) Ids.Add(D.DroneId);
    if (AircraftIds != Ids || AircraftButtons.IsEmpty())
    {
        AircraftIds=Ids; AircraftList->ClearChildren(); AircraftButtons.Reset();
        for (int32 I=0; I<FMath::Max(6,Ids.Num()); ++I)
        {
            auto* B=MakeFeedButton(); B->DroneId=Ids.IsValidIndex(I)?Ids[I]:0; AircraftButtons.Add(B);
            AircraftList->AddChildToVerticalBox(B)->SetPadding(FMargin(0,0,0,6));
        }
    }
    // Preserve slot order; selecting any secondary exchanges it with the old primary.
    SecondaryIds.RemoveAll([&](int32 Id){return !Ids.Contains(Id) || Id == DisplayedDroneId;});
    for(int32 Id:Ids) if(Id != DisplayedDroneId) SecondaryIds.AddUnique(Id);
    for(auto B:AircraftButtons)
    {
        B->SetIsEnabled(B->DroneId > 0); B->SetStyle(FeedStyle(B->DroneId > 0 && B->DroneId == DisplayedDroneId));
        B->Label->SetText(ProductText::Source(FeedDescription(B->DroneId,false)));
        FDroneDescriptor D;
        if (Registry->GetDroneDescriptor(B->DroneId,D)) B->SetToolTipText(FText::Format(ProductText::Get(TEXT("Video.DescriptorTip")),FText::AsCultureInvariant(D.Name),FText::AsCultureInvariant(GetDroneModelDisplayName(Registry->GetDroneModelType(B->DroneId)))));
    }
    for(int32 I=0;I<SecondaryButtons.Num();++I)
    {
        auto B=SecondaryButtons[I]; B->DroneId=GetSecondaryDroneId(I); B->SetIsEnabled(B->DroneId > 0);
        B->Label->SetText(ProductText::Source(FeedDescription(B->DroneId,true)));
    }
    bool bQA = false;
    for (const auto& D : Drones) bQA |= D.Name.Contains(TEXT("MOCK")) || D.Name.Contains(TEXT("QA VISUAL STATE"));
    SystemHeader->SetText(FText::Format(ProductText::Get(TEXT("Video.System")),FText::AsNumber(Ids.Num()),FText::AsNumber(ViewMode),FText::AsNumber(MediaState==TEXT("PLAYING")?1:0),FText::AsCultureInvariant(FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")))));
}

void UVideoShellWidget::Refresh()
{
    if (!Browser || bStopped || !Registry.IsValid()) return;
    auto Drones = Registry->GetFriendlyDroneDescriptors();
    Drones.Sort([](const FDroneDescriptor& A, const FDroneDescriptor& B) { return A.DroneId < B.DroneId; });
    auto* ContextSync = GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    int32 Id=LocalSourceId;
    if(ContextSync->IsEnabled()) {
        FString Target;if(ContextSync->GetVideoView())ContextSync->GetVideoView()->TryGetStringField(TEXT("video_target_uav_id"),Target);
        Id=0;if(Target.StartsWith(TEXT("UAV-")))LexTryParseString(Id,*Target.Mid(4));
    }
    FDroneDescriptor Selected;
    bool bValid = Registry->GetDroneDescriptor(Id, Selected) && !Selected.bIsEnemyTarget;
    if (!bValid) Id=0;
    const FString Url = bValid ? Selected.VideoUrl : FString();
    if (Id != DisplayedDroneId || Url != SourceUrl) SetSource(Id, Url);
    Heading->SetText(FText::Format(ProductText::Get(TEXT("Video.Heading")),bValid?FText::AsCultureInvariant(FString::Printf(TEXT("UAV-%02d"),Id)):ProductText::Source(TEXT("NO TARGET"))));
    UpdateFeeds();
    if (auto* Sync = GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>(); Sync && Sync->IsEnabled())
        {
            FString Global; if(Sync->GetContext()) Sync->GetContext()->TryGetStringField(TEXT("active_uav_id"),Global);
            SystemHeader->SetText(FText::Format(ProductText::Get(TEXT("Video.Header")),FText::AsCultureInvariant(Sync->GetStatusText()),DisplayedDroneId>0?FText::AsCultureInvariant(FString::Printf(TEXT("UAV-%02d"),DisplayedDroneId)):ProductText::Source(TEXT("NO TARGET")),Global.IsEmpty()?ProductText::Source(TEXT("NO TARGET")):FText::AsCultureInvariant(Global)));
        }
    const double Now = FPlatformTime::Seconds();
    if (!SourceUrl.IsEmpty() && Now - LastProgress > 10) MediaState = bPageReady ? TEXT("STALLED / NO FRAMES") : TEXT("OFFLINE / LOAD FAILED");
    if (bPageReady && Now - LastProbe >= 0.5)
    {
        LastProbe = Now;
        // Only inspect the currently committed source document. A navigation is not playback.
        Browser->ExecuteJavascript(FString::Printf(TEXT(
            "(()=>{const v=document.querySelector('video');const q=v&&v.getVideoPlaybackQuality?v.getVideoPlaybackQuality():null;"
            "console.log('V1-%s:'+JSON.stringify({g:%d,s:performance.now()/1000,url:location.href,t:v?v.currentTime:-1,f:q?q.totalVideoFrames:-1,"
            "w:v?v.videoWidth:0,h:v?v.videoHeight:0,p:!v||v.paused||v.ended,e:!!(v&&v.error)}));})()"), *ProbeToken, Generation));
    }
    UpdateLabels();
}

void UVideoShellWidget::SetSource(int32 Id, const FString& Url)
{
    if (Id != DisplayedDroneId)
    {
        const int32 FeedSlot = SecondaryIds.IndexOfByKey(Id);
        if (FeedSlot != INDEX_NONE) SecondaryIds[FeedSlot] = DisplayedDroneId;
    }
    ++Generation; DisplayedDroneId = Id; SourceUrl = Url;
    bPageReady = false; Width = Height = 0; Fps = -1; LastMediaTime = LastFrames = -1; LastSample = LastProbe = 0;
    LastProgress = FPlatformTime::Seconds();
    MediaState = Id<=0?TEXT("NO TARGET"):Url.IsEmpty()?TEXT("NO SOURCE"):TEXT("CONNECTING");
    // LoadURL replaces the old document and its peer connection in the single browser.
    Browser->LoadURL(Url.IsEmpty() ? TEXT("about:blank") : Url);
    UpdateLabels();
    UE_LOG(LogTemp, Log, TEXT("[Video] Source selected UAV=%d generation=%d hasSource=%d"), Id, Generation, !Url.IsEmpty());
}

void UVideoShellWidget::UrlChanged(const FText& Url)
{
    if (bStopped || SourceUrl.IsEmpty()) return;
    FString Actual = Url.ToString(), Expected = SourceUrl;
    Actual.RemoveFromEnd(TEXT("/")); Expected.RemoveFromEnd(TEXT("/"));
    bPageReady = Actual == Expected;
    if (bPageReady) MediaState = TEXT("PAGE READY / WAITING VIDEO");
}

void UVideoShellWidget::ConsoleMessage(const FString& Message, const FString& Source, int32 Line)
{
    const FString Prefix = TEXT("V1-") + ProbeToken + TEXT(":");
    if (bStopped || !bPageReady || !Message.StartsWith(Prefix)) return;
    TSharedPtr<FJsonObject> Data;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Message.Mid(Prefix.Len())), Data) || !Data.IsValid()) return;
    double Gen=0, SampleTime=0, T=0, Frames=0, W=0, H=0; bool Paused=false, Error=false; FString Url;
    if (!Data->TryGetNumberField(TEXT("g"), Gen) || Gen != Generation || !Data->TryGetStringField(TEXT("url"), Url)
        || Url != Browser->GetUrl() || !Data->TryGetNumberField(TEXT("s"), SampleTime) || SampleTime <= LastSample
        || !Data->TryGetNumberField(TEXT("t"), T) || !Data->TryGetNumberField(TEXT("f"), Frames)
        || !Data->TryGetNumberField(TEXT("w"), W) || !Data->TryGetNumberField(TEXT("h"), H)
        || !Data->TryGetBoolField(TEXT("p"), Paused) || !Data->TryGetBoolField(TEXT("e"), Error)) return;
    const double Now = FPlatformTime::Seconds();
    const bool Progress = !Paused && !Error && W > 0 && H > 0 && LastSample > 0
        && (Frames >= 0 ? Frames > LastFrames : T != LastMediaTime);
    if (Progress)
    {
        LastProgress = Now; MediaState = TEXT("PLAYING");
        // Browser callbacks can arrive in a burst after window focus changes. Measure
        // decoded frames against the browser's monotonic clock, not callback arrival.
        Fps = Frames >= 0 && LastFrames >= 0 ? (Frames - LastFrames) / FMath::Max(SampleTime - LastSample, 0.01) : -1;
    }
    else if (Error) MediaState = TEXT("OFFLINE / MEDIA ERROR");
    else if (Paused && T >= 0) MediaState = TEXT("PAUSED / ENDED");
    Width = FMath::Clamp(W, 0.0, 16384.0); Height = FMath::Clamp(H, 0.0, 16384.0);
    LastSample = SampleTime; LastMediaTime = T; LastFrames = Frames;
    UpdateLabels();
}

void UVideoShellWidget::UpdateLabels()
{
    if (!Status || !Details) return;
    Status->SetText(ProductText::Source(MediaState));
    Status->SetColorAndOpacity(FSlateColor(MediaState == TEXT("PLAYING") ? FLinearColor(0.2f, 0.9f, 0.55f) : FLinearColor(1, 0.7f, 0.3f)));
    const FString Resolution = Width > 0 ? FString::Printf(TEXT("%d x %d"), Width, Height) : TEXT("N/A");
    const FString Rate = Fps >= 0 && MediaState == TEXT("PLAYING") ? FString::Printf(TEXT("%.0f measured"), Fps) : TEXT("N/A");
    Details->SetText(FText::Format(ProductText::Get(TEXT("Video.Details")),FText::AsCultureInvariant(DisplayedDroneId>0?FString::Printf(TEXT("UAV-%02d"),DisplayedDroneId):TEXT("")),FText::AsCultureInvariant(Resolution),FText::AsCultureInvariant(Rate),ProductText::Source(MediaState)));
    Retry->SetIsEnabled(!bStopped && !SourceUrl.IsEmpty());
    Browser->SetVisibility(SourceUrl.IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
    // Keep the old document/frame covered until the selected source decodes frames.
    MediaCover->SetVisibility(MediaState == TEXT("PLAYING") ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    EmptyVideo->SetText(DisplayedDroneId<=0?ProductText::Get(TEXT("Video.NoTargetHint")):ProductText::Source(SourceUrl.IsEmpty()?TEXT("NO SOURCE\nAssign a video source to this UAV"):*MediaState));
}

void UVideoShellWidget::RetrySource() { if (!bStopped && Browser && !SourceUrl.IsEmpty()) { SetSource(DisplayedDroneId, SourceUrl); MediaState=TEXT("RETRYING"); UpdateLabels(); } }
void UVideoShellWidget::Shutdown()
{
    if (bStopped) return;
    bStopped = true; ++Generation; bPageReady = false;
    if (Browser) Browser->LoadURL(TEXT("about:blank"));
    MediaState = TEXT("OFFLINE / CLOSED"); UpdateLabels();
    UE_LOG(LogTemp, Log, TEXT("[Video] Browser stopped"));
}
void UVideoShellWidget::Resume() { if (bStopped) { bStopped = false; SetSource(DisplayedDroneId, SourceUrl); Refresh(); } }
void UVideoShellWidget::NativeDestruct() { Shutdown(); Super::NativeDestruct(); }

void UVideoShellWidget::SetFollowTarget(bool Enabled) { bFollowTarget=false; }
void UVideoShellWidget::Chinese(){GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->SetLanguage(TEXT("zh-Hans"));}
void UVideoShellWidget::English(){GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->SetLanguage(TEXT("en"));}
bool UVideoShellWidget::SetAsActiveUAV() {
    if(bStopped || DisplayedDroneId<=0)return false;
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    return Sync && Sync->IsEnabled() && Sync->SetActiveUAV(DisplayedDroneId);
}
