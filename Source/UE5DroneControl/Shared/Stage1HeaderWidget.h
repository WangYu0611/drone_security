#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Stage1HeaderWidget.generated.h"

/** Same localized shell in all roles; never owns business state. */
UCLASS()
class UE5DRONECONTROL_API UStage1HeaderWidget : public UUserWidget {
    GENERATED_BODY()
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry&, float) override;
private:
    UPROPERTY() TObjectPtr<class UTextBlock> Identity;
    UPROPERTY() TObjectPtr<class UTextBlock> Health;
    UPROPERTY() TObjectPtr<class UTextBlock> Detail;
};
