#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Command/CommandMapInteractionService.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"

// Empty editor world: no Backend, Cesium tiles, network, or simulation fixture.
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FRouteEditCameraLockTest,"DroneOps.P54.RouteEditCameraLock",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FRouteEditCameraLockTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
    Names.Add(TEXT("2D"));Commands.Add(TEXT("2D"));
    Names.Add(TEXT("3D"));Commands.Add(TEXT("3D"));
}
bool FRouteEditCameraLockTest::RunTest(const FString& Parameters)
{
    UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
    auto* Map=NewObject<UCommandMapInteractionService>(World);
    Map->MapCamera=World->SpawnActor<ACameraActor>();
    Map->CurrentMapMode=Parameters==TEXT("2D")?ECommandMapMode::Map2D:ECommandMapMode::Map3D;
    Map->ViewFocus=FVector(100,200,300);Map->ViewYaw=32;
    Map->UpdateCameraTransform();
    Map->ZoomAt(2,nullptr);Map->FocusLocation(FVector(1000,2000,3000));
    Map->AdvanceCamera(.016f); // Enter while damping is in flight.
    const FTransform Before=Map->MapCamera->GetActorTransform();
    const FVector EntryFocus=Map->ActualFocus;
    const double EntryDistance=Map->ActualDistance;
    Map->BeginPointer(EKeys::MiddleMouseButton,{100,100},true,false);
    Map->SetRouteEditCameraLocked(true);
    TestTrue(TEXT("entry has no visual jump"),Map->MapCamera->GetActorTransform().Equals(Before,0));
    TestFalse(TEXT("entry clears old gesture"),Map->GestureButton.IsValid());
    TestTrue(TEXT("editing retires an unfinished automatic focus target"),Map->ViewFocus==EntryFocus && Map->ViewDistance==EntryDistance);
    const float Yaw=Map->ViewYaw,Pitch=Map->View3DPitch;
    const auto Mode=Map->CurrentMapMode;
    Map->BeginPointer(EKeys::LeftMouseButton,{100,100},true,false);
    TestFalse(TEXT("left mouse remains reserved for waypoint edit"),Map->GestureButton.IsValid());
    for(auto Button:{EKeys::MiddleMouseButton,EKeys::RightMouseButton}){
        Map->BeginPointer(Button,{100,100},true,false);
        TestTrue(TEXT("manual camera gesture acquired during edit"),Map->GestureButton==Button);
        Map->MovePointer({800,600},false);
        TestFalse(TEXT("leaving map cancels camera gesture"),Map->GestureButton.IsValid());
    }
    const FVector BeforeRightClickFocus=Map->ViewFocus;
    const double BeforeRightClickDistance=Map->ViewDistance;
    const float BeforeRightClickYaw=Map->ViewYaw,BeforeRightClickPitch=Map->View3DPitch;
    Map->BeginPointer(EKeys::RightMouseButton,{100,100},true,false);
    Map->EndPointer(EKeys::RightMouseButton,{100,100},true);
    TestFalse(TEXT("right click releases its camera gesture during route edit"),Map->GestureButton.IsValid());
    TestTrue(TEXT("right click during route edit preserves all camera targets"),
        Map->ViewFocus==BeforeRightClickFocus && Map->ViewDistance==BeforeRightClickDistance
        && Map->ViewYaw==BeforeRightClickYaw && Map->View3DPitch==BeforeRightClickPitch);
    Map->BeginPointer(EKeys::RightMouseButton,{100,100},true,false);
    Map->MovePointer({140,120},true);
    TestTrue(TEXT("right movement past threshold is a camera drag"),Map->bLeftDragging);
    TestTrue(TEXT("right drag orbits only the 3D camera during route edit"),
        Mode==ECommandMapMode::Map3D?(Map->ViewYaw!=BeforeRightClickYaw && Map->View3DPitch!=BeforeRightClickPitch)
        :(Map->ViewYaw==BeforeRightClickYaw && Map->View3DPitch==BeforeRightClickPitch));
    Map->MovePointer({100,100},true);
    TestTrue(TEXT("returning to press point does not turn a right drag into a click"),Map->bLeftDragging);
    Map->EndPointer(EKeys::RightMouseButton,{100,100},true);
    TestTrue(TEXT("right drag release clears camera gesture state"),!Map->GestureButton.IsValid() && !Map->bLeftDragging);
    Map->PanAtViewport({100,100},1920);
    TestFalse(TEXT("manual pan changes focus during edit"),Map->ViewFocus.Equals(EntryFocus));
    Map->ZoomAt(3,nullptr);TestTrue(TEXT("manual wheel zoom changes distance during edit"),Map->ViewDistance<EntryDistance);
    Map->RotateView({100,30});
    TestTrue(TEXT("3D orbit stays available; 2D stays north-up"),Mode==ECommandMapMode::Map3D?(Map->ViewYaw!=Yaw && Map->View3DPitch!=Pitch):(Map->ViewYaw==Yaw && Map->View3DPitch==Pitch));
    const FVector ManualFocus=Map->ViewFocus;const double ManualDistance=Map->ViewDistance;const float ManualYaw=Map->ViewYaw;
    TestFalse(TEXT("RouteEditLocksFocus"),Map->FocusLocation(FVector(9000)));
    TestFalse(TEXT("RouteEditLocksExecutionRecenter"),Map->FocusExecutionBounds(FBox(FVector(-100000),FVector(100000))));
    TestFalse(TEXT("RouteEditLocksUAVFocus"),Map->FocusDrone(1));
    TestFalse(TEXT("RouteEditLocksAlertFocus"),Map->FocusAlertOnMap(1));
    Map->SetMapMode(Mode==ECommandMapMode::Map2D?ECommandMapMode::Map3D:ECommandMapMode::Map2D);
    TestTrue(TEXT("RouteEditDisablesModeSwitch"),Map->CurrentMapMode==Mode);
    TestTrue(TEXT("automatic focus requests preserve manual camera targets"),Map->ViewFocus==ManualFocus && Map->ViewDistance==ManualDistance && Map->ViewYaw==ManualYaw);
    for(int I=0;I<600;++I)Map->AdvanceCamera(1.f/60);
    TestTrue(TEXT("manual camera smoothly reaches requested focus and zoom"),Map->ActualFocus.Equals(ManualFocus,.01) && FMath::IsNearlyEqual(Map->ActualDistance,ManualDistance,.01));
    TestFalse(TEXT("manual camera actually moves while editing"),Map->MapCamera->GetActorTransform().Equals(Before,.01));
    const FTransform Settled=Map->MapCamera->GetActorTransform();
    Map->SetRouteEditCameraLocked(true);
    TestTrue(TEXT("repeated lock preserves manual view"),Map->ViewFocus==ManualFocus);
    Map->SetRouteEditCameraLocked(false);
    Map->EndPointer(EKeys::MiddleMouseButton,{2000,2000},true);
    TestTrue(TEXT("unlock and stale release do not jump"),Map->MapCamera->GetActorTransform().Equals(Settled,0) && Map->ViewFocus==ManualFocus);
    Map->ZoomAt(1,nullptr);TestTrue(TEXT("zoom also remains available after finish"),Map->ViewDistance<ManualDistance);
    TestTrue(TEXT("FinishEditRestoresFocus"),Map->FocusLocation(FVector(4000)));
    Map->Enter3DMode();Map->RotateView({50,10});
    TestTrue(TEXT("FinishEditRestoresModeAndOrbit"),Map->CurrentMapMode==ECommandMapMode::Map3D && Map->ViewYaw!=Yaw);
    Map->Unbind();
    return true;
}
#endif
