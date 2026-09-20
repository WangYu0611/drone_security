#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MapShellWidget.generated.h"

/** Map-only presentation. All planning and playback actions use existing adapters. */
UCLASS()
class UE5DRONECONTROL_API UMapShellWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void InitializeManager(class UCommandScreenManager* InManager);
    void Refresh();
    void AttachCenterWidget(UUserWidget* Widget);
    bool IsCursorOverMap() const;
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
private:
    TWeakObjectPtr<class UCommandScreenManager> Manager;
    UPROPERTY() TObjectPtr<class UOverlay> CenterHost;
    UPROPERTY() TObjectPtr<class UBorder> Toolbar;
    UPROPERTY() TObjectPtr<class UBorder> SelectionHighlight;
    UPROPERTY() TObjectPtr<class UBorder> MissionHighlight;
    UPROPERTY() TObjectPtr<class UTextBlock> Status;
    UPROPERTY() TObjectPtr<class UTextBlock> ActionStatus;
    UPROPERTY() TObjectPtr<class UMapMissionRouteWidget> PlanPanel;
    UPROPERTY() TObjectPtr<class UComboBoxString> Aircraft;
    UPROPERTY() TObjectPtr<class UCommandActionButton> Map2DButton;
    UPROPERTY() TObjectPtr<class UCommandActionButton> Map3DButton;
    TMap<FString, int32> AircraftIds;
    TArray<FString> AircraftOptions;
    UPROPERTY() TObjectPtr<class UMapExecutionWidget> ExecutionMonitor;
    bool bRefreshing = false;
    UFUNCTION() void ActionRequested(FName Action, int32 ItemId);
    UFUNCTION() void AircraftSelected(FString Item, ESelectInfo::Type Type);
};
