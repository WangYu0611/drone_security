#pragma once
#include "CoreMinimal.h"
class UWorld;
namespace MapSurface {
// Loaded collision surfaces only; never invent a zero-height datum on failure.
bool Raycast(UWorld* World,const FVector& Start,const FVector& End,FVector& Hit);
bool Height(UWorld* World,UObject* Coordinates,const FVector& Geographic,double& Meters);
bool ClampCamera(UWorld* World,UObject* Coordinates,const FVector& Desired,FVector& Result);
}
