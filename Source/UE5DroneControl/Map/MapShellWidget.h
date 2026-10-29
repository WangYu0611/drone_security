#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Map/PlanRouteVisualSet.h"
#include "MapShellWidget.generated.h"

/** Map-only presentation. All planning and playback actions use existing adapters. */
UCLASS()
class UE5DRONECONTROL_API UMapShellWidget : public UUserWidget
{
    GENERATED_BODY()
    friend class FP55SecurityPlanVisual;
    friend class FFigmaMapChecks;
public:
    void InitializeManager(class UCommandScreenManager* InManager);
    void Refresh();
    void AttachCenterWidget(UUserWidget* Widget);
    bool IsCursorOverMap() const;
    bool IsPointOverMap(const FVector2D& Screen) const;
    FGeometry GetMapViewportGeometry() const;
    TArray<FBox2D> GetMapControlBounds() const;
    bool IsDeviceSelectionPending() const {return bDeviceSelectionPending;}
    class UMapPlanMoveWidget* GetMovePanel() const { return MovePanel; }
    class UMapFleetInteractionWidget* GetFleetInteraction() const { return FleetInteraction; }
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
    virtual int32 NativePaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
private:
    UPROPERTY() TObjectPtr<class UCanvasPanel> FigmaChrome;
    UPROPERTY() TArray<TObjectPtr<class UBorder>> BackgroundMasks;
    UPROPERTY() TObjectPtr<class UBorder> MapTint;
    UPROPERTY() TObjectPtr<class UBorder> MapFrame;
    UPROPERTY() TObjectPtr<class UBorder> DroneDropdown;
    UPROPERTY() TObjectPtr<class UVerticalBox> DroneOptions;
    UPROPERTY() TObjectPtr<class UCommandActionButton> DeviceButton;
    UPROPERTY() TObjectPtr<class UCommandActionButton> RouteButton;
    UPROPERTY() TObjectPtr<class UImage> RouteButtonIcon;
    UPROPERTY() TObjectPtr<class UTextBlock> DeviceCaption;
    UPROPERTY() TObjectPtr<class UBorder> CameraHint;
    UPROPERTY() TObjectPtr<class UVerticalBox> ZoomControls;
    bool bDropdownOpen=false,bDeviceSelectionPending=false,bRouteIconActive=false;
    int32 PendingDevice=0;
    double DeviceSelectionRequestedAt=0;
    FString MenuSignature;
    FText DeviceSelectionError;
    void RefreshDeviceMenu();
    TWeakObjectPtr<class UCommandScreenManager> Manager;
    UPROPERTY() TObjectPtr<class UOverlay> CenterHost;
    UPROPERTY() TObjectPtr<class UBorder> Toolbar;
    UPROPERTY() TObjectPtr<class UBorder> SelectionHighlight;
    UPROPERTY() TObjectPtr<class UBorder> MissionHighlight;
    UPROPERTY() TObjectPtr<class UTextBlock> Status;
    UPROPERTY() TObjectPtr<class UTextBlock> ActionStatus;
    UPROPERTY() TObjectPtr<class UBorder> RouteLegend;
    UPROPERTY() TObjectPtr<class UTextBlock> RouteLegendText;
    UPROPERTY() TObjectPtr<class UMapMissionRouteWidget> PlanPanel;
    UPROPERTY() TObjectPtr<class UComboBoxString> Aircraft;
    UPROPERTY() TObjectPtr<class UCommandActionButton> Map2DButton;
    UPROPERTY() TObjectPtr<class UCommandActionButton> Map3DButton;
    UPROPERTY() TObjectPtr<class UCommandActionButton> BoxSelectButton;
    UPROPERTY() TObjectPtr<class UCommandActionButton> GroupMoveButton;
    UPROPERTY() TObjectPtr<class UCommandActionButton> TakeoverButton;
    UPROPERTY() TObjectPtr<class UCommandActionButton> ExecutionControlsButton;
    UPROPERTY() TObjectPtr<class UMapFleetInteractionWidget> FleetInteraction;
    TMap<FString, int32> AircraftIds;
    TArray<FString> AircraftOptions;
    UPROPERTY() TObjectPtr<class UMapExecutionWidget> ExecutionMonitor;
    UPROPERTY() TObjectPtr<class UMapPlanMoveWidget> MovePanel;
    bool bWasMoving=false;
    FPlanRouteVisualSet SavedRouteVisuals;
    void RefreshSavedRoutes(bool Moving);
    bool bRefreshing = false;
    UFUNCTION() void ActionRequested(FName Action, int32 ItemId);
    UFUNCTION() void AircraftSelected(FString Item, ESelectInfo::Type Type);
};
