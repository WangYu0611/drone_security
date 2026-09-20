#pragma once
#include "CoreMinimal.h"
#include "Components/Button.h"
#include "CommandActionButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCommandAction, FName, Action, int32, ItemId);

/** Small presentation control carrying an action, never business or selection state. */
UCLASS()
class UE5DRONECONTROL_API UCommandActionButton : public UButton
{
    GENERATED_BODY()
public:
    UCommandActionButton(const FObjectInitializer& ObjectInitializer);
    void Configure(FName InAction, int32 InItemId = 0);
    UPROPERTY() FCommandAction OnAction;
private:
    FName Action;
    int32 ItemId = 0;
    UFUNCTION() void Clicked();
};
