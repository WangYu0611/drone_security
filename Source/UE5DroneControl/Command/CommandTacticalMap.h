#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CommandTacticalMap.generated.h"

/** Independent 2D raster view over the existing Registry. No scene or camera replication. */
UCLASS()
class UE5DRONECONTROL_API UCommandTacticalMap : public UUserWidget
{
    GENERATED_BODY()
    friend class FMapPointMoveChecks;
public:
    void Refresh();
    void ToggleFollow(){bFollow=!bFollow;}
    bool IsFollowing() const { return bFollow; }
    int32 GetZoom() const { return Zoom; }
    void SetZoom(int32 Value) { Zoom=FMath::Clamp(Value,3,19);Refresh(); }
    void SetFigmaPresentation(bool Enabled) { bFigmaPresentation=Enabled; }
    bool SelectUAV(int32 Id);
    bool HasConfiguredBasemap() const { return !Template.IsEmpty(); }
    static FVector2D Project(double Latitude, double Longitude, int32 Zoom);
    static FVector2D Unproject(const FVector2D& Pixel, int32 Zoom);
protected:
    virtual void NativeOnInitialized() override;
    virtual int32 NativePaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
    virtual FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
private:
    UPROPERTY() TObjectPtr<class UCanvasPanel> Canvas;
    UPROPERTY() TObjectPtr<class UWidget> MoveControls;
    UPROPERTY() TMap<FString,TObjectPtr<class UTexture2D>> Tiles;
    TSet<FString> Pending;
    TMap<FString,double> RetryAfter;
    FVector2D Center = FVector2D(0.5,0.5);
    bool bCentered = false, bDragging = false, bFollow=false;
    bool bFigmaPresentation=false;
    int32 Zoom = 15;
    FString Template, Attribution, Error;
    FVector2D PointerStart=FVector2D::ZeroVector,PointerLast=FVector2D::ZeroVector;
    bool bPanning=false,bHasMoveTarget=false,bMovePending=false,bSelectionPending=false;
    FKey PointerButton;
    double SelectionRequestedAt=0;
    TArray<int32> RequestedSelection,MoveMembers;
    int32 RequestedPrimary=0;
    FVector MoveTarget=FVector::ZeroVector;
    FText MoveMessage;
    FString MoveErrorCode;
    bool PickMoveTarget(const FVector2D& Local);
    FText MoveUnavailable() const;
    void AddMoveControls(const FVector2D& Size);
    void ClearMoveTarget();
    void LoadTile(const FString& Key, int32 X, int32 Y);
    UFUNCTION() void MarkerClicked(FName Action, int32 Id);
    UFUNCTION() void MoveAction(FName Action, int32 Id);
};
