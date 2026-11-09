#include "Video/VideoShellWidget.h"
#include "Command/FigmaCommandStyle.h"
#include "Components/Image.h"
#include "Command/CommandTheme.h"
#include "Shared/ProductText.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
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
#include "Components/CanvasPanelSlot.h"
#include "WebBrowser.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Brushes/SlateColorBrush.h"


void UVideoShellWidget::InitializeRegistry(UDroneRegistrySubsystem* InRegistry) { Registry = InRegistry; }
void UVideoShellWidget::StartDetectionReceiver()
{
    // 闃叉閲嶅鍒涘缓 Socket
    if (DetectionSocket)
    {
        return;
    }

    ISocketSubsystem* SocketSubsystem =
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[YOLO UDP] Socket subsystem unavailable"));
        return;
    }

    DetectionSocket = SocketSubsystem->CreateSocket(
        NAME_DGram,
        TEXT("YOLODetectionReceiver"),
        false
    );

    if (!DetectionSocket)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[YOLO UDP] Failed to create UDP socket"));
        return;
    }

    TSharedRef<FInternetAddr> LocalAddress =
        SocketSubsystem->CreateInternetAddr();

    LocalAddress->SetAnyAddress();
    LocalAddress->SetPort(18992);

    DetectionSocket->SetNonBlocking(true);

    if (!DetectionSocket->Bind(*LocalAddress))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[YOLO UDP] Failed to bind UDP port 18992"));

        SocketSubsystem->DestroySocket(DetectionSocket);
        DetectionSocket = nullptr;
        return;
    }

    int32 ActualBufferSize = 0;
    DetectionSocket->SetReceiveBufferSize(
        1024 * 1024,
        ActualBufferSize
    );

    UE_LOG(LogTemp, Log,
        TEXT("[YOLO UDP] Listening on 0.0.0.0:18992, buffer=%d"),
        ActualBufferSize);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            DetectionPollTimer,
            this,
            &UVideoShellWidget::PollDetectionSocket,
            0.016f,
            true
        );
    }
    else
    {
        UE_LOG(LogTemp, Error,
            TEXT("[YOLO UDP] Cannot start polling: World unavailable"));

        StopDetectionReceiver();
    }
}

void UVideoShellWidget::StopDetectionReceiver()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DetectionPollTimer);
    }

    if (DetectionSocket)
    {
        ISocketSubsystem* SocketSubsystem =
            ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

        if (SocketSubsystem)
        {
            SocketSubsystem->DestroySocket(DetectionSocket);
        }

        DetectionSocket = nullptr;

        UE_LOG(LogTemp, Log,
            TEXT("[YOLO UDP] Receiver stopped"));
    }
}
void UVideoShellWidget::PollDetectionSocket()
{
    if (!DetectionSocket || bStopped)
    {
        return;
    }

    // 鍙繚鐣欐渶鏂扮殑鏁版嵁鍖咃紝閬垮厤澶勭悊绉帇鐨勬棫甯?
    TArray<uint8> Buffer;
    Buffer.SetNumUninitialized(65537);

    FString LatestPayload;
    int32 ReceivedPackets = 0;

    uint32 PendingSize = 0;

    // 鏈€澶氳繛缁鍙?28涓暟鎹寘锛岄伩鍏嶅崟娆¤疆璇㈠崰鐢ㄨ繃闀挎椂闂?
    while (ReceivedPackets < 128 &&
           DetectionSocket->HasPendingData(PendingSize))
    {
        int32 BytesRead = 0;

        if (!DetectionSocket->Recv(
                Buffer.GetData(),
                65536,
                BytesRead))
        {
            break;
        }

        if (BytesRead <= 0)
        {
            break;
        }

        Buffer[BytesRead] = 0;

        LatestPayload = UTF8_TO_TCHAR(
            reinterpret_cast<const char*>(Buffer.GetData()));

        ++ReceivedPackets;
    }

    if (LatestPayload.IsEmpty())
    {
        return;
    }

    // 瑙ｆ瀽 JSON
    TSharedPtr<FJsonObject> JsonObject;

    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(LatestPayload);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) ||
        !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[YOLO UDP] Invalid JSON packet"));
        return;
    }

    double DroneIdValue = 0;
    double FrameIdValue = 0;
    double WidthValue = 0;
    double HeightValue = 0;

    if (!JsonObject->TryGetNumberField(TEXT("drone_id"), DroneIdValue) ||
        !JsonObject->TryGetNumberField(TEXT("image_width"), WidthValue) ||
        !JsonObject->TryGetNumberField(TEXT("image_height"), HeightValue))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[YOLO UDP] Missing required JSON fields"));
        return;
    }

    JsonObject->TryGetNumberField(TEXT("frame_id"), FrameIdValue);

    const int32 DroneId = static_cast<int32>(DroneIdValue);
    const int32 FrameId = static_cast<int32>(FrameIdValue);
    const int32 ImageWidth = static_cast<int32>(WidthValue);
    const int32 ImageHeight = static_cast<int32>(HeightValue);

    // 鍙帴鏀跺綋鍓嶈棰戠獥鍙ｅ搴旂殑鏃犱汉鏈烘暟鎹?
    if (DroneId != DisplayedDroneId)
    {
        return;
    }

    if (ImageWidth <= 0 || ImageHeight <= 0)
    {
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* Detections = nullptr;

    if (!JsonObject->TryGetArrayField(
            TEXT("detections"),
            Detections) || !Detections)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[YOLO UDP] Missing detections array"));
        return;
    }

    // 鎻愬彇妫€娴嬫鍧愭爣鍜岀被鍒俊鎭?
    TArray<FVector4> Boxes;
    TArray<FString> Labels;

    Boxes.Reserve(Detections->Num());
    Labels.Reserve(Detections->Num());

    for (const TSharedPtr<FJsonValue>& DetectionValue : *Detections)
    {
        if (!DetectionValue.IsValid())
        {
            continue;
        }

        const TSharedPtr<FJsonObject> DetectionObject =
            DetectionValue->AsObject();

        if (!DetectionObject.IsValid())
        {
            continue;
        }

        double X1 = 0.0;
        double Y1 = 0.0;
        double X2 = 0.0;
        double Y2 = 0.0;
        double Confidence = 0.0;

        FString ClassName;

        if (!DetectionObject->TryGetNumberField(TEXT("x1"), X1) ||
            !DetectionObject->TryGetNumberField(TEXT("y1"), Y1) ||
            !DetectionObject->TryGetNumberField(TEXT("x2"), X2) ||
            !DetectionObject->TryGetNumberField(TEXT("y2"), Y2))
        {
            continue;
        }

        DetectionObject->TryGetNumberField(TEXT("confidence"), Confidence);
        DetectionObject->TryGetStringField(TEXT("class_name"), ClassName);

        if (ClassName.IsEmpty())
        {
            ClassName = TEXT("Object");
        }

        Boxes.Add(FVector4(
            static_cast<float>(X1),
            static_cast<float>(Y1),
            static_cast<float>(X2),
            static_cast<float>(Y2)
        ));

        Labels.Add(FString::Printf(
            TEXT("%s %.0f%%"),
            *ClassName,
            FMath::Clamp(Confidence, 0.0, 1.0) * 100.0
        ));
    }

    // 鏇存柊 UE 鍘熺敓妫€娴嬫
    UpdateDetectionOverlay(
        Boxes,
        Labels,
        ImageWidth,
        ImageHeight
    );

    // 姣?0甯ц褰曚竴娆℃帴鏀剁姸鎬?
    if (FrameId > 0 && FrameId % 60 == 0)
    {
        UE_LOG(LogTemp, Log,
            TEXT("[YOLO UDP] Drone=%d Frame=%d Detections=%d Image=%dx%d Packets=%d"),
            DroneId,
            FrameId,
            Boxes.Num(),
            ImageWidth,
            ImageHeight,
            ReceivedPackets);
    }
}

void UVideoShellWidget::ClearDetectionOverlay()
{
    if (DetectionOverlay)
    {
        DetectionOverlay->ClearChildren();
    }

    DetectionBoxPool.Reset();
}
void UVideoShellWidget::UpdateDetectionOverlay(
    const TArray<FVector4>& Boxes,
    const TArray<FString>& Labels,
    int32 ImageWidth,
    int32 ImageHeight)
{
    if (!DetectionOverlay || ImageWidth <= 0 || ImageHeight <= 0)
    {
        return;
    }

    const FVector2D OverlaySize =
        DetectionOverlay->GetCachedGeometry().GetLocalSize();

    if (OverlaySize.X <= 0.0f || OverlaySize.Y <= 0.0f)
    {
        return;
    }

    const int32 Count = FMath::Min(Boxes.Num(), Labels.Num());

    // 鎸?object-fit: contain 璁＄畻瑙嗛瀹為檯鏄剧ず鍖哄煙
    const float Scale = FMath::Min(
        OverlaySize.X / static_cast<float>(ImageWidth),
        OverlaySize.Y / static_cast<float>(ImageHeight));

    const float VideoWidth = ImageWidth * Scale;
    const float VideoHeight = ImageHeight * Scale;

    const float OffsetX = (OverlaySize.X - VideoWidth) * 0.5f;
    const float OffsetY = (OverlaySize.Y - VideoHeight) * 0.5f;

    // 鍒涘缓鎺т欢姹犱腑缂哄皯鐨勬娴嬫
    while (DetectionBoxPool.Num() < Count)
    {
        UCanvasPanel* BoxCanvas =
            WidgetTree->ConstructWidget<UCanvasPanel>(
                UCanvasPanel::StaticClass());

        DetectionOverlay->AddChildToCanvas(BoxCanvas);

        // 缁胯壊杈规
        for (int32 Side = 0; Side < 4; ++Side)
        {
            UBorder* Border =
                WidgetTree->ConstructWidget<UBorder>();

            Border->SetBrushColor(
                FLinearColor(0.1f, 1.0f, 0.2f, 1.0f));

            BoxCanvas->AddChildToCanvas(Border);
        }

        // 绫诲埆涓庣疆淇″害鏂囧瓧
        UTextBlock* Label =
            WidgetTree->ConstructWidget<UTextBlock>();

        Label->SetColorAndOpacity(
            FSlateColor(FLinearColor(0.1f, 1.0f, 0.2f, 1.0f)));

        Label->SetShadowColorAndOpacity(
            FLinearColor::Black);

        Label->SetShadowOffset(FVector2D(1.0f, 1.0f));

        BoxCanvas->AddChildToCanvas(Label);

        DetectionBoxPool.Add(BoxCanvas);
    }

    // 鏇存柊姣忎釜妫€娴嬫
    for (int32 Index = 0; Index < DetectionBoxPool.Num(); ++Index)
    {
        UCanvasPanel* BoxCanvas = DetectionBoxPool[Index];

        if (!BoxCanvas)
        {
            continue;
        }

        if (Index >= Count)
        {
            BoxCanvas->SetVisibility(ESlateVisibility::Collapsed);
            continue;
        }

        BoxCanvas->SetVisibility(ESlateVisibility::Visible);

        const FVector4& Box = Boxes[Index];

        const float X1 = FMath::Clamp(Box.X, 0.0f, static_cast<float>(ImageWidth));
        const float Y1 = FMath::Clamp(Box.Y, 0.0f, static_cast<float>(ImageHeight));
        const float X2 = FMath::Clamp(Box.Z, 0.0f, static_cast<float>(ImageWidth));
        const float Y2 = FMath::Clamp(Box.W, 0.0f, static_cast<float>(ImageHeight));

        if (X2 <= X1 || Y2 <= Y1)
        {
            BoxCanvas->SetVisibility(ESlateVisibility::Collapsed);
            continue;
        }

        const float Left = OffsetX + X1 * Scale;
        const float Top = OffsetY + Y1 * Scale;
        const float BoxWidth = (X2 - X1) * Scale;
        const float BoxHeight = (Y2 - Y1) * Scale;

        UCanvasPanelSlot* BoxSlot =
            Cast<UCanvasPanelSlot>(BoxCanvas->Slot);

        if (BoxSlot)
        {
            BoxSlot->SetPosition(FVector2D(Left, Top));
            BoxSlot->SetSize(FVector2D(BoxWidth, BoxHeight));
        }

        const float Thickness = 2.0f;

        const FVector2D Positions[] =
        {
            FVector2D(0.0f, 0.0f),
            FVector2D(0.0f, FMath::Max(0.0f, BoxHeight - Thickness)),
            FVector2D(0.0f, 0.0f),
            FVector2D(FMath::Max(0.0f, BoxWidth - Thickness), 0.0f)
        };

        const FVector2D Sizes[] =
        {
            FVector2D(BoxWidth, Thickness),
            FVector2D(BoxWidth, Thickness),
            FVector2D(Thickness, BoxHeight),
            FVector2D(Thickness, BoxHeight)
        };
        for (int32 Side = 0; Side < 4; ++Side)
        {
            UWidget* Child = BoxCanvas->GetChildAt(Side);

            UCanvasPanelSlot* SideSlot =
                Child ? Cast<UCanvasPanelSlot>(Child->Slot) : nullptr;

            if (SideSlot)
            {
                SideSlot->SetPosition(Positions[Side]);
                SideSlot->SetSize(Sizes[Side]);
            }
        }

        // 鏇存柊鏂囧瓧
        UTextBlock* LabelWidget =
            Cast<UTextBlock>(BoxCanvas->GetChildAt(4));

        if (LabelWidget)
        {
            LabelWidget->SetText(FText::FromString(Labels[Index]));

            UCanvasPanelSlot* LabelSlot =
                Cast<UCanvasPanelSlot>(LabelWidget->Slot);

            if (LabelSlot)
            {
                LabelSlot->SetPosition(FVector2D(2.0f, 2.0f));
                LabelSlot->SetSize(FVector2D(
                    FMath::Max(80.0f, BoxWidth - 4.0f),
                    20.0f));
            }
        }
    }
}

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
    const bool WaitingForProgress=MediaState==TEXT("CONNECTING") || MediaState==TEXT("RETRYING")
        || MediaState==TEXT("PAGE READY / WAITING VIDEO") || MediaState==TEXT("PLAYING");
    if (!SourceUrl.IsEmpty() && WaitingForProgress && Now - LastProgress > 10)
        MediaState = bPageReady ? TEXT("STALLED / NO FRAMES") : TEXT("OFFLINE / LOAD FAILED");
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
    bPageReady = false; bHasDecodedFrame = false; Width = Height = 0; Fps = -1; LastMediaTime = LastFrames = -1; LastSample = LastProbe = 0;
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
    if(W>0 && H>0 && (Frames>0 || T>0))bHasDecodedFrame=true;
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
    else if (Paused && T >= 0 && bHasDecodedFrame) MediaState = TEXT("PAUSED / ENDED");
    else if (!Paused && MediaState == TEXT("PAUSED / ENDED"))
    {
        // Resume can precede the next decoded frame. Start a fresh wait once;
        // repeated unchanged probes must not postpone the stall watchdog.
        MediaState = TEXT("PAGE READY / WAITING VIDEO");
        LastProgress = Now;
    }
    Width = FMath::Clamp(W, 0.0, 16384.0); Height = FMath::Clamp(H, 0.0, 16384.0);
    LastSample = SampleTime; LastMediaTime = T; LastFrames = Frames;
    UpdateLabels();
}

void UVideoShellWidget::UpdateLabels()
{
    if (!Status || !Details) return;
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    const bool Switching=Sync && Sync->IsVideoSwitchPending();
    const FString SwitchError=Sync?Sync->ViewSyncError:FString();
    const FText Feedback=Switching?FText::Format(ProductText::Get(TEXT("VideoUI.Switching")),FText::AsCultureInvariant(Sync->GetPendingVideoTarget()))
        :!SwitchError.IsEmpty()?ProductText::Get(TEXT("Errors.")+SwitchError):ProductText::Source(MediaState);
    Status->SetText(Feedback);Status->SetToolTipText(Feedback);
    Status->SetColorAndOpacity(FSlateColor(!Switching && SwitchError.IsEmpty() && MediaState == TEXT("PLAYING") ? FLinearColor(0.2f, 0.9f, 0.55f) : FLinearColor(1, 0.7f, 0.3f)));
    const FString Resolution = Width > 0 ? FString::Printf(TEXT("%d x %d"), Width, Height) : TEXT("N/A");
    const FString Rate = Fps >= 0 && MediaState == TEXT("PLAYING") ? FString::Printf(TEXT("%.0f measured"), Fps) : TEXT("N/A");
    Details->SetText(FText::Format(ProductText::Get(TEXT("Video.Details")),FText::AsCultureInvariant(DisplayedDroneId>0?FString::Printf(TEXT("UAV-%02d"),DisplayedDroneId):TEXT("")),FText::AsCultureInvariant(Resolution),FText::AsCultureInvariant(Rate),ProductText::Source(MediaState)));
    Retry->SetIsEnabled(!bStopped && !Switching && !SourceUrl.IsEmpty());
    RetryIcon->SetVisibility(Retry->GetIsEnabled()?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
    RetryReadyIcon->SetVisibility(Retry->GetIsEnabled()?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    FigmaUI::Text(RetryCaption,12,Retry->GetIsEnabled()?FigmaUI::PrimaryText:FLinearColor(FColor(120,126,147)),true);
    Browser->SetVisibility(SourceUrl.IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
    // Keep a newly selected source covered until it decodes. A paused confirmed
    // source retains its frame and visible browser playback controls.
    const bool Paused=MediaState==TEXT("PAUSED / ENDED");
    const bool ShowFrame=bHasDecodedFrame && (MediaState==TEXT("PLAYING") || Paused);
    MediaCover->SetVisibility(ShowFrame?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
    EmptyVideo->SetText(DisplayedDroneId<=0?ProductText::Get(TEXT("VideoUI.NoTarget")):ProductText::Source(SourceUrl.IsEmpty()?TEXT("NO SOURCE"):*MediaState));
    const bool Failed=MediaState==TEXT("STALLED / NO FRAMES") || MediaState==TEXT("OFFLINE / LOAD FAILED") || MediaState==TEXT("OFFLINE / MEDIA ERROR");
    EmptyHint->SetText(ProductText::Get(DisplayedDroneId<=0?TEXT("VideoUI.SelectSource"):SourceUrl.IsEmpty()?TEXT("VideoUI.ConfigureSource"):
        Failed?TEXT("VideoUI.FailedHint"):Paused?TEXT("VideoUI.PausedHint"):TEXT("VideoUI.ConnectingHint")));
    if(Paused && !Switching && SwitchError.IsEmpty())Status->SetToolTipText(ProductText::Get(TEXT("VideoUI.PausedHint")));
    Retry->SetToolTipText(Switching?Feedback:ProductText::Get(Retry->GetIsEnabled()?TEXT("VideoUI.RetryHint"):TEXT("VideoUI.RetryUnavailable")));
}

void UVideoShellWidget::RetrySource() { if (!bStopped && Browser && !SourceUrl.IsEmpty() && !GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->IsVideoSwitchPending()) { SetSource(DisplayedDroneId, SourceUrl); MediaState=TEXT("RETRYING"); UpdateLabels(); } }
void UVideoShellWidget::Shutdown()
{
    StopDetectionReceiver();
    ClearDetectionOverlay();

    if (bStopped) return;

    bStopped = true;
    ++Generation;
    bPageReady = false;

    if (Browser) Browser->LoadURL(TEXT("about:blank"));

    MediaState = TEXT("OFFLINE / CLOSED");
    UpdateLabels();

    UE_LOG(LogTemp, Log, TEXT("[Video] Browser stopped"));
}
void UVideoShellWidget::Resume()
{
    if (bStopped)
    {
        bStopped = false;
        SetSource(DisplayedDroneId, SourceUrl);
        Refresh();
    }

    StartDetectionReceiver();
}
void UVideoShellWidget::NativeDestruct() { Shutdown(); Super::NativeDestruct(); }

void UVideoShellWidget::SetFollowTarget(bool Enabled) { bFollowTarget=false; }
void UVideoShellWidget::Chinese(){GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->SetLanguage(TEXT("zh-Hans"));}
void UVideoShellWidget::English(){GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->SetLanguage(TEXT("en"));}
bool UVideoShellWidget::SetAsActiveUAV() {
    if(bStopped || DisplayedDroneId<=0)return false;
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    return Sync && Sync->IsEnabled() && Sync->SetActiveUAV(DisplayedDroneId);
}

