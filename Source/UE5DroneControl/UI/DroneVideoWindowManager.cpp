// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/DroneVideoWindowManager.h"
#include "UI/DroneVideoWindowWidget.h"

#include "Blueprint/UserWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericApplication.h"
#include "Widgets/SWindow.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "Stats/Stats.h"
#include "Serialization/JsonWriter.h"
#include "Containers/StringConv.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool UDroneVideoWindowManager::OpenVideoWindow(int32 DroneId, const FString& DroneName, const FString& VideoUrl, FString& OutError)
{
	if (DroneId <= 0)
	{
		OutError = TEXT("无效的无人机");
		return false;
	}

	// Re-check on an already-open drone: only activate + bring its window to front.
	if (TSharedRef<FVideoWindow>* Existing = Windows.Find(DroneId))
	{
		if ((*Existing)->Window.IsValid())
		{
			(*Existing)->Window->BringToFront(true);
			(*Existing)->Window->ShowWindow();
			return true;
		}
		// Stale record with a dead window — drop it and fall through to recreate.
		Windows.Remove(DroneId);
	}

	if (VideoUrl.IsEmpty())
	{
		OutError = TEXT("该无人机未配置视频地址");
		return false;
	}

	if (Windows.Num() >= MaxConcurrentWindows)
	{
		OutError = FString::Printf(TEXT("最多同时播放 %d 路视频"), MaxConcurrentWindows);
		return false;
	}

	if (!WindowContentClass)
	{
		OutError = TEXT("视频窗口控件类未配置");
		return false;
	}

	if (!FSlateApplication::IsInitialized())
	{
		OutError = TEXT("Slate 未初始化，无法创建视频窗口");
		return false;
	}

	// Own the widget under the player controller so it has a valid world/owning
	// player; the manager is outered to that controller.
	APlayerController* OwningPC = Cast<APlayerController>(GetOuter());
	UDroneVideoWindowWidget* Widget = OwningPC
		? CreateWidget<UDroneVideoWindowWidget>(OwningPC, WindowContentClass)
		: CreateWidget<UDroneVideoWindowWidget>(GetWorld(), WindowContentClass);
	if (!Widget)
	{
		OutError = TEXT("无法创建视频窗口控件");
		return false;
	}

	TSharedRef<FVideoWindow> Record = MakeShared<FVideoWindow>();
	Record->Widget = Widget;
	LiveWidgets.Add(Widget); // keep rooted against GC for the window's lifetime

	const FText WindowTitle = FText::FromString(
		DroneName.IsEmpty()
			? FString::Printf(TEXT("无人机视频 - DroneId %d"), DroneId)
			: FString::Printf(TEXT("%s (DroneId %d)"), *DroneName, DroneId));

	TSharedRef<SWindow> NewWindow = SNew(SWindow)
		.Title(WindowTitle)
		.ClientSize(FVector2D(960.0f, 540.0f))
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		.IsInitiallyMaximized(false);

	// TakeWidget() builds the Slate content that the SWindow hosts; the widget
	// object is separately strong-held in LiveWidgets to keep UObject and Slate
	// lifetimes consistent.
	NewWindow->SetContent(Widget->TakeWidget());

	// The OS title-bar close box must route into the same cleanup as everything
	// else. Bind before AddWindow; the lambda captures only DroneId + this.
	NewWindow->SetOnWindowClosed(FOnWindowClosed::CreateUObject(
		this, &UDroneVideoWindowManager::HandleNativeWindowClosed, DroneId));

	Record->Window = NewWindow;
	Windows.Add(DroneId, Record);

	// Widget-side controls (UMG close / fullscreen) funnel back to the manager.
	Widget->OnCloseRequested.AddUObject(this, &UDroneVideoWindowManager::HandleWidgetCloseRequested);
	Widget->OnFullscreenToggleRequested.AddUObject(this, &UDroneVideoWindowManager::HandleWidgetFullscreenToggle);

	FSlateApplication::Get().AddWindow(NewWindow);
	NewWindow->BringToFront(true);

	Widget->SetDroneContext(DroneId, DroneName, VideoUrl);

    // The normal Video role owns the detector port. Opt in when debugging the detached window on its own.
    if (FParse::Param(FCommandLine::Get(), TEXT("YoloDebugOverlay")))
    {
        StartDetectionReceiver();
    }

    return true;
}

bool UDroneVideoWindowManager::HasWindow(int32 DroneId) const
{
	const TSharedRef<FVideoWindow>* Found = Windows.Find(DroneId);
	return Found && (*Found)->Window.IsValid();
}

void UDroneVideoWindowManager::CloseVideoWindow(int32 DroneId)
{
	TSharedRef<FVideoWindow>* FoundPtr = Windows.Find(DroneId);
	if (!FoundPtr)
	{
		return;
	}

	// Take a local ref and remove the record up front so any re-entrant close
	// (native OnWindowClosed firing during RequestDestroyWindow) is a no-op.
	TSharedRef<FVideoWindow> Record = *FoundPtr;
	Windows.Remove(DroneId);

	// 1) Stop video / navigate about:blank.
	if (Record->Widget)
	{
		Record->Widget->ShutdownWidget();
	}

	// 2) Unbind callbacks (widget delegates + native window closed handler).
	if (Record->Widget)
	{
		Record->Widget->OnCloseRequested.RemoveAll(this);
		Record->Widget->OnFullscreenToggleRequested.RemoveAll(this);
	}
	if (Record->Window.IsValid())
	{
		Record->Window->SetOnWindowClosed(FOnWindowClosed());
	}

	// 3) Destroy the native window.
	if (Record->Window.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().RequestDestroyWindow(Record->Window.ToSharedRef());
	}
	Record->Window.Reset();

	// 4) Remove records / release the widget root so it can be GC'd.
	if (Record->Widget)
	{
		LiveWidgets.Remove(Record->Widget);
		Record->Widget = nullptr;
	}

	// 最后一个视频窗口关闭后，释放 UDP 接收端口。
    if (Windows.Num() == 0)
    {
            StopDetectionReceiver();
    }

	// 5) Tell listeners (the BP row's checkbox) this drone's window is gone, so it can
	//    reflect the closed state. Broadcast last, after the record is fully torn down.
	OnWindowClosed.Broadcast(DroneId);
}

void UDroneVideoWindowManager::CloseAllVideoWindows()
{
	TArray<int32> OpenIds;
	Windows.GetKeys(OpenIds);
	for (int32 DroneId : OpenIds)
	{
		CloseVideoWindow(DroneId);
	}
	Windows.Empty();
	LiveWidgets.Empty();
}

void UDroneVideoWindowManager::ToggleFullscreen(int32 DroneId)
{
	TSharedRef<FVideoWindow>* FoundPtr = Windows.Find(DroneId);
	if (!FoundPtr || !(*FoundPtr)->Window.IsValid() || !FSlateApplication::IsInitialized())
	{
		return;
	}

	TSharedRef<FVideoWindow> Record = *FoundPtr;
	const TSharedRef<SWindow> Window = Record->Window.ToSharedRef();

	if (!Record->bIsFullscreen)
	{
		// Capture the exact pre-fullscreen geometry so restore is faithful.
		Record->RestoreScreenPosition = Window->GetPositionInScreen();
		Record->RestoreSize = Window->GetSizeInScreen();

		// Find the monitor that currently contains the window's center and fill it.
		const FVector2D Center = Record->RestoreScreenPosition + Record->RestoreSize * 0.5f;

		FDisplayMetrics Metrics;
		FSlateApplication::Get().GetInitialDisplayMetrics(Metrics);

		FVector2D TargetPos(Metrics.PrimaryDisplayWorkAreaRect.Left, Metrics.PrimaryDisplayWorkAreaRect.Top);
		FVector2D TargetSize(
			Metrics.PrimaryDisplayWorkAreaRect.Right - Metrics.PrimaryDisplayWorkAreaRect.Left,
			Metrics.PrimaryDisplayWorkAreaRect.Bottom - Metrics.PrimaryDisplayWorkAreaRect.Top);

		for (const FMonitorInfo& Monitor : Metrics.MonitorInfo)
		{
			const FPlatformRect& R = Monitor.DisplayRect;
			if (Center.X >= R.Left && Center.X < R.Right && Center.Y >= R.Top && Center.Y < R.Bottom)
			{
				TargetPos = FVector2D(R.Left, R.Top);
				TargetSize = FVector2D(R.Right - R.Left, R.Bottom - R.Top);
				break;
			}
		}

		Window->ReshapeWindow(TargetPos, TargetSize);
		Record->bIsFullscreen = true;
	}
	else
	{
		// Restore to the pre-fullscreen screen position and size.
		if (Record->RestoreSize.X > 0.0f && Record->RestoreSize.Y > 0.0f)
		{
			Window->ReshapeWindow(Record->RestoreScreenPosition, Record->RestoreSize);
		}
		Record->bIsFullscreen = false;
	}

	Window->BringToFront(true);
	if (Record->Widget)
	{
		Record->Widget->NotifyFullscreenChanged(Record->bIsFullscreen);
	}
}

void UDroneVideoWindowManager::HandleWidgetCloseRequested(int32 DroneId)
{
	CloseVideoWindow(DroneId);
}

void UDroneVideoWindowManager::HandleWidgetFullscreenToggle(int32 DroneId)
{
	ToggleFullscreen(DroneId);
}

void UDroneVideoWindowManager::HandleNativeWindowClosed(const TSharedRef<SWindow>& ClosedWindow, int32 DroneId)
{
	// OS title-bar X (or Alt+F4). Funnel into the single cleanup path. If the
	// close originated from CloseVideoWindow itself, the record is already gone
	// and this is a no-op.
	CloseVideoWindow(DroneId);
}

// ============================================================
// YOLO UDP Detection Receiver
// ============================================================

bool UDroneVideoWindowManager::StartDetectionReceiver()
{
        if (DetectionSocket)
        {
                return true;
        }

        ISocketSubsystem* SocketSubsystem =
                ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

        if (!SocketSubsystem)
        {
                UE_LOG(LogTemp, Error,
                        TEXT("[YOLO UDP] SocketSubsystem unavailable"));
                return false;
        }

        DetectionSocket = SocketSubsystem->CreateSocket(
                NAME_DGram,
                TEXT("YOLODetectionReceiver"),
                false
        );

        if (!DetectionSocket)
        {
                UE_LOG(LogTemp, Error,
                        TEXT("[YOLO UDP] Failed to create socket"));
                return false;
        }

        DetectionSocket->SetNonBlocking(true);

        int32 ActualReceiveBufferSize = 0;
        DetectionSocket->SetReceiveBufferSize(
                2 * 1024 * 1024,
                ActualReceiveBufferSize
        );

        TSharedRef<FInternetAddr> Address =
                SocketSubsystem->CreateInternetAddr();

        Address->SetAnyAddress();
        Address->SetPort(DetectionUdpPort);

        if (!DetectionSocket->Bind(*Address))
        {
                UE_LOG(LogTemp, Error,
                        TEXT("[YOLO UDP] Detached debug overlay could not bind UDP port %d. Stop the main Video role or other detector listener before reopening the debug window."),
                        DetectionUdpPort);

                StopDetectionReceiver();
                return false;
        }

        UE_LOG(LogTemp, Display,
                TEXT("[YOLO UDP] Listening on UDP port %d"),
                DetectionUdpPort);

        return true;
}


void UDroneVideoWindowManager::StopDetectionReceiver()
{
        if (!DetectionSocket)
        {
                return;
        }

        DetectionSocket->Close();

        ISocketSubsystem* SocketSubsystem =
                ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

        if (SocketSubsystem)
        {
                SocketSubsystem->DestroySocket(DetectionSocket);
        }

        DetectionSocket = nullptr;

        UE_LOG(LogTemp, Display,
                TEXT("[YOLO UDP] Receiver stopped"));
}


bool UDroneVideoWindowManager::IsTickable() const
{
        return DetectionSocket != nullptr
                && Windows.Num() > 0;
}


TStatId UDroneVideoWindowManager::GetStatId() const
{
        RETURN_QUICK_DECLARE_CYCLE_STAT(
                UDroneVideoWindowManager,
                STATGROUP_Tickables
        );
}


void UDroneVideoWindowManager::Tick(float DeltaTime)
{
        ReceiveDetectionPackets();
}


void UDroneVideoWindowManager::ReceiveDetectionPackets()
{
        if (!DetectionSocket)
        {
                return;
        }

        constexpr int32 MaxPacketSize = 65507;
        constexpr int32 MaxPacketsPerTick = 128;

        uint8 Buffer[MaxPacketSize];

        int32 ProcessedPackets = 0;

        while (ProcessedPackets < MaxPacketsPerTick)
        {
                uint32 PendingSize = 0;

                if (!DetectionSocket->HasPendingData(PendingSize)
                        || PendingSize == 0)
                {
                        break;
                }

                int32 BytesRead = 0;

                TSharedRef<FInternetAddr> SenderAddress =
                        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)
                        ->CreateInternetAddr();

                const bool bReceived = DetectionSocket->RecvFrom(
                        Buffer,
                        MaxPacketSize,
                        BytesRead,
                        *SenderAddress
                );

                if (!bReceived || BytesRead <= 0)
                {
                        break;
                }

                ++ProcessedPackets;

				FString JsonString;

                // UTF8_TO_TCHAR 需要以 null 结尾。
                // 使用实际字节长度重新构造，避免读取未初始化区域。
                FUTF8ToTCHAR Converted(
                        reinterpret_cast<const ANSICHAR*>(Buffer),
                        BytesRead
                );

                JsonString = FString(
                        Converted.Length(),
                        Converted.Get()
                );

                TSharedPtr<FJsonObject> JsonObject;

                TSharedRef<TJsonReader<>> Reader =
                        TJsonReaderFactory<>::Create(JsonString);

                if (!FJsonSerializer::Deserialize(Reader, JsonObject)
                        || !JsonObject.IsValid())
                {
                        UE_LOG(LogTemp, Warning,
                                TEXT("[YOLO UDP] Invalid JSON packet"));
                        continue;
                }

                double DroneIdValue = 0.0;

                if (!JsonObject->HasTypedField<EJson::Number>(TEXT("drone_id"))
                    || !JsonObject->TryGetNumberField(TEXT("drone_id"), DroneIdValue)
                    || !FMath::IsFinite(DroneIdValue) || DroneIdValue <= 0
                    || DroneIdValue > MAX_int32 || FMath::FloorToDouble(DroneIdValue) != DroneIdValue)
                {
                        continue;
                }

                const int32 DroneId = static_cast<int32>(DroneIdValue);

                TSharedRef<FVideoWindow>* Found =
                        Windows.Find(DroneId);

                if (!Found || !(*Found)->Widget)
                {
                        continue;
                }

                // 重新序列化为安全的 JSON 对象文本，交给 WebBrowser。
                FString SerializedJson;

                TSharedRef<TJsonWriter<>> Writer =
                        TJsonWriterFactory<>::Create(&SerializedJson);

                if (!FJsonSerializer::Serialize(
                        JsonObject.ToSharedRef(),
                        Writer))
                {
                        continue;
                }

                (*Found)->Widget->UpdateDetectionJson(SerializedJson);
        }
}


void UDroneVideoWindowManager::BeginDestroy()
{
        StopDetectionReceiver();

        Super::BeginDestroy();
}
