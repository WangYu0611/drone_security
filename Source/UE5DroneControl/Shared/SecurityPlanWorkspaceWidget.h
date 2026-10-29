#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "Dom/JsonObject.h"
#include "SecurityPlanWorkspaceWidget.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPlanMissionAction,FName,Action,int32,Index);
// ComboBox keeps generated Slate content, not a bare UTextBlock UObject. The
// SObjectWidget wrapper keeps its text/style alive across GC and hidden pages.
UCLASS()
class UE5DRONECONTROL_API UPlanUAVOptionWidget:public UUserWidget {
    GENERATED_BODY()
public:void SetCaption(const FString& Item);
protected:virtual void NativeOnInitialized() override;
private:UPROPERTY() TObjectPtr<class UTextBlock> Caption;
};
UCLASS()
class UE5DRONECONTROL_API UPlanMissionListWidget:public UUserWidget {
    GENERATED_BODY()
public:
    void Refresh(const TSharedPtr<FJsonObject>& State,const TSharedPtr<FJsonObject>& Plan,const FString& SelectedMission=TEXT(""));
    TArray<FString> MissionIds;
    TArray<int32> MissionActionIds;
    bool ResolveAction(int32 Id,FString& OutPlan,FString& OutMission) const;
    FPlanMissionAction OnAction;
protected:virtual void NativeOnInitialized() override;
private:
    UPROPERTY() TObjectPtr<class UVerticalBox> Content;
    FString PlanId,SelectedId,VisibleSignature;
    int32 NextActionId=0;
    TMap<int32,FString> ActionTargets;
    UFUNCTION() void Action(FName Name,int32 Id){OnAction.Broadcast(Name,Id);}
};
UCLASS()
class UE5DRONECONTROL_API UPlanReviewWidget:public UUserWidget {
    GENERATED_BODY()
    friend class FPlanMissionActions;
public:void Refresh(const TSharedPtr<FJsonObject>& State,const TSharedPtr<FJsonObject>& Plan);
protected:virtual void NativeOnInitialized() override;
private:
    UPROPERTY() TObjectPtr<class UTextBlock> Summary;
    UPROPERTY() TObjectPtr<class UTextBlock> Description;
    UPROPERTY() TObjectPtr<class UTextBlock> MissionValue;
    UPROPERTY() TObjectPtr<class UTextBlock> AircraftValue;
    UPROPERTY() TObjectPtr<class UTextBlock> WaypointsValue;
    UPROPERTY() TObjectPtr<class UTextBlock> DistanceValue;
    UPROPERTY() TObjectPtr<class UTextBlock> Assignments;
    UPROPERTY() TObjectPtr<class UTextBlock> Issues;
    UPROPERTY() TObjectPtr<class UTextBlock> Deployment;
    UPROPERTY() TObjectPtr<class UTextBlock> Readiness;
    UPROPERTY() TObjectPtr<class UTextBlock> MapReadiness;
};
UCLASS()
class UE5DRONECONTROL_API USecurityPlanWorkspaceWidget:public UUserWidget {
    GENERATED_BODY()
    friend class FP4CommandReviewCopy;
    friend class FP52ExecutionWidgets;
    friend class FP55ReservationWidgets;
    friend class FPlanConfigurationWorkspace;
    friend class FFigmaCommandChecks;
    friend class FPlanMissionActions;
    friend class FPlanStartReadinessUxTest;
    friend class FPlanScheduleDraftUxTest;
public:
    void Refresh();
    bool IsConfiguring() const;
    bool IsCreatingPlan() const { return bCreating; }
    bool IsMissionNavigationBlocked() const { return bPending || bConfigurationGuard || !PendingDeletePlan.IsEmpty(); }
    bool HasUnsavedChanges() const;
    FText ConfigurationStatusText() const;
    void RequestFinishConfiguration(TFunction<void()> OnFinished = {});
    void FocusConfiguration();
    // Selects the exact acknowledged task and opens its execution details only.
    void OpenMissionExecution(const FString& InPlanId,const FString& InMissionId);
protected:virtual void NativeOnInitialized() override;
private:
    UPROPERTY() TObjectPtr<class UVerticalBox> ListPage;
    UPROPERTY() TObjectPtr<class UVerticalBox> PlanCards;
    UPROPERTY() TObjectPtr<class UVerticalBox> Workspace;
    UPROPERTY() TObjectPtr<class UVerticalBox> BasicPage;
    UPROPERTY() TObjectPtr<class UVerticalBox> BasicPageHost;
    UPROPERTY() TObjectPtr<class UVerticalBox> TaskPage;
    UPROPERTY() TObjectPtr<class UVerticalBox> RoutePage;
    UPROPERTY() TObjectPtr<class UVerticalBox> ReviewPage;
    UPROPERTY() TObjectPtr<class UTextBlock> WorkspaceTitle;
    UPROPERTY() TObjectPtr<class UTextBlock> GeometryStatus;
    UPROPERTY() TObjectPtr<class UTextBlock> Steps;
    UPROPERTY() TObjectPtr<class UHorizontalBox> DetailColumns;
    UPROPERTY() TObjectPtr<class UVerticalBox> DetailSidebar;
    UPROPERTY() TObjectPtr<class UBorder> DeploymentActions;
    UPROPERTY() TObjectPtr<class UTextBlock> PlanStatusCaption;
    UPROPERTY() TArray<TObjectPtr<class UBorder>> StepMarkers;
    UPROPERTY() TArray<TObjectPtr<class UTextBlock>> StepLabels;
    UPROPERTY() TObjectPtr<class UTextBlock> RouteStatus;
    UPROPERTY() TObjectPtr<class UTextBlock> ReviewHeading;
    UPROPERTY() TObjectPtr<class UTextBlock> DeploymentBlockers;
    UPROPERTY() TObjectPtr<class UBorder> NewPlanDialog;
    UPROPERTY() TObjectPtr<class UOverlay> NewPlanLayer;
    UPROPERTY() TObjectPtr<class UOverlay> ConfigurationGuardLayer;
    UPROPERTY() TObjectPtr<class UOverlay> DeletePlanLayer;
    UPROPERTY() TObjectPtr<class UVerticalBox> DeletePlanContent;
    UPROPERTY() TObjectPtr<class UTextBlock> DeletePlanPrompt;
    UPROPERTY() TObjectPtr<class UEditableTextBox> DeletePlanNameInput;
    UPROPERTY() TObjectPtr<class UVerticalBox> NewPlanContent;
    UPROPERTY() TObjectPtr<class UVerticalBox> EmptyPlanState;
    UPROPERTY() TObjectPtr<class UScrollBox> WorkspaceScroll;
    UPROPERTY() TObjectPtr<class UMultiLineEditableTextBox> DescriptionEditor;
    UPROPERTY() TObjectPtr<class UTextBlock> BasicHeading;
    bool bList=true,bCreating=false,bInitializedSelection=false;
    int32 CurrentStep=1;
    FString CardsSignature;
    void RefreshPlanCards(const TSharedPtr<FJsonObject>& State);
    FString LastWorkflow;
    FString PendingDeletePlan;
    FString PendingDeleteName;
    double PendingDeleteRevision=-1;
    TMap<int32,FString> PlanActionTargets;
    TArray<int32> PlanActionIds;
    int32 NextPlanActionId=0;
    void RequestPlanDeletion(const FString& Id);
    void RefreshDeleteConfirmation();
    UFUNCTION() void DeleteConfirmationChanged(const FText& Text);
    void ConfirmDeployment();
    UPROPERTY() TObjectPtr<class UVerticalBox> ExecutionPage;
    UPROPERTY() TObjectPtr<class UTextBlock> ExecutionSummary;
    UPROPERTY() TObjectPtr<class UTextBlock> ExecutionMode;
    UPROPERTY() TObjectPtr<class UTextBlock> ExecutionPrompt;
    UPROPERTY() TObjectPtr<class UTextBlock> ExecutionHistory;
    UPROPERTY() TObjectPtr<class UProgressBar> ExecutionProgress;
    FString ExecutionId,ExecutionIntent,StartRequestId;
    FString ScheduleExecutionId,ScheduleAcceptedAt,ScheduleLoadedText;
    void ResetStartSchedule(const FDateTime& LocalNow);
    void RefreshScheduleInput(const TSharedPtr<FJsonObject>& Execution);
    static TArray<FText> ExecutionStartBlockers(const TSharedPtr<FJsonObject>& State,const TSharedPtr<FJsonObject>& Mission,const TArray<FString>& AvailableUavs);
    bool bExecutionView=false;
    void RefreshExecution(const TSharedPtr<FJsonObject>& State,const TSharedPtr<FJsonObject>& Plan);
    void ExecutionAction(FName Name);
    TSharedPtr<FJsonObject> SelectedExecution(const TSharedPtr<FJsonObject>& State) const;
    void SelectMission(const FString& InPlanId,const FString& InMissionId,TFunction<void()> Continue,bool FinishConfigurationFirst=false);
    void EditMission(const FString& InPlanId,const FString& InMissionId);
    void PrepareMissionExecution(const FString& InPlanId,const FString& InMissionId);


    UPROPERTY() TObjectPtr<class UVerticalBox> Content;
    UPROPERTY() TObjectPtr<UComboBoxString> Plans;
    UPROPERTY() TObjectPtr<UComboBoxString> UAVs;
    UPROPERTY() TObjectPtr<class UVerticalBox> AssignmentList;
    UPROPERTY() TMap<FString,TObjectPtr<class UCheckBox>> AssignmentChecks;
    UPROPERTY() TObjectPtr<class UCheckBox> ScheduleEnabled;
    UPROPERTY() TObjectPtr<class UEditableTextBox> ScheduleInput;
    UPROPERTY() TObjectPtr<class UEditableTextBox> SpacingInput;
    FString AssignmentSelection;

    UPROPERTY() TObjectPtr<class UEditableTextBox> PlanName;
    UPROPERTY() TObjectPtr<class UEditableTextBox> Description;
    UPROPERTY() TObjectPtr<class UEditableTextBox> MissionName;
    UPROPERTY() TObjectPtr<class UTextBlock> Summary;
    UPROPERTY() TObjectPtr<class UTextBlock> Result;
    UPROPERTY() TObjectPtr<UPlanMissionListWidget> MissionList;
    UPROPERTY() TObjectPtr<UPlanReviewWidget> Review;
    UPROPERTY() TMap<FName,TObjectPtr<class UCommandActionButton>> Actions;
    TArray<FString> PlanIds,PlanLabels,UavIds;
    FString PlanId,MissionId,LoadedSelection,ErrorCode;
    FText ExecutionErrorDetails;
    bool bRefreshing=false,bPending=false,bReview=false,bConfirm=false;
    double ConfirmRevision=-1;
    UFUNCTION() void PlanSelected(FString Item,ESelectInfo::Type Type);
    UFUNCTION() void UAVSelected(FString Item,ESelectInfo::Type Type);
    UFUNCTION() UWidget* UAVOption(FString Item);
    UFUNCTION() void Action(FName Name,int32 Id);
    UFUNCTION() void MissionAction(FName Name,int32 Index);
    void Submit(const TSharedRef<FJsonObject>& Body,TFunction<void()> OnSuccess = {});
    bool CanEditConfiguration() const;
    void BeginConfiguration(const FString& Id, TFunction<void()> Continue);
    void GuardConfigurationChange(TFunction<void()> Continue);
    void SaveConfiguration(TFunction<void()> Continue = {});
    void SaveNextConfigurationChange();
    void FinishConfiguration();
    void ResetConfigurationInputs();
    void RefreshConfiguration();
    void BuildPlanPresentation();
    void RefreshPlanPresentation();
    UFUNCTION() void PlanNameChanged(const FText& Text);
    UFUNCTION() void DescriptionChanged(const FText& Text);
    UPROPERTY() TObjectPtr<class UVerticalBox> ConfigurationGuard;
    UPROPERTY() TObjectPtr<class UTextBlock> ConfigurationStatus;
    bool bConfigurationGuard=false;
    TFunction<void()> GuardedConfigurationChange;
    TFunction<void()> AfterConfigurationSave;
    TFunction<void()> AfterConfigurationFinish;
    TArray<TSharedRef<FJsonObject>> ConfigurationSaveQueue;
};
