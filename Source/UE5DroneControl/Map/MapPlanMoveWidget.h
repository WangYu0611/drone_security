#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Dom/JsonObject.h"
#include "MapPlanMoveWidget.generated.h"

UCLASS()
class UE5DRONECONTROL_API UMapPlanMoveWidget : public UUserWidget {
    GENERATED_BODY()
    friend class FP53GeometryUI;
public:
    bool IsMoving() const { return !SessionId.IsEmpty(); }
    void Refresh();
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry&,float) override;
    virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseMove(const FGeometry&,const FPointerEvent&) override;
    virtual int32 NativePaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
private:
    struct FRoutePreview { FString Id; TSharedPtr<FJsonObject> Saved; TArray<FVector> Original; bool Closed=false; };
    TArray<FRoutePreview> Routes;
    FString PlanId,MissionId,SessionId,PlanName,ErrorCode;
    FVector Anchor=FVector::ZeroVector,Delta=FVector::ZeroVector,DragStart=FVector::ZeroVector,DragDelta=FVector::ZeroVector;
    bool bPending=false,bDragging=false;
    UPROPERTY() TObjectPtr<class UTextBlock> Summary;
    UPROPERTY() TObjectPtr<class UButton> Handle;
    UPROPERTY() TObjectPtr<class UCommandActionButton> Confirm;
    UPROPERTY() TObjectPtr<class UCommandActionButton> Cancel;
    void Requested(const TSharedPtr<FJsonObject>&);
    UFUNCTION() void Action(FName Name,int32 Id);
    bool CursorOnPlane(const FVector2D& Absolute,FVector&) const;
    bool Project(const FVector&,FVector2D&) const;
    void Leave();
};
