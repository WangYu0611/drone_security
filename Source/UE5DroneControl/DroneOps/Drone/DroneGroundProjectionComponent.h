// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneGroundProjectionComponent.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Records the drone trail and renders vertical projections to the ground.
 *
 * Raw samples are always kept at the maximum configured resolution. The global
 * density setting only resamples the rendered geometry, so changing the slider
 * immediately affects both existing and future rays.
 *
 * At the exact maximum setting, the component restores the legacy connected
 * curtain topology so the right endpoint is visually identical to the original
 * implementation. Lower settings render independent crossed rays.
 *
 * Each sample owns its creation timestamp and expires independently. Cleanup
 * therefore never depends on a mirror drone reproducing a shadow drone route.
 */
UCLASS(ClassGroup = "DroneOps", meta = (BlueprintSpawnableComponent))
class UE5DRONECONTROL_API UDroneGroundProjectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneGroundProjectionComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	/** Ray color. Vertex alpha is additionally multiplied by the lifetime fade. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GroundProjection")
	FLinearColor RayColor = FLinearColor(0.0f, 1.0f, 0.0f, 0.8f);

	/** Maximum downward trace distance in centimetres. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GroundProjection", meta = (ClampMin = "100.0"))
	float MaxTraceDistance = 500000.0f;

	/**
	 * Raw trail sampling distance in centimetres.
	 * Keep this at 50 cm to preserve the previous maximum-density behaviour.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GroundProjection|Trail", meta = (ClampMin = "1.0"))
	float MinRecordDistance = 50.0f;

	/** Lifetime of every ray in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GroundProjection|Trail", meta = (ClampMin = "0.1"))
	float TrailLifetime = 30.0f;

	/** Fade duration at the end of each ray's lifetime, in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GroundProjection|Trail", meta = (ClampMin = "0.0"))
	float FadeOutDuration = 5.0f;

	/** Half-width of each crossed ray ribbon at 0% density, in centimetres. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GroundProjection|Rendering", meta = (ClampMin = "0.1"))
	float RayHalfWidth = 1.0f;

	/** Half-width of each crossed ray ribbon immediately below 100% density. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GroundProjection|Rendering", meta = (ClampMin = "0.1"))
	float DenseRayHalfWidth = 2.0f;

	/** Maximum raw samples retained per component as a defensive memory bound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GroundProjection|Trail", meta = (ClampMin = "2"))
	int32 MaxTrailPoints = 4096;

	/** Maximum interpolated vertical rays rendered below the legacy 100% mode. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GroundProjection|Rendering", meta = (ClampMin = "2"))
	int32 MaxRenderedRays = 8192;

	/** Read the global visibility switch from [DroneDisplay]. */
	UFUNCTION(BlueprintPure, Category = "GroundProjection")
	static bool IsGroundProjectionEnabled();

	/** Read the normalized global density setting: 0 = sparse, 1 = current maximum density. */
	UFUNCTION(BlueprintPure, Category = "GroundProjection")
	static float GetGroundProjectionDensity();

	/**
	 * Persist a normalized density and apply it immediately to every component in this world.
	 */
	UFUNCTION(BlueprintCallable, Category = "GroundProjection", meta = (WorldContext = "WorldContextObject"))
	static void SetGroundProjectionDensity(UObject* WorldContextObject, float NormalizedDensity);

	/** Convert normalized density into effective spacing in centimetres. */
	UFUNCTION(BlueprintPure, Category = "GroundProjection")
	static float DensityToRaySpacingCm(float NormalizedDensity);

	/** Clear this component's complete ray history immediately. */
	UFUNCTION(BlueprintCallable, Category = "GroundProjection")
	void ClearTrail();

	/** Clear all projection histories in this world. Returns the number of components cleared. */
	UFUNCTION(BlueprintCallable, Category = "GroundProjection", meta = (WorldContext = "WorldContextObject"))
	static int32 ClearAllTrails(UObject* WorldContextObject);

	/** Runtime diagnostics used by external acceptance tooling and support screens. */
	UFUNCTION(BlueprintPure, Category = "GroundProjection|Diagnostics")
	int32 GetRecordedSampleCount() const { return TrailPoints.Num(); }

	/**
	 * Number of discrete rays, or 25 cm-equivalent columns while the continuous
	 * legacy curtain is active. Use GetRenderedCurtainSegmentCount for topology.
	 */
	UFUNCTION(BlueprintPure, Category = "GroundProjection|Diagnostics")
	int32 GetRenderedRayCount() const { return LastRenderedRayCount; }

	/** Number of connected legacy curtain segments currently rendered. */
	UFUNCTION(BlueprintPure, Category = "GroundProjection|Diagnostics")
	int32 GetRenderedCurtainSegmentCount() const { return LastRenderedCurtainSegmentCount; }

	/** True when the exact right endpoint is rendering the legacy curtain topology. */
	UFUNCTION(BlueprintPure, Category = "GroundProjection|Diagnostics")
	bool IsRenderingLegacyMaximumDensity() const;

	UFUNCTION(BlueprintPure, Category = "GroundProjection|Diagnostics")
	float GetEffectiveRaySpacingCm() const;

private:
	struct FTrailPoint
	{
		FVector AirPosition;
		FVector GroundPosition;
		float Timestamp = 0.0f;
	};

	TArray<FTrailPoint> TrailPoints;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> RayMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RayMaterial;

	bool bCachedEnabled = true;
	float CachedDensity = 1.0f;
	bool bHasRecordAnchor = false;
	FVector LastRecordAnchor = FVector::ZeroVector;
	int32 FrameCounter = 0;
	bool bMeshDirty = false;
	float MeshRefreshAccumulator = 0.0f;
	int32 LastRenderedRayCount = 0;
	int32 LastRenderedCurtainSegmentCount = 0;

	static constexpr int32 CacheRefreshInterval = 30;
	static constexpr float MeshRefreshIntervalSeconds = 0.1f;
	static constexpr float DenseRaySpacingCm = 25.0f;
	static constexpr float SparseRaySpacingCm = 200.0f;
	static constexpr float LegacyMaximumDensityThreshold = 0.9999f;

	bool TraceToGround(const FVector& Start, FVector& OutHitPoint) const;
	void PruneExpiredPoints(float CurrentTime);
	void BuildInterpolatedRayPoints(float SpacingCm, TArray<FTrailPoint>& OutPoints) const;
	void RebuildRayMesh(float CurrentTime);
	void RebuildLegacyCurtainMesh(float CurrentTime);
	void CreateRayMaterial();
	void RefreshCachedSettings();
};
