#pragma once
#include "CoreMinimal.h"

/** Local presentation defaults; never a UAV position or a Backend write. */
struct FMapStartupSettings
{
    bool bEnabled=false;
    bool bMenuOverride=false;
    bool bLaunchOverride=false;
    bool bRealtimeLaunch=false;
    FVector LongitudeLatitudeHeight=FVector::ZeroVector;
    double ViewDistanceMeters=1200.;
    double PitchDegrees=-50.;
    double YawDegrees=-35.;

    bool IsValid() const;
    /** Live positions may lie outside photorealistic 3D coverage. Start from the
     * existing global 2D basemap; explicit later map-mode changes remain local. */
    bool StartsIn2D() const { return bEnabled && bLaunchOverride && bRealtimeLaunch; }
    static FMapStartupSettings Read();
    /** Explicit Demo/Realtime launch origins require a complete WGS84/ellipsoid tuple.
     * Invalid explicit arguments disable the configured fallback. No arguments retain it.
     * This input-only helper never changes aircraft, Backend state, or process arguments. */
    void ResolveLaunchArguments(const TCHAR* CommandLine);
    void ResolveMenuOverride(class UWorld* World);
    bool ApplyOrigin(class UWorld* World) const;
};
