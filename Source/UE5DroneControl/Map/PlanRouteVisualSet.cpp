#include "Map/PlanRouteVisualSet.h"
#include "Shared/PlanWidgetSupport.h"
#include "DroneOps/Core/ICoordinateService.h"
#include "PathEditor/DronePathConflictLibrary.h"
#include "Engine/World.h"
using namespace PlanUI;
FDronePathSaveData FPlanRouteVisualSet::Decode(const TSharedPtr<FJsonObject>& Route,UObject* C,int32 Id){
    FDronePathSaveData Data;Data.PathId=Id;if(!Route || !C)return Data;
    Route->TryGetBoolField(TEXT("bClosedLoop"),Data.bClosedLoop);
    const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;if(!Route->TryGetArrayField(TEXT("waypoints"),Points))return Data;
    for(const auto& Value:*Points){const auto P=Value->AsObject();if(!P)continue;FDroneWaypointSaveData W;
        P->TryGetStringField(TEXT("altitude_reference"),W.AltitudeReference);
        W.AltitudeOffsetMeters=EllipsoidAltitude(P)-P->GetNumberField(TEXT("altitude"));
        W.Location=ICoordinateService::Execute_GeographicToWorld(C,P->GetNumberField(TEXT("latitude")),P->GetNumberField(TEXT("longitude")),EllipsoidAltitude(P));
        W.SegmentSpeed=P->GetNumberField(TEXT("segmentSpeed"));double Wait=0;P->TryGetNumberField(TEXT("waitTime"),Wait);W.WaitTime=Wait;Data.Waypoints.Add(W);
    }return Data;
}
ADronePathActor* FPlanRouteVisualSet::Put(UWorld* World,UObject* C,const FString& Key,const TSharedPtr<FJsonObject>& Route,ERouteVisualState State,bool Selected){
    if(!World || !C || !Route || !ICoordinateService::Execute_IsCoordinateSystemReady(C))return nullptr;
    FString Snapshot;FJsonSerializer::Serialize(Route.ToSharedRef(),TJsonWriterFactory<>::Create(&Snapshot));
    auto* Path=Find(Key);
    if(!Path){Path=World->SpawnActor<ADronePathActor>();if(!Path)return nullptr;
        Path->PathNumericId=INDEX_NONE;Path->Tags.Add(TEXT("SecurityPlanRouteV2"));Path->bParticipatesInConflictChecks=false;
        Path->bSpawnWaypointHandlesAtRuntime=false;Path->bSpawnWaypointHandlesInEditor=false;Actors.Add(Key,Path);Snapshots.Remove(Key);}
    if(Snapshots.FindRef(Key)!=Snapshot){const auto Data=Decode(Route,C);Path->SetVisualPresentationOffset(FVector::ZeroVector);Path->Waypoints.Empty();Path->bClosedLoop=Data.bClosedLoop;
        for(const auto& Point:Data.Waypoints){FDroneWaypoint W;W.Location=Point.Location;W.SegmentSpeed=Point.SegmentSpeed;W.WaitTime=Point.WaitTime;W.AltitudeReference=Point.AltitudeReference;W.AltitudeOffsetMeters=Point.AltitudeOffsetMeters;Path->Waypoints.Add(W);}
        Path->RefreshPath();Snapshots.Add(Key,Snapshot);
    }
    Path->SetRoutePresentation(State,Selected);return Path;
}
void FPlanRouteVisualSet::Retain(const TSet<FString>& Keys){for(auto It=Actors.CreateIterator();It;++It)if(!Keys.Contains(It.Key())){if(It.Value().IsValid())It.Value()->Destroy();Snapshots.Remove(It.Key());It.RemoveCurrent();}}
void FPlanRouteVisualSet::Reset(){Retain({});}
void FPlanRouteVisualSet::Translate(const FVector& Delta){for(auto& Item:Actors)if(Item.Value.IsValid())Item.Value->SetVisualPresentationOffset(Delta);}
void FPlanRouteVisualSet::CheckConflicts(ADronePathActor* Editable){
    TArray<ADronePathActor*> Paths;for(auto& Item:Actors)if(Item.Value.IsValid())Paths.Add(Item.Value.Get());if(Editable)Paths.AddUnique(Editable);
    // Reuse the existing detector, scoped to the single displayed owner of each route.
    TArray<bool> Previous;for(auto* P:Paths){Previous.Add(P->bParticipatesInConflictChecks);P->bParticipatesInConflictChecks=true;}
    UDronePathConflictLibrary::CheckPathConflictsWithVolume(Paths);
    for(int I=0;I<Paths.Num();++I)Paths[I]->bParticipatesInConflictChecks=Previous[I];
}
