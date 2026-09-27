#pragma once
#include "CoreMinimal.h"

// Presentation-only inputs: resolved heights share one datum, never persisted.
enum class ERouteVisualState : uint8 { Planning, Confirmed, Active, Completed };
struct FDronePathSegmentVisualState
{
    int32 StartWaypointIndex=0, EndWaypointIndex=0;
    double StartAltitude=0, EndAltitude=0;
    float EffectiveSpeed=1, FlowRate=0;
    FLinearColor StartColor, EndColor, HaloColor;
    bool bConflict=false;
};
namespace RouteVisual
{
    constexpr double MinimumAltitudeSpan=30.;
    UE5DRONECONTROL_API FLinearColor AltitudeColor(double Height,double Minimum,double Maximum);
    UE5DRONECONTROL_API float FlowRate(float Speed,float DefaultSpeed);
    UE5DRONECONTROL_API FLinearColor HaloColor(ERouteVisualState State);
    UE5DRONECONTROL_API TArray<FDronePathSegmentVisualState> Build(const TArray<double>& Heights,const TArray<float>& IncomingSpeeds,bool Closed,float DefaultSpeed,ERouteVisualState State,const TSet<int32>& Conflicts);
}
