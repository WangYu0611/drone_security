// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimpleCoordinateService.h"

USimpleCoordinateService::USimpleCoordinateService()
{
}

FVector USimpleCoordinateService::WorldToNed_Implementation(const FVector& WorldLocation) const
{
	// UE5 coordinates (cm) to NED (meters)
	// UE5: X=Forward, Y=Right, Z=Up
	// NED: X=North, Y=East, Z=Down
	// Conversion: N=X/100, E=Y/100, D=-Z/100
	return FVector(
		WorldLocation.X / 100.0f,  // North (meters)
		WorldLocation.Y / 100.0f,  // East (meters)
		-WorldLocation.Z / 100.0f  // Down (meters, negated)
	);
}

FVector USimpleCoordinateService::NedToWorld_Implementation(const FVector& NedLocation) const
{
	// NED (meters) to UE5 coordinates (cm)
	// NED: X=North, Y=East, Z=Down
	// UE5: X=Forward, Y=Right, Z=Up
	// Conversion: X=N*100, Y=E*100, Z=-D*100
	return FVector(
		NedLocation.X * 100.0f,   // Forward (cm)
		NedLocation.Y * 100.0f,   // Right (cm)
		-NedLocation.Z * 100.0f   // Up (cm, negated)
	);
}

FVector USimpleCoordinateService::WorldToGeographic_Implementation(const FVector& WorldLocation) const
{
	// Not supported in simple coordinate service
	return FVector::ZeroVector;
}

FVector USimpleCoordinateService::GeographicToWorld_Implementation(double Latitude, double Longitude, double Altitude) const
{
	// Not supported in simple coordinate service
	return FVector::ZeroVector;
}

bool USimpleCoordinateService::IsGeographicSupported_Implementation() const
{
	return false;
}

bool USimpleCoordinateService::IsCoordinateSystemReady_Implementation() const
{
	// Simple coordinate service is always ready
	return true;
}
