#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "SecurityPlanPanel.generated.h"

UCLASS()
class UE5DRONECONTROL_API USecurityPlanPanel : public UUserWidget {
    GENERATED_BODY()
public:
    void Refresh();
    void SetEditorEnabled(bool Enabled);
    bool IsEditorEnabled() const { return bEditor; }
    int32 GetAssignedUAV() const;
protected:
    virtual void NativeOnInitialized() override;
private:
    bool bMap=false,bEditor=false,bRefreshing=false,bPending=false,bConfirm=false;
    FString SelectedPlan,SelectedMission,LoadedRoute,LastSelection;
    int64 LoadedVersion=-1;
    int64 ViewedVersion=-1;
    UPROPERTY() TObjectPtr<class UTextBlock> Summary;
    UPROPERTY() TObjectPtr<class UTextBlock> Result;
    UPROPERTY() TObjectPtr<UComboBoxString> Plans;
    UPROPERTY() TObjectPtr<UComboBoxString> Missions;
    UPROPERTY() TObjectPtr<UComboBoxString> UAVs;
    UPROPERTY() TObjectPtr<class UEditableTextBox> PlanName;
    UPROPERTY() TObjectPtr<class UEditableTextBox> Description;
    UPROPERTY() TObjectPtr<class UEditableTextBox> MissionName;
    UPROPERTY() TObjectPtr<class UVerticalBox> Editing;
    UPROPERTY() TObjectPtr<class UVerticalBox> Content;
    TArray<FString> PlanIds,MissionIds,UavIds;
    TArray<FString> PlanLabels,MissionLabels,UavLabels;
    UFUNCTION() void Action(FName Name,int32 Id);
    UFUNCTION() void PlanSelected(FString Item,ESelectInfo::Type Type);
    UFUNCTION() void MissionSelected(FString Item,ESelectInfo::Type Type);
    void Submit(const TSharedRef<class FJsonObject>& Request);
    void LoadRoute(bool Force=false);
    TSharedPtr<class FJsonObject> CurrentMission() const;
};
