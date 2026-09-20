// Copyright Epic Games, Inc. All Rights Reserved.

#include "DroneOps/Drone/DroneGroundProjectionComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ConfigCacheIni.h"
#include "ProceduralMeshComponent.h"

namespace
{
constexpr TCHAR GroundProjectionSection[] = TEXT("DroneDisplay");
constexpr TCHAR GroundProjectionEnabledKey[] = TEXT("bShowGroundProjectionRay");
constexpr TCHAR GroundProjectionDensityKey[] = TEXT("GroundProjectionDensity");
}

UDroneGroundProjectionComponent::UDroneGroundProjectionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDroneGroundProjectionComponent::BeginPlay()
{
	Super::BeginPlay();

	RefreshCachedSettings();
	TrailPoints.Reset();
	bHasRecordAnchor = false;

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	RayMesh = NewObject<UProceduralMeshComponent>(Owner, TEXT("GroundProjectionRayMesh"));
	if (!RayMesh)
	{
		return;
	}

	RayMesh->SetupAttachment(Owner->GetRootComponent());
	// Mesh vertices use world coordinates and must not follow later owner movement.
	RayMesh->SetAbsolute(true, true, true);
	RayMesh->SetWorldLocation(FVector::ZeroVector);
	RayMesh->SetWorldRotation(FRotator::ZeroRotator);
	RayMesh->SetWorldScale3D(FVector::OneVector);
	RayMesh->RegisterComponent();
	RayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RayMesh->SetCastShadow(false);
	RayMesh->SetVisibility(bCachedEnabled);

	CreateRayMaterial();
}

void UDroneGroundProjectionComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	if (RayMesh)
	{
		RayMesh->DestroyComponent();
		RayMesh = nullptr;
	}
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UDroneGroundProjectionComponent::CreateRayMaterial()
{
	if (!RayMesh)
	{
		return;
	}

	// Reuse the existing translucent, unlit, vertex-colour material.
	UMaterialInterface* MaterialSource = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/DroneOps/Materials/M_CurtainBase.M_CurtainBase"));
	if (!MaterialSource)
	{
		MaterialSource = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[GroundProjection] M_CurtainBase is missing; using DefaultMaterial fallback."));
	}

	if (MaterialSource)
	{
		RayMaterial = UMaterialInstanceDynamic::Create(MaterialSource, this);
	}
}

void UDroneGroundProjectionComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	if (++FrameCounter >= CacheRefreshInterval)
	{
		FrameCounter = 0;
		RefreshCachedSettings();
	}

	const float CurrentTime = World->GetTimeSeconds();
	const int32 OldCount = TrailPoints.Num();
	PruneExpiredPoints(CurrentTime);
	if (TrailPoints.Num() != OldCount)
	{
		bMeshDirty = true;
	}

	if (!bCachedEnabled)
	{
		if (RayMesh && RayMesh->IsVisible())
		{
			RayMesh->SetVisibility(false);
		}
		return;
	}

	if (RayMesh && !RayMesh->IsVisible())
	{
		RayMesh->SetVisibility(true);
		bMeshDirty = true;
	}

	const FVector DronePosition = Owner->GetActorLocation();
	FVector GroundHit;
	if (TraceToGround(DronePosition, GroundHit))
	{
		const bool bShouldRecord = !bHasRecordAnchor
			|| FVector::DistSquared(LastRecordAnchor, DronePosition) >= FMath::Square(MinRecordDistance);

		if (bShouldRecord)
		{
			FTrailPoint& NewPoint = TrailPoints.AddDefaulted_GetRef();
			NewPoint.AirPosition = DronePosition;
			NewPoint.GroundPosition = GroundHit;
			NewPoint.Timestamp = CurrentTime;
			LastRecordAnchor = DronePosition;
			bHasRecordAnchor = true;

			const int32 ExcessPointCount = TrailPoints.Num() - FMath::Max(2, MaxTrailPoints);
			if (ExcessPointCount > 0)
			{
				TrailPoints.RemoveAt(0, ExcessPointCount, EAllowShrinking::No);
			}
			bMeshDirty = true;
		}
	}

	MeshRefreshAccumulator += DeltaTime;
	if (MeshRefreshAccumulator >= MeshRefreshIntervalSeconds)
	{
		// A periodic refresh updates per-ray alpha even while the drone is stationary.
		if (bMeshDirty || !TrailPoints.IsEmpty())
		{
			RebuildRayMesh(CurrentTime);
		}
		MeshRefreshAccumulator = 0.0f;
	}
}

bool UDroneGroundProjectionComponent::TraceToGround(
	const FVector& Start,
	FVector& OutHitPoint) const
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DroneGroundProjection), false);
	QueryParams.AddIgnoredActor(Owner);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

	FHitResult HitResult;
	const bool bHit = World->LineTraceSingleByObjectType(
		HitResult,
		Start,
		Start + FVector(0.0f, 0.0f, -MaxTraceDistance),
		ObjectQueryParams,
		QueryParams);
	if (bHit)
	{
		OutHitPoint = HitResult.ImpactPoint;
	}
	return bHit;
}

void UDroneGroundProjectionComponent::PruneExpiredPoints(float CurrentTime)
{
	int32 RemoveCount = 0;
	for (const FTrailPoint& Point : TrailPoints)
	{
		if (CurrentTime - Point.Timestamp <= TrailLifetime)
		{
			break;
		}
		++RemoveCount;
	}

	if (RemoveCount > 0)
	{
		TrailPoints.RemoveAt(0, RemoveCount, EAllowShrinking::No);
	}
}

void UDroneGroundProjectionComponent::BuildInterpolatedRayPoints(
	float SpacingCm,
	TArray<FTrailPoint>& OutPoints) const
{
	OutPoints.Reset();
	if (TrailPoints.IsEmpty())
	{
		return;
	}

	const int32 RenderLimit = FMath::Max(2, MaxRenderedRays);
	const float SafeSpacing = FMath::Max(1.0f, SpacingCm);
	OutPoints.Reserve(FMath::Min(RenderLimit, TrailPoints.Num() * 4));
	OutPoints.Add(TrailPoints[0]);

	float DistanceUntilNextPoint = SafeSpacing;
	for (int32 SegmentIndex = 1;
		SegmentIndex < TrailPoints.Num() && OutPoints.Num() < RenderLimit;
		++SegmentIndex)
	{
		const FTrailPoint& SegmentStart = TrailPoints[SegmentIndex - 1];
		const FTrailPoint& SegmentEnd = TrailPoints[SegmentIndex];
		const float SegmentLength = FVector::Distance(
			SegmentStart.AirPosition,
			SegmentEnd.AirPosition);
		if (SegmentLength <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		float DistanceAlongSegment = 0.0f;
		while (DistanceAlongSegment + DistanceUntilNextPoint <= SegmentLength
			&& OutPoints.Num() < RenderLimit)
		{
			DistanceAlongSegment += DistanceUntilNextPoint;
			const float Alpha = FMath::Clamp(
				DistanceAlongSegment / SegmentLength,
				0.0f,
				1.0f);

			FTrailPoint& NewPoint = OutPoints.AddDefaulted_GetRef();
			NewPoint.AirPosition = FMath::Lerp(
				SegmentStart.AirPosition,
				SegmentEnd.AirPosition,
				Alpha);
			NewPoint.GroundPosition = FMath::Lerp(
				SegmentStart.GroundPosition,
				SegmentEnd.GroundPosition,
				Alpha);
			NewPoint.Timestamp = FMath::Lerp(
				SegmentStart.Timestamp,
				SegmentEnd.Timestamp,
				Alpha);
			DistanceUntilNextPoint = SafeSpacing;
		}

		DistanceUntilNextPoint -= SegmentLength - DistanceAlongSegment;
	}

	// Preserve the live endpoint even if the render limit or spacing interval
	// was reached before the final raw sample.
	const FTrailPoint& LatestPoint = TrailPoints.Last();
	if (!OutPoints.Last().AirPosition.Equals(LatestPoint.AirPosition, 0.1f))
	{
		if (OutPoints.Num() >= RenderLimit)
		{
			OutPoints.Last() = LatestPoint;
		}
		else
		{
			OutPoints.Add(LatestPoint);
		}
	}
}

void UDroneGroundProjectionComponent::RebuildRayMesh(float CurrentTime)
{
	if (!RayMesh)
	{
		return;
	}

	if (TrailPoints.IsEmpty())
	{
		RayMesh->ClearMeshSection(0);
		LastRenderedRayCount = 0;
		LastRenderedCurtainSegmentCount = 0;
		bMeshDirty = false;
		return;
	}

	if (CachedDensity >= LegacyMaximumDensityThreshold)
	{
		RebuildLegacyCurtainMesh(CurrentTime);
		bMeshDirty = false;
		return;
	}

	LastRenderedCurtainSegmentCount = 0;
	const float EffectiveSpacing = GetEffectiveRaySpacingCm();
	TArray<FTrailPoint> RenderPoints;
	BuildInterpolatedRayPoints(EffectiveSpacing, RenderPoints);

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	Vertices.Reserve(RenderPoints.Num() * 8);
	Normals.Reserve(RenderPoints.Num() * 8);
	UV0.Reserve(RenderPoints.Num() * 8);
	VertexColors.Reserve(RenderPoints.Num() * 8);
	Triangles.Reserve(RenderPoints.Num() * 24);

	LastRenderedRayCount = 0;
	const float EffectiveHalfWidth = FMath::Lerp(
		FMath::Max(0.1f, RayHalfWidth),
		FMath::Max(RayHalfWidth, DenseRayHalfWidth),
		FMath::Clamp(CachedDensity, 0.0f, 1.0f));
	for (const FTrailPoint& Point : RenderPoints)
	{
		if (FVector::DistSquared(Point.AirPosition, Point.GroundPosition) < 1.0f)
		{
			continue;
		}

		const float Age = FMath::Max(0.0f, CurrentTime - Point.Timestamp);
		const float RemainingLife = TrailLifetime - Age;
		float Alpha = RayColor.A;
		if (FadeOutDuration > KINDA_SMALL_NUMBER && RemainingLife <= FadeOutDuration)
		{
			Alpha *= FMath::Clamp(RemainingLife / FadeOutDuration, 0.0f, 1.0f);
		}
		const FLinearColor VertexColor(RayColor.R, RayColor.G, RayColor.B, Alpha);

		auto AddDoubleSidedRibbon = [&](
			const FVector& WidthOffset,
			const FVector& Normal)
		{
			const int32 BaseVertex = Vertices.Num();
			Vertices.Add(Point.AirPosition + WidthOffset);
			Vertices.Add(Point.GroundPosition + WidthOffset);
			Vertices.Add(Point.AirPosition - WidthOffset);
			Vertices.Add(Point.GroundPosition - WidthOffset);

			for (int32 VertexIndex = 0; VertexIndex < 4; ++VertexIndex)
			{
				Normals.Add(Normal);
				VertexColors.Add(VertexColor);
			}
			UV0.Add(FVector2D(0.0f, 0.0f));
			UV0.Add(FVector2D(0.0f, 1.0f));
			UV0.Add(FVector2D(1.0f, 0.0f));
			UV0.Add(FVector2D(1.0f, 1.0f));

			// Front.
			Triangles.Append({
				BaseVertex, BaseVertex + 1, BaseVertex + 2,
				BaseVertex + 2, BaseVertex + 1, BaseVertex + 3
			});
			// Back.
			Triangles.Append({
				BaseVertex, BaseVertex + 2, BaseVertex + 1,
				BaseVertex + 2, BaseVertex + 3, BaseVertex + 1
			});
		};

		AddDoubleSidedRibbon(
			FVector(EffectiveHalfWidth, 0.0f, 0.0f),
			FVector::YAxisVector);
		AddDoubleSidedRibbon(
			FVector(0.0f, EffectiveHalfWidth, 0.0f),
			FVector::XAxisVector);
		++LastRenderedRayCount;
	}

	if (LastRenderedRayCount == 0)
	{
		RayMesh->ClearMeshSection(0);
	}
	else
	{
		RayMesh->CreateMeshSection_LinearColor(
			0,
			Vertices,
			Triangles,
			Normals,
			UV0,
			VertexColors,
			Tangents,
			false);
		if (RayMaterial)
		{
			RayMesh->SetMaterial(0, RayMaterial);
		}
	}

	bMeshDirty = false;
}

void UDroneGroundProjectionComponent::RebuildLegacyCurtainMesh(float CurrentTime)
{
	if (!RayMesh)
	{
		return;
	}

	// This is intentionally the same topology as the pre-slider implementation:
	// every adjacent 50 cm raw sample is connected from the drone path to ground.
	if (TrailPoints.Num() < 2)
	{
		RayMesh->ClearMeshSection(0);
		LastRenderedRayCount = 0;
		LastRenderedCurtainSegmentCount = 0;
		return;
	}

	const int32 SegmentCount = TrailPoints.Num() - 1;
	const int32 VertexCount = TrailPoints.Num() * 2;

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	Vertices.Reserve(VertexCount);
	Normals.Reserve(VertexCount);
	UV0.Reserve(VertexCount);
	VertexColors.Reserve(VertexCount);
	Triangles.Reserve(SegmentCount * 12);

	for (int32 Index = 0; Index < TrailPoints.Num(); ++Index)
	{
		const FTrailPoint& Point = TrailPoints[Index];
		const float Age = FMath::Max(0.0f, CurrentTime - Point.Timestamp);
		const float RemainingLife = TrailLifetime - Age;
		float Alpha = RayColor.A;
		if (FadeOutDuration > KINDA_SMALL_NUMBER && RemainingLife <= FadeOutDuration)
		{
			Alpha *= FMath::Clamp(RemainingLife / FadeOutDuration, 0.0f, 1.0f);
		}
		const FLinearColor VertexColor(RayColor.R, RayColor.G, RayColor.B, Alpha);
		const float UCoordinate = static_cast<float>(Index) / SegmentCount;

		Vertices.Add(Point.AirPosition);
		Vertices.Add(Point.GroundPosition);
		VertexColors.Add(VertexColor);
		VertexColors.Add(VertexColor);
		UV0.Add(FVector2D(UCoordinate, 0.0f));
		UV0.Add(FVector2D(UCoordinate, 1.0f));
		Normals.Add(FVector::YAxisVector);
		Normals.Add(FVector::YAxisVector);
	}

	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		const int32 TopA = Index * 2;
		const int32 BottomA = TopA + 1;
		const int32 TopB = TopA + 2;
		const int32 BottomB = TopA + 3;

		// Front and back faces match the original double-sided curtain.
		Triangles.Append({
			TopA, BottomA, TopB,
			TopB, BottomA, BottomB,
			TopA, TopB, BottomA,
			TopB, BottomB, BottomA
		});
	}

	RayMesh->CreateMeshSection_LinearColor(
		0,
		Vertices,
		Triangles,
		Normals,
		UV0,
		VertexColors,
		Tangents,
		false);
	if (RayMaterial)
	{
		RayMesh->SetMaterial(0, RayMaterial);
	}

	float TotalPathLength = 0.0f;
	for (int32 Index = 1; Index < TrailPoints.Num(); ++Index)
	{
		TotalPathLength += FVector::Distance(
			TrailPoints[Index - 1].AirPosition,
			TrailPoints[Index].AirPosition);
	}
	LastRenderedRayCount = FMath::Max(
		TrailPoints.Num(),
		FMath::FloorToInt(TotalPathLength / DenseRaySpacingCm) + 1);
	LastRenderedCurtainSegmentCount = SegmentCount;
}

void UDroneGroundProjectionComponent::ClearTrail()
{
	TrailPoints.Reset();
	if (const AActor* Owner = GetOwner())
	{
		LastRecordAnchor = Owner->GetActorLocation();
		bHasRecordAnchor = true;
	}
	else
	{
		bHasRecordAnchor = false;
	}
	LastRenderedRayCount = 0;
	LastRenderedCurtainSegmentCount = 0;
	bMeshDirty = false;
	MeshRefreshAccumulator = 0.0f;
	if (RayMesh)
	{
		RayMesh->ClearMeshSection(0);
	}
}

bool UDroneGroundProjectionComponent::IsGroundProjectionEnabled()
{
	bool bEnabled = true;
	if (GConfig)
	{
		GConfig->GetBool(
			GroundProjectionSection,
			GroundProjectionEnabledKey,
			bEnabled,
			GGameIni);
	}
	return bEnabled;
}

float UDroneGroundProjectionComponent::GetGroundProjectionDensity()
{
	float Density = 1.0f;
	if (GConfig)
	{
		GConfig->GetFloat(
			GroundProjectionSection,
			GroundProjectionDensityKey,
			Density,
			GGameIni);
	}
	return FMath::Clamp(Density, 0.0f, 1.0f);
}

void UDroneGroundProjectionComponent::SetGroundProjectionDensity(
	UObject* WorldContextObject,
	float NormalizedDensity)
{
	const float ClampedDensity = FMath::Clamp(NormalizedDensity, 0.0f, 1.0f);
	if (GConfig)
	{
		GConfig->SetFloat(
			GroundProjectionSection,
			GroundProjectionDensityKey,
			ClampedDensity,
			GGameIni);
		GConfig->Flush(false, GGameIni);
	}
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[GroundProjection] Density=%.3f spacing=%.1fcm"),
		ClampedDensity,
		DensityToRaySpacingCm(ClampedDensity));

	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		TArray<UDroneGroundProjectionComponent*> Components;
		ActorIt->GetComponents<UDroneGroundProjectionComponent>(Components);
		for (UDroneGroundProjectionComponent* Component : Components)
		{
			if (Component)
			{
				Component->CachedDensity = ClampedDensity;
				Component->bMeshDirty = true;
				Component->MeshRefreshAccumulator = MeshRefreshIntervalSeconds;
			}
		}
	}
}

float UDroneGroundProjectionComponent::DensityToRaySpacingCm(float NormalizedDensity)
{
	return FMath::Lerp(
		SparseRaySpacingCm,
		DenseRaySpacingCm,
		FMath::Clamp(NormalizedDensity, 0.0f, 1.0f));
}

int32 UDroneGroundProjectionComponent::ClearAllTrails(UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return 0;
	}

	int32 ClearedComponentCount = 0;
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		TArray<UDroneGroundProjectionComponent*> Components;
		ActorIt->GetComponents<UDroneGroundProjectionComponent>(Components);
		for (UDroneGroundProjectionComponent* Component : Components)
		{
			if (Component)
			{
				Component->ClearTrail();
				++ClearedComponentCount;
			}
		}
	}
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[GroundProjection] Cleared %d component trail(s)"),
		ClearedComponentCount);
	return ClearedComponentCount;
}

float UDroneGroundProjectionComponent::GetEffectiveRaySpacingCm() const
{
	// Render spacing is independent from the raw trajectory sampling distance.
	// Dense modes interpolate along recorded segments, so retaining the old
	// MinRecordDistance clamp here would collapse 99%..86% to the same 50 cm
	// spacing and recreate the visual jump immediately below 100%.
	return FMath::Max(1.0f, DensityToRaySpacingCm(CachedDensity));
}

bool UDroneGroundProjectionComponent::IsRenderingLegacyMaximumDensity() const
{
	return CachedDensity >= LegacyMaximumDensityThreshold
		&& LastRenderedCurtainSegmentCount > 0;
}

void UDroneGroundProjectionComponent::RefreshCachedSettings()
{
	const bool bNewEnabled = IsGroundProjectionEnabled();
	const float NewDensity = GetGroundProjectionDensity();
	if (bCachedEnabled != bNewEnabled || !FMath::IsNearlyEqual(CachedDensity, NewDensity))
	{
		bMeshDirty = true;
		MeshRefreshAccumulator = MeshRefreshIntervalSeconds;
	}
	bCachedEnabled = bNewEnabled;
	CachedDensity = NewDensity;
}
