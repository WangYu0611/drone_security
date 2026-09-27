#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PathEditor/DronePathVisual.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRouteVisualEncodingTest,"DroneOps.P55.RouteVisualEncoding",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRouteVisualEncodingTest::RunTest(const FString&)
{
    const TArray<double> H{40,60,100,140,80};const TArray<float> S{0,2,6,10,15};
    const auto V=RouteVisual::Build(H,S,true,1,ERouteVisualState::Planning,{});
    TestEqual(TEXT("closed segment count"),V.Num(),5);
    TestEqual(TEXT("closing start"),V.Last().StartWaypointIndex,4);TestEqual(TEXT("closing end"),V.Last().EndWaypointIndex,0);
    TestEqual(TEXT("closing speed preserves last waypoint rule"),V.Last().EffectiveSpeed,15.f);
    TestEqual(TEXT("incoming speed"),V[0].EffectiveSpeed,2.f);
    TestTrue(TEXT("endpoint gradients continuous across joint"),V[0].EndColor==V[1].StartColor && V.Last().EndColor==V[0].StartColor);
    TestTrue(TEXT("low and high differ"),V[0].StartColor!=V[2].EndColor);
    TestTrue(TEXT("minimum span prevents one metre full scale"),RouteVisual::AltitudeColor(41,40,41).R<.1f);
    TestTrue(TEXT("speed strictly monotonic"),RouteVisual::FlowRate(2,1)<RouteVisual::FlowRate(6,1) && RouteVisual::FlowRate(6,1)<RouteVisual::FlowRate(12,1));
    TestTrue(TEXT("1 to 15 visibly distinct rate ratio"),RouteVisual::FlowRate(15,1)>10*RouteVisual::FlowRate(1,1));
    TestEqual(TEXT("zero is visual fallback"),RouteVisual::FlowRate(0,1),RouteVisual::FlowRate(1,1));
    for(auto State:{ERouteVisualState::Planning,ERouteVisualState::Confirmed,ERouteVisualState::Active,ERouteVisualState::Completed}){
        const auto C=RouteVisual::Build(H,S,true,1,State,{1,4});
        TestTrue(TEXT("conflict overrides both layers and closure"),C[1].StartColor==FLinearColor::Red && C[1].HaloColor==FLinearColor::Red && C[4].bConflict);
        TestTrue(TEXT("normal core independent of state"),C[0].StartColor==V[0].StartColor);
        TestEqual(TEXT("data not mutated"),S[0],0.f);TestEqual(TEXT("height not mutated"),H[3],140.);
    }return true;
}
#endif
