#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "PathEditor/DronePathActor.h"
#include "PathEditor/DronePathSaveLibrary.h"

// Presentation ownership only; never writes Backend or editable route data.
class FPlanRouteVisualSet {
public:
    ~FPlanRouteVisualSet(){Reset();}
    static FDronePathSaveData Decode(const TSharedPtr<FJsonObject>& Route,UObject* Coordinates,int32 PathId=1);
    ADronePathActor* Put(UWorld* World,UObject* Coordinates,const FString& Key,const TSharedPtr<FJsonObject>& Route,ERouteVisualState State,bool Selected);
    void Retain(const TSet<FString>& Keys);
    void Reset();
    void Translate(const FVector& Delta);
    void CheckConflicts(ADronePathActor* Editable=nullptr);
    ADronePathActor* Find(const FString& Key) const {const auto* P=Actors.Find(Key);return P?P->Get():nullptr;}
    int32 Num() const {return Actors.Num();}
private:
    TMap<FString,TWeakObjectPtr<ADronePathActor>> Actors;
    TMap<FString,FString> Snapshots;
};
