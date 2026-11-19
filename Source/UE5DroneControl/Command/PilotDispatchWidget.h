#pragma once

#include "Blueprint/UserWidget.h"
#include "PilotDispatchWidget.generated.h"

struct FPilotControlAircraft;

UCLASS()
class UPilotDispatchWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void Open(const TArray<FString>& Selection);
    static FText AssignmentText(const FPilotControlAircraft& Aircraft);
    static FText Title();

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    UPROPERTY() TObjectPtr<class UComboBoxString> PilotChoice;
    UPROPERTY() TObjectPtr<class UTextBlock> SelectionText;
    UPROPERTY() TObjectPtr<class UTextBlock> Feedback;
    UPROPERTY() TObjectPtr<class UScrollBox> Assignments;
    UPROPERTY() TObjectPtr<class UCommandActionButton> GrantButton;
    TWeakObjectPtr<class UPilotControlSubsystem> Control;
    TArray<FString> Targets;
    TArray<FString> OnlinePilots;
    TArray<FString> RevokeTargets;
    void Refresh();
    class UTextBlock* MakeText(const FText& Value, int32 Size = 14, bool bBold = false);
    class UCommandActionButton* MakeButton(FName Action, const FText& Label, int32 Index = 0);
    UFUNCTION() void PerformAction(FName Action, int32 Index);
};
