#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GroupControlWidget.generated.h"

UCLASS()
class UE5DRONECONTROL_API UGroupControlWidget : public UUserWidget {
    GENERATED_BODY()
public:
    void Refresh();
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY() TObjectPtr<class UTextBlock> Summary;
    UPROPERTY() TObjectPtr<class UTextBlock> Result;
    UPROPERTY() TObjectPtr<class UEditableTextBox> Latitude;
    UPROPERTY() TObjectPtr<class UEditableTextBox> Longitude;
    UPROPERTY() TObjectPtr<class UEditableTextBox> Altitude;
    UPROPERTY() TObjectPtr<class UEditableTextBox> AreaRadius;
    UPROPERTY() TMap<FName,TObjectPtr<class UCommandActionButton>> Buttons;
    FString ExecutionId;
    bool bPending=false;
    UFUNCTION() void Action(FName Name,int32 Id);
};
