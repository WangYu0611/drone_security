#include "Shared/ExecutionPresentation.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRouteOwnershipTest,"DroneOps.P55.RouteOwnership",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRouteOwnershipTest::RunTest(const FString&){
    auto State=MakeShared<FJsonObject>(),Executions=MakeShared<FJsonObject>(),Plans=MakeShared<FJsonObject>();State->SetObjectField(TEXT("executions"),Executions);State->SetObjectField(TEXT("plans"),Plans);
    auto Plan=MakeShared<FJsonObject>();Plan->SetStringField(TEXT("deployment_id"),TEXT("d1"));Plans->SetObjectField(TEXT("p1"),Plan);
    auto Add=[&](const TCHAR* Id,const TCHAR* Mission,const TCHAR* Group,const TCHAR* Status,double Time){auto E=MakeShared<FJsonObject>();
        E->SetStringField(TEXT("execution_id"),Id);E->SetStringField(TEXT("plan_id"),TEXT("p1"));E->SetStringField(TEXT("mission_id"),Mission);E->SetStringField(TEXT("group_id"),Group);
        E->SetStringField(TEXT("deployment_id"),TEXT("d1"));E->SetStringField(TEXT("state"),Status);E->SetNumberField(TEXT("created_at"),Time);Executions->SetObjectField(Id,E);return E;};
    Add(TEXT("old"),TEXT("m1"),TEXT(""),TEXT("COMPLETED"),1);
    auto Current=Add(TEXT("current"),TEXT("m1"),TEXT(""),TEXT("COMPLETED"),2);
    TestEqual(TEXT("legacy empty groups cannot duplicate a Mission"),ExecutionUI::RouteOwners(State,TEXT("p1")).Num(),1);
    Add(TEXT("other"),TEXT("m2"),TEXT("g2"),TEXT("EXECUTING"),3);
    auto Owners=ExecutionUI::RouteOwners(State,TEXT("p1"));TestEqual(TEXT("completed route retained alongside another executing mission"),Owners.Num(),2);TestTrue(TEXT("latest immutable completion retained"),Owners.Contains(Current));
    Add(TEXT("restart"),TEXT("m1"),TEXT("g3"),TEXT("EXECUTING"),4);Owners=ExecutionUI::RouteOwners(State,TEXT("p1"));TestEqual(TEXT("restart replaces completed owner"),Owners.Num(),2);TestFalse(TEXT("old completion excluded"),Owners.Contains(Current));
    Plan->SetStringField(TEXT("deployment_id"),TEXT("d2"));Executions->RemoveField(TEXT("restart"));TestEqual(TEXT("stale deployment completion excluded"),ExecutionUI::RouteOwners(State,TEXT("p1")).Num(),1);
    return true;
}
#endif
