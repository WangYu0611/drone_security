// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneOps/Core/DroneOpsTypes.h"
#include "DroneTelemetryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTelemetrySnapshotUpdated, const FDroneTelemetrySnapshot&, Snapshot);

/**
 * Component for caching and broadcasting real-time telemetry.
 * Attach to ARealTimeDroneReceiver.
 * After parsing YAML data, call PushSnapshot() to update state and
 * propagate to DroneRegistrySubsystem.
 */
UCLASS(ClassGroup = "DroneOps", meta = (BlueprintSpawnableComponent))
class UE5DRONECONTROL_API UDroneTelemetryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneTelemetryComponent();

	// Offline timeout in seconds; if no packet received within this time
	// the availability is marked as Lost.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Telemetry")
	float OfflineTimeoutSec = 3.0f;

	// ---- Called by receiver to push a new snapshot ----

	UFUNCTION(BlueprintCallable, Category = "Telemetry")
	void PushSnapshot(const FDroneTelemetrySnapshot& Snapshot);

	// ---- Query ----

	UFUNCTION(BlueprintPure, Category = "Telemetry")
	const FDroneTelemetrySnapshot& GetCurrentSnapshot() const { return CurrentSnapshot; }

	UFUNCTION(BlueprintPure, Category = "Telemetry")
	bool HasValidTelemetry() const { return bHasValidTelemetry; }

	UFUNCTION(BlueprintPure, Category = "Telemetry")
	float GetSecondsSinceLastUpdate() const;

	// Fired every time PushSnapshot is called with new data
	UPROPERTY(BlueprintAssignable, Category = "Telemetry")
	FOnTelemetrySnapshotUpdated OnSnapshotUpdated;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	FDroneTelemetrySnapshot CurrentSnapshot;

	bool bHasValidTelemetry = false;
	double LastUpdateRealTime = 0.0;

	// Push the current snapshot into DroneRegistrySubsystem
	void PropagateToRegistry();
};
