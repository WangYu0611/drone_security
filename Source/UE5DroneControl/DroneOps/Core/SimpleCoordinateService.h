// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ICoordinateService.h"
#include "SimpleCoordinateService.generated.h"

/**
 * Simple coordinate service for prototype level (non-Cesium)
 * Assumes UE5 world origin (0,0,0) corresponds to NED origin
 */
UCLASS(BlueprintType)
class UE5DRONECONTROL_API USimpleCoordinateService : public UObject, public ICoordinateService
{
	GENERATED_BODY()

public:
	USimpleCoordinateService();

	// ICoordinateService interface
	virtual FVector WorldToNed_Implementation(const FVector& WorldLocation) const override;
	virtual FVector NedToWorld_Implementation(const FVector& NedLocation) const override;
	virtual FVector WorldToGeographic_Implementation(const FVector& WorldLocation) const override;
	virtual FVector GeographicToWorld_Implementation(double Latitude, double Longitude, double Altitude) const override;
	virtual bool IsGeographicSupported_Implementation() const override;
	virtual bool IsCoordinateSystemReady_Implementation() const override;
};
