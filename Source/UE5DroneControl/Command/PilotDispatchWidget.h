#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "PilotDispatchWidget.generated.h"

struct FPilotControlAircraft;

UCLASS()
class UPilotDispatchWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void Open(const TArray<FString>& Selection, bool bRequestedHandoff = false);
    static FText AssignmentText(const FPilotControlAircraft& Aircraft);
    static FText Title();
    static FText HandoffRequestsTitle(int32 Count);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    UPROPERTY() TObjectPtr<class UComboBoxString> PilotChoice;
    UPROPERTY() TObjectPtr<class UTextBlock> SelectionText;
    UPROPERTY() TObjectPtr<class UTextBlock> HandoffNotice;
    UPROPERTY() TObjectPtr<class UTextBlock> PilotPrompt;
    UPROPERTY() TObjectPtr<class UTextBlock> Feedback;
    UPROPERTY() TObjectPtr<class UScrollBox> Assignments;
    UPROPERTY() TObjectPtr<class UCommandActionButton> GrantButton;
    TWeakObjectPtr<class UPilotControlSubsystem> Control;
    TArray<FString> Targets;
    TArray<FString> OnlinePilots;
    TArray<FString> RevokeTargets;
    FString ChosenPilot;
    FString OfferedHandoffExecution;
    int64 OfferedHandoffVersion = 0;
    bool bFromHandoffRequest = false;
    void Refresh();
    bool CanGrant() const;
    const FPilotControlAircraft* GetOfferedHandoff() const;
    class UTextBlock* MakeText(const FText& Value, int32 Size = 14, bool bBold = false);
    class UCommandActionButton* MakeButton(FName Action, const FText& Label, int32 Index = 0);
    UFUNCTION() void PilotSelected(FString PilotId, ESelectInfo::Type SelectionType);
    UFUNCTION() void PerformAction(FName Action, int32 Index);
};
