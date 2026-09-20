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
    UPROPERTY() TObjectPtr<class UTextBlock> CurrentExecutionSummary;
    bool bRestoredWorkspace=false;
    UPROPERTY() TObjectPtr<class UWidgetSwitcher> WorkspacePages;
    UPROPERTY() TObjectPtr<class UTextBlock> CurrentPlanSummary;
    UPROPERTY() TObjectPtr<class UTextBlock> RecentEvents;
    UPROPERTY() TArray<TObjectPtr<class UCommandActionButton>> PrimaryNavigationButtons;
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
