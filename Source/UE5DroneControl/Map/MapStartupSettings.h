#pragma once
#include "CoreMinimal.h"

/** Local presentation defaults; never a UAV position or a Backend write. */
struct FMapStartupSettings
{
    bool bEnabled=false;
    bool bMenuOverride=false;
    FVector LongitudeLatitudeHeight=FVector::ZeroVector;
    double ViewDistanceMeters=1200.;
    double PitchDegrees=-50.;
    double YawDegrees=-35.;

    bool IsValid() const;
    static FMapStartupSettings Read();
    void ResolveMenuOverride(class UWorld* World);
    bool ApplyOrigin(class UWorld* World) const;
};
