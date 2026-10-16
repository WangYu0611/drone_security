#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Map/PlanRouteVisualSet.h"
#include "Map/FleetSelection.h"
#include "MapFleetInteractionWidget.generated.h"

UENUM()
enum class EMapFleetMode : uint8 { Browse, BoxSelect, GroupMove };

/** Native pointer gestures and previews; selection/execution commit only on Backend ACK. */
UCLASS()
class UE5DRONECONTROL_API UMapFleetInteractionWidget : public UUserWidget {
    GENERATED_BODY()
    friend class FFleetMapWorkflow;
public:
    void SetMode(EMapFleetMode Value);
    EMapFleetMode GetMode() const { return Mode; }
    void Refresh();
    bool BeginPointer(const FVector2D& Screen,bool Shift,bool Control);
    void MovePointer(const FVector2D& Screen,bool OverMap);
    void EndPointer(const FVector2D& Screen,bool OverMap);
    void CancelGesture();
    bool CancelMode();
    bool OwnsPrimaryInput() const { return Mode!=EMapFleetMode::Browse || bDragging; }
    bool IsDragging() const { return bDragging; }
    bool IsOverControls(const FVector2D& Screen) const;
    bool IsAvailable() const;
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;
    virtual int32 NativePaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
private:
    UPROPERTY() TObjectPtr<class UBorder> Panel;
    UPROPERTY() TObjectPtr<class UTextBlock> Summary;
    UPROPERTY() TObjectPtr<class UTextBlock> TargetText;
    UPROPERTY() TObjectPtr<class UTextBlock> Result;
    UPROPERTY() TMap<FName,TObjectPtr<class UCommandActionButton>> Buttons;
    EMapFleetMode Mode=EMapFleetMode::Browse;
    FleetSelection::EOperation SelectionOperation=FleetSelection::EOperation::Replace;
    bool bDragging=false,bPointerValid=false,bPending=false,bHasTarget=false;
    bool bSelectionPending=false;
    TArray<int32> RequestedSelection;
    int32 RequestedPrimary=0;
    FVector2D Start=FVector2D::ZeroVector,Current=FVector2D::ZeroVector;
    FVector TargetGeographic=FVector::ZeroVector; // longitude, latitude, ellipsoid altitude
    TArray<int32> TargetMembers;
    int32 TargetPrimary=0;
    FString ExecutionId,MessageKey;
    FText ErrorDetails;
    FPlanRouteVisualSet PreviewRoutes;
    bool Project(int32 Id,FVector2D& Point) const;
    bool Telemetry(int32 Id,FVector& Geographic) const;
    void SelectInRectangle(bool Click);
    bool PickTarget(const FVector2D& Local);
    void ClearTarget();
    void BuildPreview();
    FText UnavailableReason() const;
    TArray<int32> OrderedSelection() const;
    UFUNCTION() void Action(FName Name,int32 Id);
};
