// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GroundProjectionSettingsWidget.generated.h"

class UButton;
class USlider;
class UTextBlock;

/**
 * Always-available CesiumWorld control for vertical ground-projection rays.
 *
 * The widget is built in C++ when no Blueprint tree is supplied, which keeps
 * runtime behaviour deterministic while still allowing a designer subclass.
 */
UCLASS(Blueprintable)
class UE5DRONECONTROL_API UGroundProjectionSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** True while the pointer or slider capture is inside this panel. */
	static bool IsPanelInteractive() { return bStaticPanelInteractive; }

	UFUNCTION(BlueprintPure, Category = "GroundProjection")
	float GetDensityValue() const;

	UFUNCTION(BlueprintPure, Category = "GroundProjection")
	float GetDensityStepSize() const;

	/** Read-only product copy snapshot used by external acceptance tooling. */
	UFUNCTION(BlueprintPure, Category = "GroundProjection|Diagnostics")
	FString GetProductCopySnapshot() const;

	UFUNCTION(BlueprintCallable, Category = "GroundProjection")
	void SetDensityValue(float NormalizedDensity);

	/** Clear all projection histories and update the panel status text. */
	UFUNCTION(BlueprintCallable, Category = "GroundProjection")
	int32 ClearAllProjectionRays();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

private:
	static bool bStaticPanelInteractive;

	UPROPERTY(Transient)
	TObjectPtr<USlider> DensitySlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DensityValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ClearButton;

	bool bPointerInside = false;
	bool bSliderCaptured = false;

	void BuildFallbackWidgetTree();
	void RefreshInteractionState();
	void RefreshDensityReadout(float NormalizedDensity);

	UFUNCTION()
	void OnDensityChanged(float NormalizedDensity);

	UFUNCTION()
	void OnClearButtonClicked();

	UFUNCTION()
	void OnSliderCaptureBegin();

	UFUNCTION()
	void OnSliderCaptureEnd();
};
