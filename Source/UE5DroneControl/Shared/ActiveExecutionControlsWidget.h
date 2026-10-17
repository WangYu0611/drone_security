#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Dom/JsonObject.h"
#include "ActiveExecutionControlsWidget.generated.h"

namespace ExecutionControls {
struct FGroup {
    FString Id,PlanName,MissionName,State;
    TArray<FString> Members;
    TArray<TSharedPtr<FJsonObject>> Executions;
    double CreatedAt=-1,Progress=0;
    int32 ActiveCount=0;
    bool CanPause=false,CanResume=false,CanAbort=false;
};
TArray<FGroup> Groups(const TSharedPtr<FJsonObject>& State,bool ActiveOnly=true);
FGroup Find(const TSharedPtr<FJsonObject>& State,const FString& Id);
TSharedPtr<FJsonObject> Representative(const FGroup& Group,FName Action=NAME_None);
}

/** Explicit live execution targets; independent of global selection and configuration. */
UCLASS()
class UE5DRONECONTROL_API UActiveExecutionControlsWidget : public UUserWidget {
    GENERATED_BODY()
public:
    void Refresh();
    int32 GetGroupCount() const { return GroupIds.Num(); }
    TArray<FString> GetVisibleGroupIds() const { return GroupIds; }
    bool IsRequestPending() const { return !PendingGroups.IsEmpty(); }
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY() TObjectPtr<class UVerticalBox> Rows;
    UPROPERTY() TObjectPtr<class UTextBlock> Result;
    UPROPERTY() TArray<TObjectPtr<class UTextBlock>> Labels;
    UPROPERTY() TArray<TObjectPtr<class UCommandActionButton>> PauseButtons;
    UPROPERTY() TArray<TObjectPtr<class UCommandActionButton>> ResumeButtons;
    UPROPERTY() TArray<TObjectPtr<class UCommandActionButton>> AbortButtons;
    UPROPERTY() TArray<TObjectPtr<class UCommandActionButton>> ConfirmButtons;
    UPROPERTY() TArray<TObjectPtr<class UCommandActionButton>> CancelButtons;
    TArray<FString> GroupIds;
    TMap<int32,FString> ActionTargets;
    int32 NextActionId=0;
    TSet<FString> PendingGroups,AbortConfirmGroups;
    FText LastResult;
    bool bLastFailed=false;
    double LastResultUntil=0;
    void RebuildRows();
    UFUNCTION() void Action(FName Name,int32 Index);
};
