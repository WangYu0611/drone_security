#pragma once
#include "CoreMinimal.h"

namespace FleetSelection {
enum class EOperation { Replace, Add, Toggle };
inline bool Contains(const FVector2D& Start,const FVector2D& End,const FVector2D& Point) {
    return !Point.ContainsNaN() && Point.X>=FMath::Min(Start.X,End.X) && Point.X<=FMath::Max(Start.X,End.X)
        && Point.Y>=FMath::Min(Start.Y,End.Y) && Point.Y<=FMath::Max(Start.Y,End.Y);
}
inline TArray<int32> Apply(const TArray<int32>& Current,const TArray<int32>& Hits,EOperation Operation) {
    TArray<int32> Result=Operation==EOperation::Replace?TArray<int32>{}:Current;
    TSet<int32> Seen;
    for(int32 Id:Hits)if(Id>0 && !Seen.Contains(Id)) {
        Seen.Add(Id);
        if(Operation==EOperation::Toggle && Result.Contains(Id))Result.Remove(Id);else Result.AddUnique(Id);
    }
    Result.Sort();return Result;
}
}
