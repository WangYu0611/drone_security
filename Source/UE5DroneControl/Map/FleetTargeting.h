#pragma once
#include "CoreMinimal.h"
#include "Misc/DefaultValueHelper.h"

namespace FleetTargeting {
// Select horizontal position on the anchor's current flight plane. Height is a
// separate, explicit geographic value; adjusting it never changes latitude/longitude.
inline bool IntersectFlightPlane(const FVector& Origin,const FVector& Direction,const FVector& Anchor,FVector& Point) {
    if(Origin.ContainsNaN() || Direction.ContainsNaN() || Anchor.ContainsNaN() || FMath::Abs(Direction.Z)<1.e-8)return false;
    const double Distance=(Anchor.Z-Origin.Z)/Direction.Z;
    if(!FMath::IsFinite(Distance) || Distance<=0)return false;
    const FVector Candidate=Origin+Direction*Distance;
    if(Candidate.ContainsNaN() || FVector::Dist2D(Candidate,Anchor)>1000000.)return false; // Existing Backend local domain: 10 km.
    Point=Candidate;return true;
}
inline bool ParseHeight(const FString& Text,double& Height) {
    double Parsed=0;
    if(!FDefaultValueHelper::ParseDouble(Text.TrimStartAndEnd(),Parsed) || !FMath::IsFinite(Parsed))return false;
    Height=Parsed;return true;
}
}
