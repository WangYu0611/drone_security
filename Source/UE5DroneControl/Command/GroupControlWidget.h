#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GroupControlWidget.generated.h"

UCLASS()
class UE5DRONECONTROL_API UGroupControlWidget : public UUserWidget {
    GENERATED_BODY()
public:
    void Refresh();
    // Opt-in Command presentation; preserves the existing execution actions and guards.
    void ConfigureFigmaLayout();
    // Fleet selection is acknowledged independently; only a new move must wait for it.
    void SetSelectionPending(bool bInSelectionPending);
    FString GetManagedGroup() const { return ManagedGroup; }
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY() TObjectPtr<class UTextBlock> Summary;
    UPROPERTY() TObjectPtr<class UTextBlock> Result;
    UPROPERTY() TObjectPtr<class UVerticalBox> MoveInputs;
    UPROPERTY() TObjectPtr<class UEditableTextBox> Latitude;
    UPROPERTY() TObjectPtr<class UEditableTextBox> Longitude;
    UPROPERTY() TObjectPtr<class UEditableTextBox> Altitude;
    UPROPERTY() TObjectPtr<class UEditableTextBox> AreaRadius;
    UPROPERTY() TMap<FName,TObjectPtr<class UCommandActionButton>> Buttons;
    FString ExecutionId,ManagedGroup,AbortGroup;
    bool bPending=false,bAbortConfirm=false,bSelectionPending=false;
    UFUNCTION() void Action(FName Name,int32 Id);
};
