#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Dom/JsonObject.h"
#include "Map/PlanRouteVisualSet.h"
#include "MapExecutionWidget.generated.h"
UCLASS()
class UE5DRONECONTROL_API UMapExecutionWidget:public UUserWidget {
    GENERATED_BODY()
    friend class FP52ExecutionWidgets;
public:void Refresh();
    bool IsOverControls(const FVector2D& Screen) const;
    void SetManagedFleetGroup(const FString& Group,const TArray<int32>& PreviewMembers);
    void SetRouteSuppression(const FString& Plan,const FString& Mission,bool Moving) { SuppressedPlan=Plan;SuppressedMission=Mission;bPlanMoving=Moving; }
    bool OwnsMission(const FString& Plan,const FString& Mission) const;
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry&,float DeltaSeconds) override;
    virtual int32 NativePaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
private:
    struct FPresentedRoute {
        FString Key;
        TSharedPtr<FJsonObject> Route;
        TArray<TSharedPtr<FJsonObject>> Members;
        bool bMissionGroup=false;
    };
    struct FReplaySample {
        FVector Geographic=FVector::ZeroVector;
        double Seconds=0;
        double Progress=0;
        int32 CurrentWaypoint=0;
        int32 CompletedWaypoints=0;
    };
    UPROPERTY() TObjectPtr<class UBorder> SummaryPanel;
    UPROPERTY() TObjectPtr<class UTextBlock> Summary;
    UPROPERTY() TObjectPtr<class UComboBoxString> RunPicker;
    UPROPERTY() TObjectPtr<class UVerticalBox> ReplayControls;
    UPROPERTY() TObjectPtr<class USlider> ReplaySlider;
    UPROPERTY() TObjectPtr<class UButton> PlayButton;
    UPROPERTY() TObjectPtr<class UTextBlock> PlayCaption;
    UPROPERTY() TObjectPtr<class UTextBlock> ReplayTime;
    TArray<FString> RunOptions;
    TMap<FString,FString> HistoryIds;
    TMap<FString,TArray<FReplaySample>> ReplayTracks;
    FString ReplayId;
    double ReplaySeconds=0;
    double ReplayDuration=0;
    bool bReplayPlaying=false;
    bool bChangingReplayUi=false;
    bool bReplaySelectionChanged=false;
    UFUNCTION() void SelectRun(FString Option,ESelectInfo::Type Type);
    UFUNCTION() void PlayReplay();
    UFUNCTION() void ScrubReplay(float Position);
    UFUNCTION() void LocateExecution();
    void RefreshHistory(const TSharedPtr<FJsonObject>& State);
    void LoadReplay(const TSharedPtr<FJsonObject>& State,const TSharedPtr<FJsonObject>& Run);
    void RefreshSummary();
    bool ReplaySample(const TSharedPtr<FJsonObject>& Current,FReplaySample& Result) const;
    bool FocusExecution();
    TSharedPtr<FJsonObject> Execution;
    TArray<TSharedPtr<FJsonObject>> RenderedExecutions;
    TArray<FPresentedRoute> PresentedRoutes;
    FString FocusedExecution;
    FString SuppressedPlan,SuppressedMission;
    FString ManagedFleetGroup;
    TSet<FString> FleetPreviewUavs;
    bool bPlanMoving=false;
    bool bInitialExecutionSnapshotObserved=false;
    bool IsRouteSuppressed(const TSharedPtr<FJsonObject>& E) const;
    bool IsPresentationSuppressed(const FPresentedRoute& Presentation) const;
    void BuildPresentedRoutes(const TSharedPtr<FJsonObject>& State);
    FPlanRouteVisualSet VisualRoutes;
};
