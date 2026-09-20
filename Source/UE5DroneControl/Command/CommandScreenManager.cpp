#include "Command/CommandScreenManager.h"
#include "Video/VideoShellWidget.h"
#include "Map/MapShellWidget.h"
#include "Command/CommandShellWidget.h"
#include "Command/CommandMapInteractionService.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Control/DroneOpsGameMode.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "UI/SequenceDispatchPanelWidget.h"
#include "UI/GeographicTargetPanelWidget.h"
#include "UI/DefaultSegmentSpeedWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

EDroneClientRole UCommandScreenManager::ResolveClientRole()
{
    FString DefaultRole = TEXT("Standalone");
    GConfig->GetString(TEXT("CommandClient"), TEXT("DefaultClientRole"), DefaultRole, GGameIni);
    return ResolveClientRoleFromSettings(FCommandLine::Get(), DefaultRole);
}
EDroneClientRole UCommandScreenManager::ResolveClientRoleFromSettings(const TCHAR* CommandLine, const FString& DefaultRole)
{
    FString Role = DefaultRole;
    FParse::Value(CommandLine, TEXT("ClientRole="), Role);
    Role.TrimStartAndEndInline();
    return Role.Equals(TEXT("Command"), ESearchCase::IgnoreCase) ? EDroneClientRole::Command
        : Role.Equals(TEXT("Map"), ESearchCase::IgnoreCase) ? EDroneClientRole::Map
        : (Role.Equals(TEXT("Video"), ESearchCase::IgnoreCase) ? EDroneClientRole::Video : EDroneClientRole::Standalone);
}
void UCommandScreenManager::Initialize(ADroneOpsPlayerController* Owner)
{
    if (Controller == Owner && (VideoShell || Shell || MapShell)) { Show(); return; }
    Destroy();
    Controller = Owner;
    if (!Owner || !Owner->IsLocalController()) return;
    ClientRole = ResolveClientRole();
    if (ClientRole == EDroneClientRole::Map) GConfig->GetBool(TEXT("OperationalContext"), TEXT("FollowSelection"), bFollowSelection, GGameIni);
    if (ClientRole == EDroneClientRole::Video)
    {
        Registry = Owner->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
        Show();
        Owner->GetWorldTimerManager().SetTimer(RefreshTimer, this, &UCommandScreenManager::Refresh, 0.25f, true);
        UE_LOG(LogTemp, Log, TEXT("[Video] role=Video Command Shell skipped; Video Shell created"));
        return;
    }
    Registry = Owner->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    const ADroneOpsGameMode* GameMode = Owner->GetWorld()->GetAuthGameMode<ADroneOpsGameMode>();
    if (ClientRole == EDroneClientRole::Map || ClientRole == EDroneClientRole::Standalone)
    {
        MapService = ClientRole == EDroneClientRole::Map && GameMode ? GameMode->GetCommandMapService() : nullptr;
        if (!MapService) MapService = NewObject<UCommandMapInteractionService>(this);
        MapService->Initialize(Owner);
    }
    if (Registry.IsValid())
    {
        Registry->OnPrimarySelectionChanged.AddUniqueDynamic(this, &UCommandScreenManager::SelectionChanged);
        Registry->OnDroneRegistered.AddUniqueDynamic(this, &UCommandScreenManager::DroneChanged);
        Registry->OnTelemetryUpdated.AddUniqueDynamic(this, &UCommandScreenManager::TelemetryChanged);
        Registry->OnDroneTaskStateUpdated.AddUniqueDynamic(this, &UCommandScreenManager::TaskChanged);
    }
    Show();
    // Coalesces per-drone telemetry bursts; also handles deletion and late-spawned actors.
    Owner->GetWorldTimerManager().SetTimer(RefreshTimer, this, &UCommandScreenManager::Refresh, 0.25f, true);
    bSelectionDirty = true;
    UE_LOG(LogTemp, Log, TEXT("[Command] Shell created, role=%s"), *StaticEnum<EDroneClientRole>()->GetNameStringByValue(static_cast<int64>(ClientRole)));
}

void UCommandScreenManager::Show()
{
    if (!Controller.IsValid()) return;
    if (ClientRole == EDroneClientRole::Video)
    {
        if (!VideoShell)
        {
            VideoShell = CreateWidget<UVideoShellWidget>(Controller.Get(), UVideoShellWidget::StaticClass());
            VideoShell->InitializeRegistry(Registry.Get());
            VideoShell->AddToViewport(30);
        }
        VideoShell->SetVisibility(ESlateVisibility::Visible);
        VideoShell->Resume();
        VideoShell->Refresh();
        return;
    }
    if (ClientRole == EDroneClientRole::Map)
    {
        if (!MapShell)
        {
            MapShell = CreateWidget<UMapShellWidget>(Controller.Get(), UMapShellWidget::StaticClass());
            MapShell->InitializeManager(this);
            MapShell->AddToViewport(30);
            UE_LOG(LogTemp, Log, TEXT("[Map] unique MapShell created; CommandShell=0 VideoShell=0"));
        }
        MapShell->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        Refresh();
        return;
    }
    if (!Shell)
    {
        Shell = CreateWidget<UCommandShellWidget>(Controller.Get(), UCommandShellWidget::StaticClass());
        Shell->InitializeManager(this);
        Shell->AddToViewport(30);
    }
    Shell->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Refresh();
}
void UCommandScreenManager::Hide()
{
    if (VideoShell) { VideoShell->Shutdown(); VideoShell->SetVisibility(ESlateVisibility::Collapsed); }
    if (MapShell) MapShell->SetVisibility(ESlateVisibility::Collapsed);
    if (Shell) Shell->SetVisibility(ESlateVisibility::Collapsed);
}
void UCommandScreenManager::Destroy()
{
    if (Controller.IsValid()) Controller->GetWorldTimerManager().ClearTimer(RefreshTimer);
    if (Registry.IsValid())
    {
        Registry->OnPrimarySelectionChanged.RemoveAll(this);
        Registry->OnDroneRegistered.RemoveAll(this);
        Registry->OnTelemetryUpdated.RemoveAll(this);
        Registry->OnDroneTaskStateUpdated.RemoveAll(this);
    }
    if (MapService) MapService->Unbind();
    if (PathPanel) PathPanel->RemoveFromParent();
    if (GeographicPanel) GeographicPanel->RemoveFromParent();
    if (SpeedPanel) SpeedPanel->RemoveFromParent();
    if (VideoShell) { VideoShell->Shutdown(); VideoShell->RemoveFromParent(); VideoShell = nullptr; }
    if (MapShell) { MapShell->RemoveFromParent(); MapShell = nullptr; }
    if (Shell) Shell->RemoveFromParent();
    PathPanel = nullptr; GeographicPanel = nullptr; SpeedPanel = nullptr; Shell = nullptr; MapService = nullptr;
    Registry.Reset(); Controller.Reset();
    bSelectionDirty = false;
}
void UCommandScreenManager::SelectionChanged(int32 OldId, int32 NewId)
{
    // Delegate projection is immediate; focus is deferred until the map Actor exists.
    if (Controller.IsValid() && MapService) Controller->RefreshCommandSelection();
    bSelectionDirty = true;
    Refresh();
}
void UCommandScreenManager::DroneChanged(int32 Id) { Refresh(); }
void UCommandScreenManager::TelemetryChanged(int32 Id, const FDroneTelemetrySnapshot& Snapshot)
{
    // The shell reads Registry at the next coalesced refresh; no second telemetry cache.
}
void UCommandScreenManager::TaskChanged(int32 Id, const FDroneTaskStateSnapshot& Snapshot) { Refresh(); }
void UCommandScreenManager::Refresh()
{
    if (VideoShell) VideoShell->Refresh();
    if (MapShell) MapShell->Refresh();
    if (Shell) Shell->Refresh();
    if (bFollowSelection && bSelectionDirty && Registry.IsValid() && MapService)
    {
        const int32 Id = Registry->GetPrimarySelectedDrone();
        if (Id <= 0 || MapService->FocusDrone(Id)) bSelectionDirty = false;
    }
}
USequenceDispatchPanelWidget* UCommandScreenManager::ShowPathPanel()
{
    if ((!Shell && !MapShell) || !MapService || !Controller.IsValid()) return nullptr;
    if (!PathPanel)
    {
        UClass* Class = LoadClass<USequenceDispatchPanelWidget>(nullptr,
            TEXT("/Game/DroneOps/UI/WBP_SequenceDispatchPanel.WBP_SequenceDispatchPanel_C"));
        PathPanel = CreateWidget<USequenceDispatchPanelWidget>(Controller.Get(),
            Class ? Class : USequenceDispatchPanelWidget::StaticClass());
        if (MapShell) MapShell->AttachCenterWidget(PathPanel);
        else Shell->AttachCenterWidget(PathPanel);
        MapService->SetPathPanel(PathPanel);
        if (ClientRole == EDroneClientRole::Map) PathPanel->SetCommandPresentation();
    }
    if (ClientRole != EDroneClientRole::Map) PathPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    return PathPanel;
}
void UCommandScreenManager::HidePathPanel()
{
    // Keep a single panel and its existing edit/playback lifecycle for this controller.
    if (PathPanel) PathPanel->SetVisibility(ESlateVisibility::Collapsed);
}
void UCommandScreenManager::ShowGeographicPanel(bool bExpand)
{
    if ((!Shell && !MapShell) || !MapService || !Controller.IsValid()) return;
    if (bExpand && ClientRole == EDroneClientRole::Map)
    {
        HidePathPanel();
        if (SpeedPanel) SpeedPanel->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (!GeographicPanel)
    {
        UClass* Class = LoadClass<UGeographicTargetPanelWidget>(nullptr,
            TEXT("/Game/DroneOps/UI/WBP_GeographicTargetPanel.WBP_GeographicTargetPanel_C"));
        GeographicPanel = CreateWidget<UGeographicTargetPanelWidget>(Controller.Get(),
            Class ? Class : UGeographicTargetPanelWidget::StaticClass());
        if (MapShell) MapShell->AttachCenterWidget(GeographicPanel);
        else Shell->AttachCenterWidget(GeographicPanel);
        if (ClientRole == EDroneClientRole::Map) GeographicPanel->ConfigureCommandLayout();
    }
    GeographicPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (bExpand) GeographicPanel->OpenCommandInput();
    else if (ClientRole == EDroneClientRole::Map) GeographicPanel->SetVisibility(ESlateVisibility::Collapsed);
}
void UCommandScreenManager::HideGeographicPanel()
{
    if (GeographicPanel) GeographicPanel->SetVisibility(ESlateVisibility::Collapsed);
}
void UCommandScreenManager::ToggleGeographicPanel()
{
    if (GeographicPanel && GeographicPanel->IsVisible()) HideGeographicPanel();
    else ShowGeographicPanel(true);
}
void UCommandScreenManager::ShowDroneList(bool bVisible) { if (Shell) Shell->ShowDroneList(bVisible); }
bool UCommandScreenManager::IsCursorOverMap() const { return MapShell ? MapShell->IsCursorOverMap() : ClientRole == EDroneClientRole::Standalone && (!Shell || Shell->IsCursorOverMap()); }
void UCommandScreenManager::RequestOpenVideo()
{
    if (Registry.IsValid() && Registry->GetPrimarySelectedDrone() > 0)
        OnRequestOpenVideo.Broadcast(Registry->GetPrimarySelectedDrone());
}

void UCommandScreenManager::TogglePathTools()
{
    if (ClientRole == EDroneClientRole::Map)
    {
        HideGeographicPanel();
        if (SpeedPanel) SpeedPanel->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (USequenceDispatchPanelWidget* Panel = ShowPathPanel()) Panel->ToggleCommandTools();
}
void UCommandScreenManager::ToggleSpeedPanel()
{
    if ((!Shell && !MapShell) || !MapService || !Controller.IsValid()) return;
    if (ClientRole == EDroneClientRole::Map) { HidePathPanel(); HideGeographicPanel(); }
    if (!SpeedPanel)
    {
        SpeedPanel = CreateWidget<UDefaultSegmentSpeedWidget>(Controller.Get(), UDefaultSegmentSpeedWidget::StaticClass());
        SpeedPanel->SetPathEditController(Controller.Get());
        SpeedPanel->SetDisplayedSpeed(Controller->GetEditDefaultSegmentSpeed());
        SpeedPanel->ShowOverlay();
        if (MapShell) MapShell->AttachCenterWidget(SpeedPanel);
        else Shell->AttachCenterWidget(SpeedPanel);
    }
    else SpeedPanel->SetVisibility(SpeedPanel->IsVisible() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}
