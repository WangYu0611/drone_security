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
    const FVector FrozenFocus=Map->ActualFocus;
    const double FrozenDistance=Map->ActualDistance;
    Map->BeginPointer(EKeys::MiddleMouseButton,{100,100},true,false);
    Map->SetRouteEditCameraLocked(true);
    TestTrue(TEXT("entry has no visual jump"),Map->MapCamera->GetActorTransform().Equals(Before,0));
    TestFalse(TEXT("entry clears old gesture"),Map->GestureButton.IsValid());
    TestTrue(TEXT("frozen target captures current actual pose"),Map->ViewFocus==FrozenFocus && Map->ViewDistance==FrozenDistance);
    const float Yaw=Map->ViewYaw,Pitch=Map->View3DPitch;
    const auto Mode=Map->CurrentMapMode;
    for(auto Button:{EKeys::LeftMouseButton,EKeys::MiddleMouseButton,EKeys::RightMouseButton}){
        Map->BeginPointer(Button,{100,100},true,false);Map->MovePointer({800,600},true);Map->EndPointer(Button,{1000,900},true);
        TestFalse(TEXT("RouteEditLocksCamera: no camera gesture acquired"),Map->GestureButton.IsValid());
    }
    Map->PanView({100,100});Map->RotateView({100,100});Map->ZoomView(3);Map->ZoomAt(-3,nullptr);
    TestFalse(TEXT("RouteEditLocksFocus"),Map->FocusLocation(FVector(9000)));
    TestFalse(TEXT("RouteEditLocksExecutionRecenter"),Map->FocusExecutionBounds(FBox(FVector(-100000),FVector(100000))));
    TestFalse(TEXT("RouteEditLocksUAVFocus"),Map->FocusDrone(1));
    TestFalse(TEXT("RouteEditLocksAlertFocus"),Map->FocusAlertOnMap(1));
    Map->SetMapMode(Mode==ECommandMapMode::Map2D?ECommandMapMode::Map3D:ECommandMapMode::Map2D);
    TestTrue(TEXT("RouteEditDisablesModeSwitch"),Map->CurrentMapMode==Mode);
    for(int I=0;I<600;++I)Map->AdvanceCamera(1.f/60);
    TestTrue(TEXT("all targets remain frozen"),Map->ViewFocus==FrozenFocus && Map->ViewDistance==FrozenDistance && Map->ViewYaw==Yaw && Map->View3DPitch==Pitch);
    TestTrue(TEXT("actual transform remains exactly frozen"),Map->MapCamera->GetActorTransform().Equals(Before,0));
    Map->SetRouteEditCameraLocked(true);
    TestTrue(TEXT("repeated lock is idempotent"),Map->ViewFocus==FrozenFocus);
    Map->SetRouteEditCameraLocked(false);
    Map->EndPointer(EKeys::MiddleMouseButton,{2000,2000},true);
    TestTrue(TEXT("unlock and stale release do not jump"),Map->MapCamera->GetActorTransform().Equals(Before,0) && Map->ViewFocus==FrozenFocus);
    Map->ZoomAt(1,nullptr);TestTrue(TEXT("FinishEditRestoresZoom"),Map->ViewDistance<FrozenDistance);
    TestTrue(TEXT("FinishEditRestoresFocus"),Map->FocusLocation(FVector(4000)));
    Map->Enter3DMode();Map->RotateView({50,10});
    TestTrue(TEXT("FinishEditRestoresModeAndOrbit"),Map->CurrentMapMode==ECommandMapMode::Map3D && Map->ViewYaw!=Yaw);
    Map->Unbind();
    return true;
}
#endif
