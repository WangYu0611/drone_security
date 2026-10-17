#include "Map/MapSurfaceHeight.h"
#include "DroneOps/Core/ICoordinateService.h"
#include "PathEditor/DronePathActor.h"
#include "PathEditor/DroneWaypointActor.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"

bool MapSurface::Raycast(UWorld* World,const FVector& Start,const FVector& End,FVector& Hit) {
    if(!World || Start.ContainsNaN() || End.ContainsNaN())return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(MapSurfaceHeight),true);
    // Flight visuals and aircraft must never be mistaken for the ground below them.
    for(TActorIterator<ADronePathActor> It(World);It;++It)Params.AddIgnoredActor(*It);
    for(TActorIterator<ADroneWaypointActor> It(World);It;++It)Params.AddIgnoredActor(*It);
    for(TActorIterator<APawn> It(World);It;++It)Params.AddIgnoredActor(*It);
    FHitResult Surface;
    if(!World->LineTraceSingleByObjectType(Surface,Start,End,FCollisionObjectQueryParams(ECC_WorldStatic),Params))return false;
    Hit=Surface.ImpactPoint;return !Hit.ContainsNaN();
}
bool MapSurface::Height(UWorld* World,UObject* Coordinates,const FVector& Geographic,double& Meters) {
    if(!Coordinates || Geographic.ContainsNaN() || !ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates))return false;
    const FVector Above=ICoordinateService::Execute_GeographicToWorld(Coordinates,Geographic.Y,Geographic.X,10000.);
    const FVector Below=ICoordinateService::Execute_GeographicToWorld(Coordinates,Geographic.Y,Geographic.X,-1000.);
    FVector Hit;if(!Raycast(World,Above,Below,Hit))return false;
    const double Sample=ICoordinateService::Execute_WorldToGeographic(Coordinates,Hit).Z;
    if(!FMath::IsFinite(Sample))return false;Meters=Sample;return true;
}
bool MapSurface::ClampCamera(UWorld* World,UObject* Coordinates,const FVector& Desired,FVector& Result) {
    Result=Desired;if(!Coordinates)return false;
    const FVector G=ICoordinateService::Execute_WorldToGeographic(Coordinates,Desired);double Ground=0;
    if(!Height(World,Coordinates,G,Ground) || G.Z>=Ground+5.)return false;
    Result=ICoordinateService::Execute_GeographicToWorld(Coordinates,G.Y,G.X,Ground+5.);return true;
}
