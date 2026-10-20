#include "Video/VideoShellWidget.h"
#include "Command/FigmaCommandStyle.h"
#include "Components/Image.h"
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

void UVideoFeedButton::ActivateFeed() { if (Owner.IsValid()) Owner->SelectDrone(DroneId); }

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
    UpdateFeeds();
    UpdatePresentation();
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
    RetryIcon->SetVisibility(Retry->GetIsEnabled()?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
    RetryReadyIcon->SetVisibility(Retry->GetIsEnabled()?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    FigmaUI::Text(RetryCaption,12,Retry->GetIsEnabled()?FigmaUI::PrimaryText:FLinearColor(FColor(120,126,147)),true);
    Browser->SetVisibility(SourceUrl.IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
    // Keep the old document/frame covered until the selected source decodes frames.
    MediaCover->SetVisibility(MediaState == TEXT("PLAYING") ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    EmptyVideo->SetText(DisplayedDroneId<=0?ProductText::Get(TEXT("VideoUI.NoTarget")):ProductText::Source(SourceUrl.IsEmpty()?TEXT("NO SOURCE"):*MediaState));
    EmptyHint->SetText(ProductText::Get(DisplayedDroneId<=0?TEXT("VideoUI.SelectSource"):SourceUrl.IsEmpty()?TEXT("VideoUI.ConfigureSource"):TEXT("VideoUI.WaitingFrames")));
    if(Retry->GetIsEnabled())Retry->SetToolTipText(ProductText::Get(TEXT("VideoUI.RetryHint")));else Retry->SetToolTipText(ProductText::Get(TEXT("VideoUI.RetryUnavailable")));
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
