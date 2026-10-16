#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "Camera/CameraActor.h"
#include "Map/FleetSelection.h"
#include "Map/FleetTargeting.h"
#include "Map/MapFleetInteractionWidget.h"
#include "Map/MapShellWidget.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandMapInteractionService.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Shared/ExecutionPresentation.h"
using namespace PlanUI;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFleetSelectionTest,"DroneOps.Fleet.Selection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFleetSelectionTest::RunTest(const FString&) {
    using namespace FleetSelection;
    TestTrue(TEXT("reverse drag contains boundary"),Contains(FVector2D(200,200),FVector2D(100,100),FVector2D(100,180)));
    TestFalse(TEXT("outside is not selected"),Contains(FVector2D(100,100),FVector2D(200,200),FVector2D(99,180)));
    TestTrue(TEXT("replace selection"),Apply({1,2},{3,4},EOperation::Replace)==TArray<int32>{3,4});
    TestTrue(TEXT("Shift adds without duplicates"),Apply({1,2},{2,3,3},EOperation::Add)==TArray<int32>{1,2,3});
    TestTrue(TEXT("Ctrl toggles each ID once"),Apply({1,2},{2,3,3},EOperation::Toggle)==TArray<int32>{1,3});
    TestTrue(TEXT("empty normal box clears"),Apply({1},{},EOperation::Replace).IsEmpty());
    TestTrue(TEXT("empty additive box preserves"),Apply({1},{},EOperation::Add)==TArray<int32>{1});
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFleetTargetingTest,"DroneOps.Fleet.TargetProjection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFleetTargetingTest::RunTest(const FString&) {
    FVector Point(99,99,99);
    TestTrue(TEXT("oblique 3D ray hits anchor flight plane"),FleetTargeting::IntersectFlightPlane(FVector(0,0,10000),FVector(1,2,-2),FVector(0,0,3000),Point));
    TestTrue(TEXT("horizontal location is the oblique ray intersection"),Point.Equals(FVector(3500,7000,3000),.0001));
    const FVector Before=Point;
    TestFalse(TEXT("horizon ray cannot fabricate a target"),FleetTargeting::IntersectFlightPlane(FVector(0,0,10000),FVector(1,0,0),FVector(0,0,3000),Point));
    TestFalse(TEXT("sky ray pointing away cannot create target"),FleetTargeting::IntersectFlightPlane(FVector(0,0,10000),FVector(1,0,1),FVector(0,0,3000),Point));
    TestFalse(TEXT("far intersection outside existing Backend domain rejected"),FleetTargeting::IntersectFlightPlane(FVector(0,0,10000),FVector(1,0,-.0001),FVector(0,0,3000),Point));
    TestTrue(TEXT("invalid rays preserve last output"),Point.Equals(Before));
    double Height=12;TestTrue(TEXT("decimal ellipsoid height parses"),FleetTargeting::ParseHeight(TEXT(" 90.5 "),Height));TestEqual(TEXT("height precision retained"),Height,90.5);
    for(const auto* Bad:{TEXT(""),TEXT("nan"),TEXT("inf"),TEXT("1e309"),TEXT("90oops")})TestFalse(TEXT("invalid height rejected"),FleetTargeting::ParseHeight(Bad,Height));
    TestEqual(TEXT("invalid text cannot replace accepted height"),Height,90.5);return true;
}
class FFleetMapWorkflow:public IAutomationLatentCommand {
    FAutomationTestBase* Test;double Began=FPlatformTime::Seconds();int Step=0;
    FString VideoBefore,Group;FVector2D TargetLocal;int BeforeExecutions=0;
    FVector CameraBefore,LastCameraSample=FVector::ZeroVector;bool View3D=false,bRequested3D=false;double ExpectedHeight=0;int StableCameraFrames=0;
public:
    explicit FFleetMapWorkflow(FAutomationTestBase* In,bool In3D=false):Test(In),View3D(In3D){}
    bool Update() override {
        if(FPlatformTime::Seconds()-Began>90){Test->AddError(FString::Printf(TEXT("Fleet workflow timeout, step %d"),Step));return true;}
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W)return false;
        auto* PC=Cast<ADroneOpsPlayerController>(W->GetFirstPlayerController());if(!PC || !PC->GetCommandScreenManager())return false;
        auto* Manager=PC->GetCommandScreenManager();auto* Shell=Manager->GetMapShell();auto* F=Shell?Shell->GetFleetInteraction():nullptr;
        auto* S=W->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();auto* R=W->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
        if(!F || !S->IsHydrated() || F->bPending)return false;F->Refresh();
        auto All=Object(S->GetPlans(),TEXT("executions"));
        if(Step==0) {
            if(View3D){auto* Map=Manager->GetMapService();if(!bRequested3D){Map->Enter3DMode();bRequested3D=true;return false;}
                if(FMath::Abs(Map->GetMapCamera()->GetActorRotation().Pitch+55.f)>.05f)return false;
                const FVector Camera=Map->GetMapCamera()->GetActorLocation();StableCameraFrames=Camera.Equals(LastCameraSample,.001)?StableCameraFrames+1:0;LastCameraSample=Camera;if(StableCameraFrames<6)return false;
                Test->TestEqual(TEXT("real map service is in 3D"),Map->GetMapMode(),ECommandMapMode::Map3D);}
            if(R->GetFriendlyDroneDescriptors().Num()!=4 || F->GetCachedGeometry().GetLocalSize().X<100)return false;
            FVector2D Min(DBL_MAX,DBL_MAX),Max(-DBL_MAX,-DBL_MAX);
            for(int Id=1;Id<=4;++Id){FVector2D P;if(!F->Project(Id,P))return false;Min.X=FMath::Min(Min.X,P.X);Min.Y=FMath::Min(Min.Y,P.Y);Max.X=FMath::Max(Max.X,P.X);Max.Y=FMath::Max(Max.Y,P.Y);}
            FJsonSerializer::Serialize(S->GetVideoView().ToSharedRef(),TJsonWriterFactory<>::Create(&VideoBefore));
            CameraBefore=Manager->GetMapService()->GetMapCamera()->GetActorLocation();
            F->SetMode(EMapFleetMode::BoxSelect);
            const auto& G=F->GetCachedGeometry();Test->TestTrue(TEXT("native event entry accepts box gesture"),F->BeginPointer(G.LocalToAbsolute(Max+FVector2D(10,10)),false,false));
            F->EndPointer(G.LocalToAbsolute(Min-FVector2D(10,10)),true);TargetLocal=(Min+Max)*.5+FVector2D(100,120);
            ++Step;return false;
        }
        if(Step==1) {
            if(R->GetMultiSelectedDrones().Num()!=4 || F->bSelectionPending)return false;
            Test->TestTrue(TEXT("box selection leaves map camera unchanged"),Manager->GetMapService()->GetMapCamera()->GetActorLocation().Equals(CameraBefore,.01));
            FString Video;FJsonSerializer::Serialize(S->GetVideoView().ToSharedRef(),TJsonWriterFactory<>::Create(&Video));Test->TestEqual(TEXT("selection never retargets Video"),Video,VideoBefore);
            BeforeExecutions=All->Values.Num();F->SetMode(EMapFleetMode::GroupMove);const auto Screen=F->GetCachedGeometry().LocalToAbsolute(TargetLocal);
            F->BeginPointer(Screen,false,false);F->EndPointer(Screen,true);
            if(View3D){const FVector Horizontal=F->TargetGeographic;const double StartHeight=Horizontal.Z;
                for(const auto& Bad:TArray<FString>{TEXT("nan"),TEXT("1e308"),FString::SanitizeFloat(StartHeight+10001.)}){
                    F->TargetHeightInput->SetText(User(Bad));F->TargetHeightInput->OnTextChanged.Broadcast(F->TargetHeightInput->GetText());F->Action(TEXT("confirm"),0);
                    Test->TestFalse(TEXT("invalid 3D height disables execution confirmation"),F->Buttons[TEXT("confirm")]->GetIsEnabled());Test->TestFalse(TEXT("invalid height never submits Backend execution"),F->bPending);
                    Test->TestEqual(TEXT("invalid height retains last valid target"),F->TargetGeographic.Z,StartHeight);
                }
                ExpectedHeight=StartHeight+50.;F->TargetHeightInput->SetText(User(FString::SanitizeFloat(ExpectedHeight)));F->TargetHeightInput->OnTextChanged.Broadcast(F->TargetHeightInput->GetText());
                Test->TestEqual(TEXT("3D height edits preserve longitude"),F->TargetGeographic.X,Horizontal.X);Test->TestEqual(TEXT("3D height edits preserve latitude"),F->TargetGeographic.Y,Horizontal.Y);
                Test->TestEqual(TEXT("3D target takes the requested ellipsoid height"),F->TargetGeographic.Z,ExpectedHeight);
                F->Action(TEXT("height_up"),0);Test->TestEqual(TEXT("raise by ten metres"),F->TargetGeographic.Z,ExpectedHeight+10.);
                F->Action(TEXT("height_down"),0);Test->TestEqual(TEXT("lower by ten metres"),F->TargetGeographic.Z,ExpectedHeight);
                Manager->GetMapService()->Enter2DMode();F->Refresh();Test->TestFalse(TEXT("switching map mode invalidates pending 3D target"),F->bHasTarget);
                Manager->GetMapService()->Enter3DMode();F->Refresh();Test->TestFalse(TEXT("switching back never resurrects old target"),F->bHasTarget);
                // Camera settles before the next projection; verify height and
                // snapshot integration after restaging from a stable 3D view.
                F->CancelMode();Step=5;return false;
            }
            Test->TestTrue(TEXT("target click creates preview"),F->bHasTarget);Test->TestEqual(TEXT("one Route V2 preview per selected UAV"),F->PreviewRoutes.Num(),4);
            for(int Id=1;Id<=4;++Id){const auto* Path=F->PreviewRoutes.Find(FString::FromInt(Id));const auto* Display=R->GetReceiverActor(Id);
                Test->TestTrue(TEXT("preview begins at visible acknowledged UAV position"),Path && Display && !Path->Waypoints.IsEmpty() && Path->Waypoints[0].Location.Equals(Display->GetActorLocation(),.1));
                if(Path && !Path->GetSegmentVisuals().IsEmpty())Test->TestEqual(TEXT("preview speed flow matches Backend execution speed"),Path->GetSegmentVisuals()[0].EffectiveSpeed,float(Object(S->GetPlans(),TEXT("mock_execution"))->GetNumberField(TEXT("default_speed_mps"))));}
            Test->TestEqual(TEXT("preview never creates Backend execution"),All->Values.Num(),BeforeExecutions);
            Test->TestTrue(TEXT("Esc cancels armed target"),F->CancelMode());Test->TestEqual(TEXT("cancel destroys preview actors"),F->PreviewRoutes.Num(),0);
            F->SetMode(EMapFleetMode::GroupMove);F->BeginPointer(Screen,false,false);F->EndPointer(Screen,true);F->Action(TEXT("confirm"),0);++Step;return false;
        }
        if(Step==2) {
            auto Latest=ExecutionUI::Latest(S->GetPlans(),TEXT(""),true);if(!Latest)return false;
            if(Field(Latest,TEXT("control_kind"))!=TEXT("GROUP_MOVE")){Test->AddError(TEXT("Wrong execution owner"));return true;}
            Group=Field(Latest,TEXT("group_id"));if(Field(Latest,TEXT("state"))!=TEXT("EXECUTING"))return false;
            int Count=0;for(const auto& Item:All->Values)if(Field(Item.Value->AsObject(),TEXT("group_id"))==Group)++Count;
            Test->TestEqual(TEXT("Map role creates a complete four-UAV group"),Count,4);Test->TestFalse(TEXT("ACK retires preview ownership"),F->bHasTarget);
            if(View3D)for(const auto& Item:All->Values){const auto E=Item.Value->AsObject();if(Field(E,TEXT("group_id"))==Group){const auto Route=Object(E,TEXT("route_snapshot"));
                const auto End=Route->GetArrayField(TEXT("waypoints")).Last()->AsObject();Test->TestEqual(TEXT("Backend immutable trajectory receives requested 3D height"),End->GetNumberField(TEXT("altitude")),ExpectedHeight);}}
            F->Action(TEXT("pause"),0);++Step;return false;
        }
        if(Step==3) {
            for(const auto& Item:All->Values)if(Field(Item.Value->AsObject(),TEXT("group_id"))==Group && Field(Item.Value->AsObject(),TEXT("state"))!=TEXT("PAUSED"))return false;
            Test->TestFalse(TEXT("occupied UAVs cannot be moved again"),F->UnavailableReason().IsEmpty());F->Action(TEXT("abort"),0);++Step;return false;
        }
        if(Step==4) {
            for(const auto& Item:All->Values)if(Field(Item.Value->AsObject(),TEXT("group_id"))==Group && Field(Item.Value->AsObject(),TEXT("state"))!=TEXT("ABORTED"))return false;
            Test->TestTrue(TEXT("abort releases entire selection"),F->UnavailableReason().IsEmpty());F->SetMode(EMapFleetMode::Browse);
            auto* Map=Manager->GetMapService();Map->SetRouteEditCameraLocked(true);F->SetMode(EMapFleetMode::BoxSelect);
            Test->TestEqual(TEXT("route edit camera lock blocks fleet mode"),F->GetMode(),EMapFleetMode::Browse);Map->SetRouteEditCameraLocked(false);return true;
        }
        if(Step==5){if(FMath::Abs(Manager->GetMapService()->GetMapCamera()->GetActorRotation().Pitch+55.f)>.05f)return false;
            F->SetMode(EMapFleetMode::GroupMove);const auto Screen=F->GetCachedGeometry().LocalToAbsolute(TargetLocal);F->BeginPointer(Screen,false,false);F->EndPointer(Screen,true);
            if(!Test->TestTrue(TEXT("3D target restaged after mode change"),F->bHasTarget))return true;
            F->TargetHeightInput->SetText(User(FString::SanitizeFloat(ExpectedHeight)));F->TargetHeightInput->OnTextChanged.Broadcast(F->TargetHeightInput->GetText());Test->TestEqual(TEXT("3D preview has four world trajectories"),F->PreviewRoutes.Num(),4);
            for(int Id=1;Id<=4;++Id){const auto* Path=F->PreviewRoutes.Find(FString::FromInt(Id));Test->TestTrue(TEXT("3D preview rises from each actual aircraft"),Path && Path->Waypoints.Num()==2 && Path->Waypoints[1].Location.Z>Path->Waypoints[0].Location.Z+1000);}
            Test->TestEqual(TEXT("3D height preview has no execution side effects"),All->Values.Num(),BeforeExecutions);F->Action(TEXT("confirm"),0);Step=2;return false;
        }
        return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFleetWorkflowTest,"DroneOps.Fleet.Workflow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFleetWorkflowTest::RunTest(const FString&) {
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FFleetMapWorkflow(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFleetWorkflow3DTest,"DroneOps.Fleet.ThreeDimensionalWorkflow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFleetWorkflow3DTest::RunTest(const FString&) {
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FFleetMapWorkflow(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
