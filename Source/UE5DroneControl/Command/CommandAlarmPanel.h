#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CommandAlarmPanel.generated.h"

UCLASS()
class UE5DRONECONTROL_API UCommandAlarmPanel : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetMapService(class UCommandMapInteractionService* Service);
    void Refresh();
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
private:
    UPROPERTY() TObjectPtr<class UScrollBox> Rows;
    TWeakObjectPtr<class UCommandAlertStore> Store;
    TWeakObjectPtr<class UCommandMapInteractionService> Map;
    UFUNCTION() void AlertChanged(int32 Id);
    UFUNCTION() void ActionRequested(FName Action, int32 Id);
};
