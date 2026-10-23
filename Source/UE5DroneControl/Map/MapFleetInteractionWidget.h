#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Map/PlanRouteVisualSet.h"
#include "Map/FleetSelection.h"
#include "Map/MapDronePresentation.h"
#include "Map/MapMoveIntent.h"
#include "MapFleetInteractionWidget.generated.h"

UENUM()
enum class EMapFleetMode : uint8 { Browse, BoxSelect, GroupMove };

/** Native pointer gestures and previews; selection/execution commit only on Backend ACK. */
UCLASS()
class UE5DRONECONTROL_API UMapFleetInteractionWidget : public UUserWidget {
    GENERATED_BODY()
    friend class FFleetMapWorkflow;
    friend class FFleetTerrainWorkflow;
    friend class FMapStartupVisibility;
    friend class FFleetPanelVisibility;
    friend class FMapFleetCardLayout;
public:
    void SetMode(EMapFleetMode Value);
    EMapFleetMode GetMode() const { return Mode; }
    void Refresh();
    bool BeginPointer(const FVector2D& Screen,bool Shift,bool Control);
    void MovePointer(const FVector2D& Screen,bool OverMap);
    void EndPointer(const FVector2D& Screen,bool OverMap);
    bool PickDestinationAtScreen(const FVector2D& Screen);
    void CancelGesture();
    bool CancelMode();
    bool CanShowExecutionControls() const;
    bool ShowExecutionControls();
    bool OwnsPrimaryInput() const { return Mode!=EMapFleetMode::Browse || bDragging; }
    bool IsDragging() const { return bDragging; }
    bool IsSelectionPending() const { return bSelectionPending; }
    bool IsOverControls(const FVector2D& Screen) const;
    bool IsAvailable() const;
    FString GetManagedGroup() const { return ManagedGroup; }
    TArray<int32> GetPreviewMembers() const { return bHasTarget?TargetMembers:TArray<int32>(); }
    // Presentation intent, not proof of rendering. Native QA also checks the
    // returned actor's active meshes are visible and recently rendered.
    AActor* GetDisplayedModelActor(int32 Id) const;
    bool IsModelPresentation(int32 Id) const { return GetDisplayedModelActor(Id)!=nullptr; }
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry&,float) override;
    virtual void NativeDestruct() override;
    virtual int32 NativePaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
private:
    mutable TArray<int32> PaintedDroneIds; // Only icons actually submitted by NativePaint.
    UPROPERTY() TObjectPtr<class UImage> MarkerIcon;
    UPROPERTY() TObjectPtr<class UImage> SelectionMarkerIcon;
    UPROPERTY() TObjectPtr<class UBorder> Panel;
    UPROPERTY() TObjectPtr<class UTextBlock> PanelTitle;
    UPROPERTY() TObjectPtr<class UTextBlock> Summary;
    UPROPERTY() TObjectPtr<class UTextBlock> TargetText;
    UPROPERTY() TObjectPtr<class UTextBlock> Result;
    UPROPERTY() TObjectPtr<class UTextBlock> SelectionText;
    UPROPERTY() TObjectPtr<class UTextBlock> TakeoverWarning;
    UPROPERTY() TObjectPtr<class UTextBlock> HeightDeltaText;
    UPROPERTY() TObjectPtr<class UTextBlock> HeightLabel;
    UPROPERTY() TObjectPtr<class UTextBlock> TerrainInfo;
    UPROPERTY() TObjectPtr<class UTextBlock> TravelText;
    UPROPERTY() TObjectPtr<class UTextBlock> StateText;
    UPROPERTY() TObjectPtr<class UBorder> TargetCard;
    UPROPERTY() TObjectPtr<class UVerticalBox> ProgressBox;
    UPROPERTY() TObjectPtr<class UVerticalBox> AbortPrompt;
    UPROPERTY() TObjectPtr<class UProgressBar> Progress;
    UPROPERTY() TObjectPtr<class USlider> HeightSlider;
    UPROPERTY() TArray<TObjectPtr<class UBorder>> Steps;
    UPROPERTY() TArray<TObjectPtr<class UTextBlock>> StepLabels;
    UPROPERTY() TObjectPtr<class UVerticalBox> HeightControls;
    UPROPERTY() TObjectPtr<class UEditableTextBox> TargetHeightInput;
    UPROPERTY() TMap<FName,TObjectPtr<class UCommandActionButton>> Buttons;
    EMapFleetMode Mode=EMapFleetMode::Browse;
    FleetSelection::EOperation SelectionOperation=FleetSelection::EOperation::Replace;
    bool bDragging=false,bPointerValid=false,bPointerMoved=false,bPending=false,bHasTarget=false;
    bool bSelectionPending=false,bSelectionVideoPending=false;
    // The card follows explicit move intent, independently of selection mode,
    // telemetry warnings or the currently selected execution's lifetime.
    bool bPanelRequested=false;
    double SelectionRequestedAt=0;
    bool bUpdatingHeight=false,bHadMapMode=false,bWas3D=false;
    bool bAbortConfirm=false,bShowCoordinates=false;
    bool bHeightAGL=true,bRecoveryPreview=false;
    int32 HeightPrimary=0;
    TArray<int32> RequestedSelection;
    int32 RequestedPrimary=0;
    FVector2D Start=FVector2D::ZeroVector,Current=FVector2D::ZeroVector;
    FVector2D PointerStartScreen=FVector2D::ZeroVector;
    float PointerDragThreshold=5.f;
    FVector TargetGeographic=FVector::ZeroVector; // longitude, latitude, ellipsoid altitude
    TArray<int32> TargetMembers;
    int32 TargetPrimary=0;
    MapMove::FTakeoverIntent TargetTakeover;
    FVector TakeoverPreviewOrigin=FVector::ZeroVector;
    double NextTakeoverPreviewUpdate=0;
    FString ExecutionId,MessageKey,ManagedGroup,AbortGroup;
    TMap<TWeakObjectPtr<class UWidgetComponent>,bool> SuppressedLabels;
    MapDronePresentation::FVisibilityOverride ModelVisibility;
    TMap<int32,TWeakObjectPtr<AActor>> DisplayedModelActors;
    TMap<int32,FBox2D> ModelScreenBounds;
    FText ErrorDetails;
    mutable FText CachedTerrainError;
    mutable double NextTerrainCheck=0;
    FPlanRouteVisualSet PreviewRoutes;
    bool Project(int32 Id,FVector2D& Point) const;
    bool Telemetry(int32 Id,FVector& Geographic) const;
    bool Is3DView() const;
    bool TargetHeight(double& Height) const;
    bool SurfaceHeight(const FVector& Geographic,double& Height) const;
    bool RecoveryHeight(double& Height) const;
    FText TerrainError(bool Force=false) const;
    double InputHeight(double EllipsoidHeight,const FVector& Geographic) const;
    void ResetHeight();
    void SelectInRectangle(bool Click);
    bool PickTarget(const FVector2D& Local);
    void ClearTarget();
    void BuildPreview();
    FText UnavailableReason() const;
    TArray<int32> OrderedSelection() const;
    UFUNCTION() void Action(FName Name,int32 Id);
    UFUNCTION() void TargetHeightChanged(const FText& Text);
    UFUNCTION() void HeightSliderChanged(float Value);
};
