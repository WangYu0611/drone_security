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
    void Refresh(const TSharedPtr<FJsonObject>& State,const TSharedPtr<FJsonObject>& Plan);
    TArray<FString> MissionIds;
    FPlanMissionAction OnAction;
protected:virtual void NativeOnInitialized() override;
private:
    UPROPERTY() TObjectPtr<class UVerticalBox> Content;
    int64 Version=-1;FString PlanId;
    UFUNCTION() void Action(FName Name,int32 Id){OnAction.Broadcast(Name,Id);}
};
UCLASS()
class UE5DRONECONTROL_API UPlanReviewWidget:public UUserWidget {
    GENERATED_BODY()
public:void Refresh(const TSharedPtr<FJsonObject>& State,const TSharedPtr<FJsonObject>& Plan);
protected:virtual void NativeOnInitialized() override;
private:UPROPERTY() TObjectPtr<class UTextBlock> Summary;
};
UCLASS()
class UE5DRONECONTROL_API USecurityPlanWorkspaceWidget:public UUserWidget {
    GENERATED_BODY()
    friend class FP4CommandReviewCopy;
    friend class FP52ExecutionWidgets;
public:void Refresh();
protected:virtual void NativeOnInitialized() override;
private:
    UPROPERTY() TObjectPtr<class UVerticalBox> ListPage;
    UPROPERTY() TObjectPtr<class UVerticalBox> PlanCards;
    UPROPERTY() TObjectPtr<class UVerticalBox> Workspace;
    UPROPERTY() TObjectPtr<class UVerticalBox> BasicPage;
    UPROPERTY() TObjectPtr<class UVerticalBox> TaskPage;
    UPROPERTY() TObjectPtr<class UVerticalBox> RoutePage;
    UPROPERTY() TObjectPtr<class UVerticalBox> ReviewPage;
    UPROPERTY() TObjectPtr<class UTextBlock> WorkspaceTitle;
    UPROPERTY() TObjectPtr<class UTextBlock> Steps;
    UPROPERTY() TObjectPtr<class UTextBlock> RouteStatus;
    UPROPERTY() TObjectPtr<class UTextBlock> ReviewHeading;
    UPROPERTY() TObjectPtr<class UBorder> NewPlanDialog;
    UPROPERTY() TObjectPtr<class UTextBlock> BasicHeading;
    bool bList=true,bCreating=false,bInitializedSelection=false;
    int32 CurrentStep=1;
    int64 CardsVersion=-1;
    FString LastWorkflow;
    FString PendingDeletePlan;
    void ConfirmDeployment();
    UPROPERTY() TObjectPtr<class UVerticalBox> ExecutionPage;
    UPROPERTY() TObjectPtr<class UTextBlock> ExecutionSummary;
    UPROPERTY() TObjectPtr<class UTextBlock> ExecutionPrompt;
    UPROPERTY() TObjectPtr<class UTextBlock> ExecutionHistory;
    UPROPERTY() TObjectPtr<class UProgressBar> ExecutionProgress;
    FString ExecutionId,ExecutionIntent,StartRequestId;
    bool bExecutionView=false;
    void RefreshExecution(const TSharedPtr<FJsonObject>& State,const TSharedPtr<FJsonObject>& Plan);
    void ExecutionAction(FName Name);


    UPROPERTY() TObjectPtr<class UVerticalBox> Content;
    UPROPERTY() TObjectPtr<UComboBoxString> Plans;
    UPROPERTY() TObjectPtr<UComboBoxString> UAVs;
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
    bool bRefreshing=false,bPending=false,bReview=false,bConfirm=false;
    double ConfirmRevision=-1;
    UFUNCTION() void PlanSelected(FString Item,ESelectInfo::Type Type);
    UFUNCTION() void UAVSelected(FString Item,ESelectInfo::Type Type);
    UFUNCTION() UWidget* UAVOption(FString Item);
    UFUNCTION() void Action(FName Name,int32 Id);
    UFUNCTION() void MissionAction(FName Name,int32 Index);
    void Submit(const TSharedRef<FJsonObject>& Body);
};
