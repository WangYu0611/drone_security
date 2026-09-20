// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DroneSelectableInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UDroneSelectableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implement on any Actor that can be selected as a drone (e.g. MultiDroneCharacter,
 * RealTimeDroneReceiver).  DroneOpsPlayerController uses this interface to
 * identify click/hover targets and obtain their DroneId.
 */
class UE5DRONECONTROL_API IDroneSelectableInterface
{
	GENERATED_BODY()

public:
	/** Return the DroneId associated with this actor. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "DroneSelectable")
	int32 GetDroneId() const;
	virtual int32 GetDroneId_Implementation() const { return 0; }

	/** Called when this drone becomes the primary selected drone. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "DroneSelectable")
	void OnPrimarySelected();
	virtual void OnPrimarySelected_Implementation() {}

	/** Called when this drone is added to / removed from the secondary selection. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "DroneSelectable")
	void OnSecondarySelected(bool bSelected);
	virtual void OnSecondarySelected_Implementation(bool bSelected) {}

	/** Called when the cursor hovers over or leaves this drone. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "DroneSelectable")
	void OnHoveredChanged(bool bHovered);
	virtual void OnHoveredChanged_Implementation(bool bHovered) {}

	/** Called when this drone is deselected entirely. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "DroneSelectable")
	void OnDeselected();
	virtual void OnDeselected_Implementation() {}
};
