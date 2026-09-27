#include "DronePathVisual.h"
FLinearColor RouteVisual::AltitudeColor(double H,double Min,double Max)
{
    const FLinearColor Stops[]{FLinearColor(.015f,.09f,.85f),FLinearColor(0,.8f,1),FLinearColor(1,.85f,.04f),FLinearColor(1,.32f,.015f),FLinearColor(1,.12f,.015f)};
    const double T=FMath::Clamp((H-Min)/FMath::Max(Max-Min,MinimumAltitudeSpan),0.,1.)*4;
    const int32 I=FMath::Min(3,FMath::FloorToInt(T));
    return FMath::Lerp(Stops[I],Stops[I+1],float(T-I));
}
float RouteVisual::FlowRate(float Speed,float DefaultSpeed)
{
    const float Effective=Speed>KINDA_SMALL_NUMBER?Speed:DefaultSpeed;
    return FMath::GetMappedRangeValueClamped(FVector2D(1,15),FVector2D(.15,2.4),Effective);
}
FLinearColor RouteVisual::HaloColor(ERouteVisualState State)
{
    switch(State){
    case ERouteVisualState::Planning:return FLinearColor(1,.5f,.04f);
    case ERouteVisualState::Active:return FLinearColor(.02f,1,.22f);
    case ERouteVisualState::Completed:return FLinearColor(.28f,.32f,.36f);
    default:return FLinearColor(0,.8f,1);
    }
}
TArray<FDronePathSegmentVisualState> RouteVisual::Build(const TArray<double>& H,const TArray<float>& S,bool Closed,float DefaultSpeed,ERouteVisualState State,const TSet<int32>& Conflicts)
{
    TArray<FDronePathSegmentVisualState> Out;if(H.Num()<2 || H.Num()!=S.Num())return Out;
    double Min=H[0],Max=H[0];for(double V:H){Min=FMath::Min(Min,V);Max=FMath::Max(Max,V);}
    for(int32 I=0;I<H.Num()-1+(Closed && H.Num()>2?1:0);++I){
        const int32 J=(I+1)%H.Num();auto& V=Out.AddDefaulted_GetRef();
        V.StartWaypointIndex=I;V.EndWaypointIndex=J;V.StartAltitude=H[I];V.EndAltitude=H[J];
        // Closing speed is the existing last waypoint speed, not WP0's zero.
        const float Speed=S[J==0?I:J];V.EffectiveSpeed=Speed>KINDA_SMALL_NUMBER?Speed:DefaultSpeed;
        V.FlowRate=FlowRate(Speed,DefaultSpeed);V.bConflict=Conflicts.Contains(I);
        V.StartColor=V.bConflict?FLinearColor::Red:AltitudeColor(H[I],Min,Max);
        V.EndColor=V.bConflict?FLinearColor::Red:AltitudeColor(H[J],Min,Max);
        V.HaloColor=V.bConflict?FLinearColor::Red:HaloColor(State);
    }return Out;
}
