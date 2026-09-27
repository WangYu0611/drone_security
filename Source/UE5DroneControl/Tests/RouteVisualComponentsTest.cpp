#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "PathEditor/DronePathActor.h"
#include "PathEditor/DroneWaypointActor.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRouteVisualComponentsTest,"DroneOps.P55.RouteVisualComponents",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRouteVisualComponentsTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();
    TArray<ADronePathActor*> Paths;
    for(int32 R=0;R<3;++R){
        auto* P=World->SpawnActor<ADronePathActor>();P->bSpawnWaypointHandlesInEditor=false;P->bParticipatesInConflictChecks=false;
        P->SetActorLocation(FVector(0,R*10000,0));P->bClosedLoop=true;
        for(int I=0;I<100;++I){FDroneWaypoint W;W.Location=FVector(I*300,(I%2)*400,I==50?14000:4000+(I%5)*2000);W.SegmentSpeed=I%15;P->Waypoints.Add(W);}
        // A vertical segment and a coincident segment must stay finite.
        P->Waypoints[2].Location=P->Waypoints[1].Location+FVector(0,0,3000);P->Waypoints[4].Location=P->Waypoints[3].Location;
        P->RefreshPath();P->SetMapDisplayRadius(40);Paths.Add(P);
        TestNotNull(TEXT("V2 material asset loads"),P->RouteV2Material.Get());
        TestEqual(TEXT("100 segments with core halo and independent joints"),P->GetVisualComponentCount(),300);
        TestEqual(TEXT("linear flight spline retained"),P->PathSpline->GetSplinePointType(2),ESplinePointType::Linear);
        TestTrue(TEXT("finite vertical visual"),!P->SplineMeshComponents[1]->GetStartPosition().ContainsNaN());
        TestTrue(TEXT("zero length safely hidden"),!P->SplineMeshComponents[3]->IsVisible());
        for(auto M:P->SplineMeshComponents){TestEqual(TEXT("core no collision"),M->GetCollisionEnabled(),ECollisionEnabled::NoCollision);TestFalse(TEXT("core no overlap"),M->GetGenerateOverlapEvents());}
        for(auto M:P->HaloComponents)TestEqual(TEXT("halo no collision"),M->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
        for(auto M:P->JointComponents)TestEqual(TEXT("joint no collision"),M->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
    }
    auto* A=Paths[0];auto* Core=A->SplineMeshComponents[0].Get();auto* MID=A->SplineVisualizationMIDs[0].Get();
    const FVector Original=A->Waypoints[1].Location;const float OtherRate=Paths[1]->SplineVisualizationMIDs[0]->K2_GetScalarParameterValue(TEXT("FlowRate"));
    const double Start=FPlatformTime::Seconds();
    for(int I=0;I<100;++I){A->UpdateWaypoint(1,Original+FVector(I,0,0));A->UpdateWaypointSegmentSpeed(1,2+(I%12));}
    AddInfo(FString::Printf(TEXT("100-segment edit smoke: 200 mutations in %.3f seconds"),FPlatformTime::Seconds()-Start));
    TestTrue(TEXT("same topology preserves component identity"),Core==A->SplineMeshComponents[0]);
    TestTrue(TEXT("same topology preserves MID identity"),MID==A->SplineVisualizationMIDs[0]);
    TestTrue(TEXT("route MIDs are independent"),MID!=Paths[1]->SplineVisualizationMIDs[0]);
    TestEqual(TEXT("other route speed unchanged"),Paths[1]->SplineVisualizationMIDs[0]->K2_GetScalarParameterValue(TEXT("FlowRate")),OtherRate);
    A->bSpawnWaypointHandlesInEditor=true;A->SyncWaypointHandles(true);
    auto* Handle=A->WaypointHandleActors[1].Get();const FVector Canonical=A->Waypoints[1].Location;
    const FVector Rendered=Core->GetEndPosition();
    Handle->BeginDeferredPathUpdate();Handle->MoveAlongGizmoAxis(EGizmoAxis::X,500);
    TestTrue(TEXT("drag preview changes core before release"),!Core->GetEndPosition().Equals(Rendered));
    TestFalse(TEXT("preview has no pending unpublished mesh update"),Core->bMeshDirty);
    TestTrue(TEXT("visual segments never cook collision"),Core->GetbNeverNeedsCookedCollisionData());
    TestTrue(TEXT("drag preview cannot mutate flight spline or saved waypoint"),A->Waypoints[1].Location==Canonical && A->PathSpline->GetLocationAtSplinePoint(1,ESplineCoordinateSpace::Local)==Canonical);
    Handle->EndDeferredPathUpdate(false);
    TestTrue(TEXT("cancelled preview restores core"),Core->GetEndPosition().Equals(Rendered));
    const FLinearColor ColorBefore= A->GetSegmentVisuals()[0].StartColor;
    A->UpdateWaypointSegmentSpeed(1,15);
    TestEqual(TEXT("speed edit immediately updates material"),MID->K2_GetScalarParameterValue(TEXT("FlowRate")),RouteVisual::FlowRate(15,1));
    TestTrue(TEXT("speed never recolors altitude"),A->GetSegmentVisuals()[0].StartColor==ColorBefore);
    const auto Data=A->Waypoints;
    A->MarkConflictSegment(0,1);A->RefreshConflictVisualization();
    TestTrue(TEXT("conflict priority material parameter"),MID->K2_GetScalarParameterValue(TEXT("ConflictStrength"))==1);
    A->ClearConflictVisualization();TestTrue(TEXT("conflict removed"),MID->K2_GetScalarParameterValue(TEXT("ConflictStrength"))==0);
    for(auto State:{ERouteVisualState::Planning,ERouteVisualState::Confirmed,ERouteVisualState::Active,ERouteVisualState::Completed})A->SetRoutePresentation(State,true);
    TestTrue(TEXT("presentation leaves business fields unchanged"),A->Waypoints[1].Location==Data[1].Location && A->Waypoints[1].SegmentSpeed==Data[1].SegmentSpeed && A->bClosedLoop);
    TestTrue(TEXT("shader animation needs no actor tick"),!A->IsActorTickEnabled());
    return true;
}
#endif
