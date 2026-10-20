#pragma once
#include "CoreMinimal.h"

// Layout affects annotation cards only; aircraft and waypoint coordinates never move.
namespace MapLabels {
inline bool ClearOfRects(const FVector2D& Position,const FVector2D& Size,const TArray<FBox2D>& Obstacles){
    const FBox2D Label(Position,Position+Size);for(const auto& Obstacle:Obstacles)if(Label.Intersect(Obstacle))return false;return true;
}
inline bool Place(const FVector2D& Anchor,const FVector2D& Size,const FBox2D& Bounds,
    const TArray<FBox2D>& Occupied,TFunctionRef<bool(const FVector2D&,const FVector2D&)> Clear,FVector2D& Result){
    if(Size.X+12>Bounds.GetSize().X || Size.Y+12>Bounds.GetSize().Y)return false;
    TArray<FVector2D> Candidates{Anchor+FVector2D(-Size.X*.5,24),Anchor+FVector2D(28,-Size.Y*.5),Anchor+FVector2D(-Size.X-28,-Size.Y*.5),Anchor+FVector2D(-Size.X*.5,-Size.Y-24)};
    for(int Row=1;Row<=20;++Row)for(int Sign:{1,-1}){
        const double Y=Anchor.Y-Size.Y*.5+Sign*Row*(Size.Y+8);
        Candidates.Add(FVector2D(Anchor.X+28,Y));Candidates.Add(FVector2D(Anchor.X-Size.X-28,Y));
    }
    for(auto Candidate:Candidates){
        Candidate.X=FMath::Clamp(Candidate.X,Bounds.Min.X+6,Bounds.Max.X-Size.X-6);
        Candidate.Y=FMath::Clamp(Candidate.Y,Bounds.Min.Y+6,Bounds.Max.Y-Size.Y-6);
        const FBox2D Test(Candidate-FVector2D(4),Candidate+Size+FVector2D(4));
        bool Free=Clear(Candidate,Size);for(const auto& Other:Occupied)if(Test.Intersect(Other)){Free=false;break;}
        if(Free){Result=Candidate;return true;}
    }return false;
}
}
