#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Command/CommandMapInteractionService.h"
#include "CommandShellWidget.generated.h"

/** Native shell, optionally subclassable in WBP_CommandShell. No external BP asset required. */
UCLASS()
class UE5DRONECONTROL_API UCommandShellWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void InitializeManager(class UCommandScreenManager* InManager);
    void Refresh();
    void AttachCenterWidget(UUserWidget* Widget);
    void ShowDroneList(bool bVisible);
    bool IsCursorOverMap() const;
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
private:
    friend class FFigmaCommandPresentation;
    friend class FFigmaCommandChecks;
    friend class FOverviewTasksChecks;
    void BuildFigmaShell(class UCanvasPanel* Root);
    void RefreshFigmaShell();
    void RefreshFigmaDialog();
    void RefreshRunningTasks();
    UFUNCTION() void OpenRunningTask(FName Action,int32 Index);
    UPROPERTY() TObjectPtr<class USizeBox> OverviewSummarySize;
    UPROPERTY() TObjectPtr<class UVerticalBox> RunningTaskRows;
    UPROPERTY() TObjectPtr<class UTextBlock> RunningTaskTitle;
    UPROPERTY() TObjectPtr<class UTextBlock> RunningTaskEmpty;
    UPROPERTY() TArray<TObjectPtr<class UTextBlock>> RunningTaskNames;
    UPROPERTY() TArray<TObjectPtr<class UTextBlock>> RunningTaskDetails;
    UPROPERTY() TArray<TObjectPtr<class UCommandActionButton>> RunningTaskButtons;
    TArray<FString> RunningGroupIds;
    TMap<int32,FString> RunningTaskTargets;
    int32 NextRunningTaskTarget=0;
    FString RunningTaskLanguage;
    UFUNCTION() void FigmaAction(FName Action, int32 Index);
    UPROPERTY() TObjectPtr<class UFigmaFleetWidget> FigmaFleet;
    UPROPERTY() TObjectPtr<class UTextBlock> HeaderClock;
    UPROPERTY() TArray<TObjectPtr<class UTextBlock>> HealthLabels;
    UPROPERTY() TArray<TObjectPtr<class UImage>> HealthDots;
    UPROPERTY() TObjectPtr<class UImage> HealthOnlineIcon;
    UPROPERTY() TObjectPtr<class UImage> HealthOfflineIcon;
    UPROPERTY() TArray<TObjectPtr<class UTextBlock>> StatisticValues;
    UPROPERTY() TArray<TObjectPtr<class UCommandActionButton>> LanguageButtons;
    UPROPERTY() TObjectPtr<class UVerticalBox> EventRows;
    UPROPERTY() TObjectPtr<class UTextBlock> ZoomCaption;
    UPROPERTY() TObjectPtr<class UCommandActionButton> FollowButton;
    UPROPERTY() TObjectPtr<class UBorder> FigmaDialog;
    UPROPERTY() TObjectPtr<class UTextBlock> DialogTitle;
    UPROPERTY() TObjectPtr<class UTextBlock> DialogCount;
    UPROPERTY() TObjectPtr<class UTextBlock> DialogFeedback;
    UPROPERTY() TObjectPtr<class UVerticalBox> DialogRows;
    UPROPERTY() TObjectPtr<class UVerticalBox> DialogColumns;
    UPROPERTY() TObjectPtr<class UScrollBox> DialogScroll;
    FString DialogKind, DialogSignature, EventSignature, PendingAlertId, PendingAlertUAV;
    double AlertSelectionAt=0;
    UPROPERTY() TObjectPtr<class UCommandTacticalMap> SituationMap;
    UPROPERTY() TObjectPtr<class UGroupControlWidget> GroupControl;
    UPROPERTY() TObjectPtr<class UTextBlock> CurrentExecutionSummary;
    bool bRestoredWorkspace=false;
    UPROPERTY() TObjectPtr<class UWidgetSwitcher> WorkspacePages;
    UPROPERTY() TObjectPtr<class UTextBlock> CurrentPlanSummary;
    UPROPERTY() TObjectPtr<class UTextBlock> RecentEvents;
    UPROPERTY() TArray<TObjectPtr<class UCommandActionButton>> PrimaryNavigationButtons;
    UPROPERTY() TObjectPtr<class UVerticalBox> ConfigurationHeader;
    UPROPERTY() TObjectPtr<class UTextBlock> ConfigurationCaption;
    UPROPERTY() TObjectPtr<class UCommandActionButton> ConfigurationFinish;
    UPROPERTY() TObjectPtr<class UActiveExecutionControlsWidget> ActiveControls;
    UFUNCTION() void Navigate(FName Action,int32 Index);
    UPROPERTY() TObjectPtr<class UCommandCenterPanel> CenterPanel;
    TWeakObjectPtr<class UCommandScreenManager> Manager;
    UPROPERTY() TObjectPtr<class UDroneListWidget> DroneList;
    UPROPERTY() TObjectPtr<class UCommandAlarmPanel> AlarmPanel;
    UPROPERTY() TObjectPtr<class UTextBlock> Overview;
    UPROPERTY() TObjectPtr<class UTextBlock> Tasks;
    UPROPERTY() TObjectPtr<class UTextBlock> SelectedInfo;
    UPROPERTY() TObjectPtr<class UTextBlock> SelectedMetrics;
    UPROPERTY() TObjectPtr<class UTextBlock> SelectedMission;
    UPROPERTY() TObjectPtr<class UTextBlock> ActionStatus;
    UPROPERTY() TObjectPtr<class UOverlay> CenterHost;
    UPROPERTY() TObjectPtr<class UBorder> MissionPanel;
    UPROPERTY() TObjectPtr<class UBorder> Header;
    UPROPERTY() TObjectPtr<class UBorder> MapToolbar;
    UPROPERTY() TObjectPtr<class UVerticalBox> MissionActions;
    UPROPERTY() TObjectPtr<class UCommandActionButton> Map2DButton;
    UPROPERTY() TObjectPtr<class UCommandActionButton> Map3DButton;
    UFUNCTION() void MapModeChanged(ECommandMapMode Mode);
    UPROPERTY() TArray<TObjectPtr<class UCommandActionButton>> ActionButtons;
    UFUNCTION() void ActionRequested(FName Action, int32 ItemId);
};
