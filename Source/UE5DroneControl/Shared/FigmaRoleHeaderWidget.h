#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FigmaRoleHeaderWidget.generated.h"

/** Figma role header for Map and Video; displays acknowledged shared state. */
UCLASS()
class UE5DRONECONTROL_API UFigmaRoleHeaderWidget : public UUserWidget {
    GENERATED_BODY()
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry&, float) override;
private:
    UPROPERTY() TObjectPtr<class UTextBlock> Identity;
    UPROPERTY() TObjectPtr<class UTextBlock> Clock;
    UPROPERTY() TObjectPtr<class UTextBlock> Feedback;
    UPROPERTY() TArray<TObjectPtr<class UTextBlock>> HealthLabels;
    UPROPERTY() TArray<TObjectPtr<class UImage>> HealthDots;
    UPROPERTY() TObjectPtr<class UImage> OnlineDot;
    UPROPERTY() TObjectPtr<class UImage> OfflineDot;
    UPROPERTY() TObjectPtr<class UButton> ChineseButton;
    UPROPERTY() TObjectPtr<class UButton> EnglishButton;
    UFUNCTION() void Chinese();
    UFUNCTION() void English();
    void Refresh();
    float RefreshElapsed = 0.f;
};
