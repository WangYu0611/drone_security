#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DefaultSegmentSpeedWidget.generated.h"

class ADroneOpsPlayerController;
class UButton;
class UEditableTextBox;
class USizeBox;
class UTextBlock;

/** Compact CesiumWorld overlay for the speed used by new waypoints and map-click moves. */
UCLASS()
class UE5DRONECONTROL_API UDefaultSegmentSpeedWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetPathEditController(ADroneOpsPlayerController* InController);
	void SetDisplayedSpeed(float SpeedMps);

	/**
	 * Make the overlay visible without turning it into a click blocker.
	 *
	 * The root must stay SelfHitTestInvisible: AddToViewport anchors it to the whole viewport,
	 * so a plain Visible root would swallow clicks for every panel painted below this one
	 * (this overlay is at ZOrder 60, above all of them) while still letting map clicks through
	 * via SViewport. Only the child controls are meant to be hit-testable.
	 */
	void ShowOverlay();

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TObjectPtr<ADroneOpsPlayerController> PathEditController;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> SpeedInput;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<UButton> ApplyButton;

	UPROPERTY()
	TObjectPtr<UButton> ToggleButton;

	UPROPERTY()
	TObjectPtr<USizeBox> ExpandedPanel;

	UPROPERTY()
	TObjectPtr<UTextBlock> ToggleText;

	UFUNCTION()
	void ApplySpeed();

	UFUNCTION()
	void ToggleExpanded();

	void SetStatus(const FText& Message, const FLinearColor& Color);
	void SetExpanded(bool bShouldExpand);
};
