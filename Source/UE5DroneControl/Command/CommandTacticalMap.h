#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CommandTacticalMap.generated.h"

/** Independent 2D raster view over the existing Registry. No scene or camera replication. */
UCLASS()
class UE5DRONECONTROL_API UCommandTacticalMap : public UUserWidget
{
    GENERATED_BODY()
public:
    void Refresh();
    bool SelectUAV(int32 Id);
    bool HasConfiguredBasemap() const { return !Template.IsEmpty(); }
    static FVector2D Project(double Latitude, double Longitude, int32 Zoom);
protected:
    virtual void NativeOnInitialized() override;
    virtual int32 NativePaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
    virtual FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
private:
    UPROPERTY() TObjectPtr<class UCanvasPanel> Canvas;
    UPROPERTY() TMap<FString,TObjectPtr<class UTexture2D>> Tiles;
    TSet<FString> Pending;
    TMap<FString,double> RetryAfter;
    FVector2D Center = FVector2D(0.5,0.5);
    bool bCentered = false, bDragging = false;
    int32 Zoom = 15;
    FString Template, Attribution, Error;
    void LoadTile(const FString& Key, int32 X, int32 Y);
    UFUNCTION() void MarkerClicked(FName Action, int32 Id);
};
