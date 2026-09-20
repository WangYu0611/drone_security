#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Cesium3DTileset.h"
#include "CesiumRasterOverlayLoadFailureDetails.h"
#include "CommandMapInteractionService.generated.h"

UENUM(BlueprintType)
enum class ECommandMapMode : uint8 { Map2D UMETA(DisplayName="2D"), Map3D UMETA(DisplayName="3D") };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommandMapModeChanged, ECommandMapMode, Mode);

/** Map presentation only. Registry, paths and network retain their existing owners. */
UCLASS()
class UE5DRONECONTROL_API UCommandMapInteractionService : public UObject
{
    GENERATED_BODY()
public:
    static UCommandMapInteractionService* GetOrCreateForWorld(UWorld* World);
    void InitializeWorld(UWorld* World);
    void Initialize(class ADroneOpsPlayerController* Controller);
    void Unbind();
    void SetPathPanel(class USequenceDispatchPanelWidget* Panel);
    UFUNCTION(BlueprintCallable) bool SelectDrone(int32 DroneId);
    UFUNCTION(BlueprintCallable) bool FocusDrone(int32 DroneId);
    UFUNCTION(BlueprintCallable) bool FocusLocation(const FVector& WorldLocation);
    bool FocusExecutionBounds(const FBox& Bounds);
    UFUNCTION(BlueprintCallable) bool FocusAlertOnMap(int32 AlertId);
    UFUNCTION(BlueprintCallable) bool BeginPathPlanning();
    UFUNCTION(BlueprintCallable) bool ConfirmPath();
    UFUNCTION(BlueprintCallable) bool CancelPathPlanning();
    UFUNCTION(BlueprintCallable) bool DeleteSelectedWaypoint();
    UFUNCTION(BlueprintCallable) bool SetSelectedPaused(bool bPaused);
    UFUNCTION(BlueprintPure) bool CanPauseSelected() const;
    UFUNCTION(BlueprintPure) bool IsPlanning() const;
    UFUNCTION(BlueprintPure) ECommandMapMode GetMapMode() const { return CurrentMapMode; }
    UFUNCTION(BlueprintCallable) void SetMapMode(ECommandMapMode Mode);
    UFUNCTION(BlueprintCallable) void Enter2DMode() { SetMapMode(ECommandMapMode::Map2D); }
    UFUNCTION(BlueprintCallable) void Enter3DMode() { SetMapMode(ECommandMapMode::Map3D); }
    UFUNCTION(BlueprintCallable) bool FocusGeographicLocation(double Latitude, double Longitude, double EllipsoidHeightMeters);
    UPROPERTY(BlueprintAssignable) FCommandMapModeChanged OnMapModeChanged;
    void TickView(float DeltaSeconds);
    void PanView(const FVector2D& Delta);
    void ZoomView(float Steps);
    void RotateView(const FVector2D& Delta);
    void ApplyPendingMapMode();
    class ACameraActor* GetMapCamera() const { return MapCamera.Get(); }
    bool OwnsCamera() const { return MapCamera.IsValid(); }
    void LogLayerState() const;
    virtual void BeginDestroy() override;
private:
    struct FLayer
    {
        TWeakObjectPtr<ACesium3DTileset> Actor;
        ETilesetSource OriginalSource = ETilesetSource::FromUrl;
        bool bBasemap = false, bHidden = false, bSuspended = false, bCollision = false;
        TMap<TWeakObjectPtr<class UCesiumRasterOverlay>, bool> RasterActivation;
    };
    ECommandMapMode CurrentMapMode = ECommandMapMode::Map2D;
    TWeakObjectPtr<UWorld> MapWorld;
    TWeakObjectPtr<class ACameraActor> MapCamera;
    TArray<FLayer> Layers;
    bool bWorldInitialized = false;
    bool bModeRequested = false;
    FVector ViewFocus = FVector::ZeroVector;
    double ViewDistance = 15000.0;
    float OriginalHitResultTraceDistance = 100000.f;
    float ViewYaw = 0.0f, View3DPitch = -55.0f;
    void UpdateCameraTransform();
    void ApplyLayers();
    void RasterFailed(const FCesiumRasterOverlayLoadFailureDetails& Details);
    void TilesetFailed(const FCesium3DTilesetLoadFailureDetails& Details);
    TWeakObjectPtr<class ADroneOpsPlayerController> Controller;
    TWeakObjectPtr<class UDroneRegistrySubsystem> Registry;
    TWeakObjectPtr<class USequenceDispatchPanelWidget> PathPanel;
};
