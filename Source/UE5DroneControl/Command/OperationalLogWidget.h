#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "OperationalLogWidget.generated.h"
UCLASS()
class UE5DRONECONTROL_API UOperationalLogWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Refresh();
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;
private:
    UPROPERTY() TObjectPtr<class UScrollBox> Scroll;
    UPROPERTY() TObjectPtr<class UTextBlock> Rows;
    UPROPERTY() TObjectPtr<class UTextBlock> Counts;
    UPROPERTY() TObjectPtr<UComboBoxString> Filter;
    uint64 ClearedThrough = 0;
    FDelegateHandle Handle;
    UFUNCTION() UWidget* FilterLabel(FString Value);
    UFUNCTION() void ClearDisplay();
    UFUNCTION() void FilterChanged(FString Value,ESelectInfo::Type Type);
};
