#pragma once
#include "CoreMinimal.h"
#include "DroneOps/Core/DroneOpsTypes.h"
#include "UObject/Object.h"
#include "CommandScreenManager.generated.h"

// Shared startup role contract for Command, Video and legacy Standalone.
UENUM(BlueprintType)
enum class EDroneClientRole : uint8 { Standalone, Command, Video, Map };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommandOpenVideoRequested, int32, DroneId);

/** PC-owned UI lifetime. No network transport or independent selected-drone state. */
UCLASS()
class UE5DRONECONTROL_API UCommandScreenManager : public UObject
{
    GENERATED_BODY()
public:
    static EDroneClientRole ResolveClientRole();
    static EDroneClientRole ResolveClientRoleFromSettings(const TCHAR* CommandLine, const FString& DefaultRole);
    void Initialize(class ADroneOpsPlayerController* Owner);
    UFUNCTION(BlueprintCallable) void Show();
    UFUNCTION(BlueprintCallable) void Hide();
    UFUNCTION(BlueprintCallable) void Destroy();
    UFUNCTION(BlueprintPure) EDroneClientRole GetClientRole() const { return ClientRole; }
    class UCommandMapInteractionService* GetMapService() const { return MapService; }
    class UCommandShellWidget* GetShell() const { return Shell; }
    class UMapShellWidget* GetMapShell() const { return MapShell; }
    class UVideoShellWidget* GetVideoShell() const { return VideoShell; }
    class UDroneRegistrySubsystem* GetRegistry() const { return Registry.Get(); }
    class USequenceDispatchPanelWidget* ShowPathPanel();
    void HidePathPanel();
    void ShowGeographicPanel(bool bExpand = false);
    void HideGeographicPanel();
    void ToggleGeographicPanel();
    void ShowDroneList(bool bVisible);
    bool IsCursorOverMap() const;
    void RequestOpenVideo();
    void TogglePathTools();
    void ToggleSpeedPanel();
    UPROPERTY(BlueprintAssignable) FCommandOpenVideoRequested OnRequestOpenVideo;
private:
    TWeakObjectPtr<class ADroneOpsPlayerController> Controller;
    TWeakObjectPtr<class UDroneRegistrySubsystem> Registry;
    UPROPERTY() TObjectPtr<class UCommandShellWidget> Shell;
    UPROPERTY() TObjectPtr<class UMapShellWidget> MapShell;
    UPROPERTY() TObjectPtr<class UVideoShellWidget> VideoShell;
    UPROPERTY() TObjectPtr<class UCommandMapInteractionService> MapService;
    UPROPERTY() TObjectPtr<class USequenceDispatchPanelWidget> PathPanel;
    UPROPERTY() TObjectPtr<class UGeographicTargetPanelWidget> GeographicPanel;
    UPROPERTY() TObjectPtr<class UDefaultSegmentSpeedWidget> SpeedPanel;
    EDroneClientRole ClientRole = EDroneClientRole::Standalone;
    FTimerHandle RefreshTimer;
    bool bSelectionDirty = false;
    bool bFollowSelection = true;
    void Refresh();
    UFUNCTION() void SelectionChanged(int32 OldId, int32 NewId);
    UFUNCTION() void DroneChanged(int32 Id);
    UFUNCTION() void TelemetryChanged(int32 Id, const FDroneTelemetrySnapshot& Snapshot);
    UFUNCTION() void TaskChanged(int32 Id, const FDroneTaskStateSnapshot& Snapshot);
};
