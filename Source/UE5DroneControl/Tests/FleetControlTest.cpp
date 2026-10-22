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
#include "Command/CommandActionButton.h"
#include "Blueprint/WidgetTree.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Shared/ExecutionPresentation.h"
#include "Components/Slider.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "Layout/WidgetPath.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFleetPresentationTest,"DroneOps.Fleet.Presentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFleetPresentationTest::RunTest(const FString&) {
    auto State=MakeShared<FJsonObject>();auto All=MakeShared<FJsonObject>();State->SetObjectField(TEXT("executions"),All);
    auto Add=[&](const FString& Id,const FString& Group,int Uav,double Time,const FString& Plan=TEXT(""),const FString& Status=TEXT("COMPLETED")){
        auto E=MakeShared<FJsonObject>();E->SetStringField(TEXT("execution_id"),Id);E->SetStringField(TEXT("group_id"),Group);E->SetStringField(TEXT("uav_id"),FString::Printf(TEXT("UAV-%02d"),Uav));
        E->SetStringField(TEXT("state"),Status);E->SetNumberField(TEXT("created_at"),Time);E->SetStringField(TEXT("control_kind"),Plan.IsEmpty()?TEXT("GROUP_MOVE"):TEXT("MISSION"));
        E->SetStringField(TEXT("plan_id"),Plan);E->SetStringField(TEXT("mission_id"),Plan.IsEmpty()?TEXT(""):TEXT("mission"));All->SetObjectField(Id,E);
    };
    for(int Id=1;Id<=4;++Id){Add(FString::Printf(TEXT("old%d"),Id),TEXT("old"),Id,1);Add(FString::Printf(TEXT("new%d"),Id),TEXT("new"),Id,2);}
    auto Owners=ExecutionUI::RouteOwners(State,TEXT(""));TestEqual(TEXT("repeated fleet moves retain four routes, not eight"),Owners.Num(),4);
    for(const auto& E:Owners)TestEqual(TEXT("latest group owns all four fleet routes"),Field(E,TEXT("group_id")),FString(TEXT("new")));
    Add(TEXT("subset1"),TEXT("subset"),1,3);Add(TEXT("subset2"),TEXT("subset"),2,3);Owners=ExecutionUI::RouteOwners(State,TEXT(""));
    TestEqual(TEXT("partial new selection retains one route per UAV"),Owners.Num(),4);
    for(const auto& E:Owners)TestEqual(TEXT("unmoved members keep their latest snapshot"),Field(E,TEXT("group_id")),Field(E,TEXT("uav_id"))==TEXT("UAV-01") || Field(E,TEXT("uav_id"))==TEXT("UAV-02")?FString(TEXT("subset")):FString(TEXT("new")));
    for(int Id=3;Id<=4;++Id){Add(FString::Printf(TEXT("plan-old%d"),Id),TEXT("plan-old"),Id,1,TEXT("plan"));Add(FString::Printf(TEXT("plan-live%d"),Id),TEXT("plan-live"),Id,2,TEXT("plan"),TEXT("EXECUTING"));}
    Owners=ExecutionUI::RouteOwners(State,TEXT("plan"));TestEqual(TEXT("Security Plan Mission still has one complete execution group"),Owners.Num(),2);
    for(const auto& E:Owners)TestEqual(TEXT("Security Plan keeps latest active owner"),Field(E,TEXT("group_id")),FString(TEXT("plan-live")));return true;
}
class FFleetMapWorkflow:public IAutomationLatentCommand {
    FAutomationTestBase* Test;double Began=FPlatformTime::Seconds(),Next=0;int Step=0,MemberCount=4;
    FString VideoBefore,Group,SecondGroup,ThirdGroup,CaptureDirectory;FVector2D TargetLocal;int BeforeExecutions=0;
    FVector CameraBefore,LastCameraSample=FVector::ZeroVector;bool View3D=false,bRequestedMode=false,bReselection=false;double ExpectedHeight=0;int StableCameraFrames=0;
    // Inspect the real rendered hit path in this test's PIE window. This does not
    // move the OS cursor and is deliberately not claimed as native mouse evidence.
    bool MapPointAvailable(UMapShellWidget* Shell,UMapFleetInteractionWidget* Fleet,const FVector2D& Local) const {
        const FVector2D Screen=Fleet->GetCachedGeometry().LocalToAbsolute(Local);
        if(!Shell->IsPointOverMap(Screen))return false;
        auto* Client=Fleet->GetWorld()->GetGameViewport();const auto View=Client?Client->GetGameViewportWidget():nullptr;
        const auto Window=Client?Client->GetWindow():nullptr;if(!View || !Window)return false;
        const TArray<TSharedRef<SWindow>> Windows{Window.ToSharedRef()};
        const auto Path=FSlateApplication::Get().LocateWindowUnderMouse(Screen,Windows);
        return Path.IsValid() && Path.Widgets.Last().Widget==View;
    }
    bool ToolbarAction(UMapShellWidget* Shell,const TCHAR* Name) {
        auto* Button=Cast<UCommandActionButton>(Shell->WidgetTree->FindWidget(FName(Name)));
        if(!Test->TestTrue(TEXT("Reselection toolbar action is visible and enabled"),Button && Button->IsVisible() && Button->GetIsEnabled()))return false;
        Button->OnAction.Broadcast(FName(Name),0);return true;
    }
    void CheckPreviewButtons(UMapFleetInteractionWidget* Fleet) {
        const auto Window=Fleet->GetWorld()->GetGameViewport()->GetWindow();
        if(!Test->TestTrue(TEXT("Preview has an owned rendered window"),Window.IsValid()))return;
        const TArray<TSharedRef<SWindow>> Windows{Window.ToSharedRef()};
        for(const TCHAR* Name:{TEXT("confirm"),TEXT("cancel")}){
            const auto* Button=Fleet->Buttons[Name].Get();const auto& Geometry=Button->GetCachedGeometry();
            for(double Height:{.25,.5,.85}){
                const auto Screen=Geometry.LocalToAbsolute(Geometry.GetLocalSize()*FVector2D(.5,Height));
                const auto Path=FSlateApplication::Get().LocateWindowUnderMouse(Screen,Windows);
                bool ReachesButton=false;for(int Index=0;Index<Path.Widgets.Num();++Index)ReachesButton|=Path.Widgets[Index].Widget==Button->GetCachedWidget();
                Test->TestTrue(*FString::Printf(TEXT("Preview %s button remains clickable above footer (%.2f)"),Name,Height),ReachesButton);
            }
        }
    }
    UCommandMapInteractionService* MapService(UMapFleetInteractionWidget* Fleet) const {
        const auto* PC=Cast<ADroneOpsPlayerController>(Fleet->GetOwningPlayer());
        return PC && PC->GetCommandScreenManager()?PC->GetCommandScreenManager()->GetMapService():nullptr;
    }
    bool BlankMapPoint(UMapShellWidget* Shell,UMapFleetInteractionWidget* Fleet,FVector2D& Result) const {
        const auto& Frame=Shell->GetMapViewportGeometry();const FVector2D Size=Frame.GetLocalSize();
        for(const FVector2D Fraction:TArray<FVector2D>{FVector2D(.3,.6),FVector2D(.4,.7),FVector2D(.2,.4),FVector2D(.5,.8)}){
            const FVector2D Local=Fleet->GetCachedGeometry().AbsoluteToLocal(Frame.LocalToAbsolute(Size*Fraction));
            bool Empty=MapPointAvailable(Shell,Fleet,Local);
            for(const auto& D:Fleet->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetFriendlyDroneDescriptors()){
                FVector2D P;if(Fleet->Project(D.DroneId,P) && FVector2D::Distance(P,Local)<32.)Empty=false;
            }
            if(Empty){Result=Local;return true;}
        }return false;
    }
    bool RightClick(UMapShellWidget* Shell,UMapFleetInteractionWidget* Fleet,const FVector2D& Local) {
        if(!Test->TestTrue(TEXT("Right-click destination reaches the rendered map and Slate viewport"),MapPointAvailable(Shell,Fleet,Local))){
            Capture(TEXT("right-click-hit-path-failure"));return false;
        }
        auto* Map=MapService(Fleet);if(!Test->TestNotNull(TEXT("Right-click uses the existing map input service"),Map))return false;
        const FVector2D Screen=Fleet->GetCachedGeometry().LocalToAbsolute(Local);
        Map->BeginPointer(EKeys::RightMouseButton,Screen,true,false);
        Map->EndPointer(EKeys::RightMouseButton,Screen,MapPointAvailable(Shell,Fleet,Local));return true;
    }
    bool RightDragAndEditLockChecks(UMapShellWidget* Shell,UMapFleetInteractionWidget* Fleet) {
        FVector2D Local=FVector2D::ZeroVector;
        if(!Test->TestTrue(TEXT("Camera gesture has a clear rendered map region"),BlankMapPoint(Shell,Fleet,Local)))return false;
        const FVector2D End=Local+FVector2D(24,12);
        if(!Test->TestTrue(TEXT("Right-drag endpoint is outside UI overlays"),MapPointAvailable(Shell,Fleet,End)))return false;
        auto* Map=MapService(Fleet);const FVector Focus=Map->ViewFocus;const float Yaw=Map->ViewYaw,Pitch=Map->View3DPitch;
        auto* Registry=Fleet->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();const auto Selection=Registry->GetMultiSelectedDrones();
        const auto& Geometry=Fleet->GetCachedGeometry();
        Map->BeginPointer(EKeys::RightMouseButton,Geometry.LocalToAbsolute(Local),true,false);
        Map->MovePointer(Geometry.LocalToAbsolute(End),MapPointAvailable(Shell,Fleet,End));
        Map->EndPointer(EKeys::RightMouseButton,Geometry.LocalToAbsolute(End),MapPointAvailable(Shell,Fleet,End));
        if(View3D)Test->TestTrue(TEXT("Right drag changes 3D orbit targets"),!FMath::IsNearlyEqual(Map->ViewYaw,Yaw) || !FMath::IsNearlyEqual(Map->View3DPitch,Pitch));
        else Test->TestFalse(TEXT("Right drag changes 2D pan target"),Map->ViewFocus.Equals(Focus,.01));
        Test->TestFalse(TEXT("Right drag never stages a destination"),Fleet->bHasTarget);
        Test->TestFalse(TEXT("Right drag never submits execution"),Fleet->bPending);
        // Reverse the gesture before the next rendered frame, preserving the
        // workflow's settled camera without assigning presentation fields.
        Map->BeginPointer(EKeys::RightMouseButton,Geometry.LocalToAbsolute(End),true,false);
        Map->MovePointer(Geometry.LocalToAbsolute(Local),MapPointAvailable(Shell,Fleet,Local));
        Map->EndPointer(EKeys::RightMouseButton,Geometry.LocalToAbsolute(Local),MapPointAvailable(Shell,Fleet,Local));
        Test->TestTrue(TEXT("Inverse camera gesture restores pan target"),Map->ViewFocus.Equals(Focus,.01));
        Test->TestTrue(TEXT("Inverse camera gesture restores orbit target"),FMath::IsNearlyEqual(Map->ViewYaw,Yaw,.01f) && FMath::IsNearlyEqual(Map->View3DPitch,Pitch,.01f));
        if(!Test->TestTrue(TEXT("Map has its native input processor for Escape cancellation"),Map->PointerInput.IsValid()))return false;
        const auto ModeBeforeEscape=Fleet->GetMode();
        auto* Sync=Fleet->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
        const int ExecutionsBeforeEscape=Object(Sync->GetPlans(),TEXT("executions"))->Values.Num();
        Map->BeginPointer(EKeys::RightMouseButton,Geometry.LocalToAbsolute(Local),true,false);
        Test->TestTrue(TEXT("Escape consumes an armed right-button gesture"),Map->PointerInput->HandleKeyDownEvent(
            FSlateApplication::Get(),FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,0)));
        Test->TestFalse(TEXT("Escape releases the map service pointer owner"),Map->GestureButton.IsValid());
        Map->EndPointer(EKeys::RightMouseButton,Geometry.LocalToAbsolute(Local),MapPointAvailable(Shell,Fleet,Local));
        Test->TestFalse(TEXT("Right release after Escape cannot recreate a destination preview"),Fleet->bHasTarget);
        Test->TestFalse(TEXT("Right release after Escape cannot submit an execution"),Fleet->bPending);
        Test->TestEqual(TEXT("Escape and delayed right release leave execution count unchanged"),Object(Sync->GetPlans(),TEXT("executions"))->Values.Num(),ExecutionsBeforeEscape);
        Test->TestTrue(TEXT("Escape and delayed right release preserve accepted selection"),Registry->GetMultiSelectedDrones()==Selection && !Fleet->bSelectionPending);
        if(ModeBeforeEscape!=EMapFleetMode::Browse && Fleet->GetMode()!=ModeBeforeEscape)
            if(!ToolbarAction(Shell,ModeBeforeEscape==EMapFleetMode::BoxSelect?TEXT("box_select"):TEXT("group_move")))return false;
        Map->SetRouteEditCameraLocked(true);
        const bool Clicked=RightClick(Shell,Fleet,Local);
        Test->TestFalse(TEXT("Route edit lock prevents right-click fleet preview"),Fleet->bHasTarget);
        Test->TestFalse(TEXT("Route edit lock prevents right-click execution submission"),Fleet->bPending);
        Test->TestTrue(TEXT("Camera gestures and edit-locked right click preserve accepted selection"),Registry->GetMultiSelectedDrones()==Selection);
        Map->SetRouteEditCameraLocked(false);return Clicked;
    }
    bool ReselectByRenderedBox(UMapShellWidget* Shell,UMapFleetInteractionWidget* Fleet,const TArray<int32>& Wanted) {
        FVector2D Min(DBL_MAX,DBL_MAX),Max(-DBL_MAX,-DBL_MAX);
        for(int Id:Wanted){FVector2D P;if(!Test->TestTrue(TEXT("Requested moved UAV projects for reselection"),Fleet->Project(Id,P)))return false;
            Min.X=FMath::Min(Min.X,P.X);Min.Y=FMath::Min(Min.Y,P.Y);Max.X=FMath::Max(Max.X,P.X);Max.Y=FMath::Max(Max.Y,P.Y);}
        FVector2D Begin=FVector2D::ZeroVector,End=FVector2D::ZeroVector;bool Found=false;
        // The four actual Mock locations may be close together. Use a thin
        // rectangle on either axis, but never relocate aircraft or fake hits.
        for(const FVector2D Half:TArray<FVector2D>{FVector2D(8,.25),FVector2D(.25,8),FVector2D(12,.5),FVector2D(.5,12),FVector2D(8,8)}) {
            const FVector2D A=Min-Half,B=Max+Half;bool Exact=true;
            for(const auto& D:Fleet->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetFriendlyDroneDescriptors()){
                FVector2D P;if(Fleet->Project(D.DroneId,P) && FleetSelection::Contains(A,B,P)!=Wanted.Contains(D.DroneId)){Exact=false;break;}
            }
            if(!Exact || !MapPointAvailable(Shell,Fleet,A) || !MapPointAvailable(Shell,Fleet,B))continue;
            bool Clear=true;for(int I=1;I<8;++I)Clear&=MapPointAvailable(Shell,Fleet,FMath::Lerp(A,B,I/8.));
            if(Clear){Begin=A;End=B;Found=true;break;}
        }
        if(!Test->TestTrue(TEXT("Post-move box endpoints and drag path reach the rendered map, not an overlay"),Found)){Capture(TEXT("reselection-hit-path-failure"));return false;}
        const auto& Geometry=Fleet->GetCachedGeometry();
        if(!Test->TestTrue(TEXT("Post-move group-move mode accepts drag press"),Fleet->BeginPointer(Geometry.LocalToAbsolute(Begin),false,false)))return false;
        const FVector2D Middle=(Begin+End)*.5;
        Fleet->MovePointer(Geometry.LocalToAbsolute(Middle),MapPointAvailable(Shell,Fleet,Middle));
        Fleet->EndPointer(Geometry.LocalToAbsolute(End),MapPointAvailable(Shell,Fleet,End));
        // This is the user's exact regression: group_move, then drag. It must
        // issue a real selection request, rather than silently ignoring the drag.
        const bool Requested=Test->TestTrue(TEXT("Left drag after the toolbar action requests UAV reselection"),Fleet->bSelectionPending);
        Test->TestEqual(TEXT("Selection drag leaves destination-picking mode"),Fleet->GetMode(),EMapFleetMode::BoxSelect);
        Test->TestFalse(TEXT("Selection drag never stages a move destination"),Fleet->bHasTarget);
        if(!Requested)Capture(TEXT("reselection-no-request-failure"));
        return Requested;
    }
    bool UpdateReselection(UMapShellWidget* Shell,UMapFleetInteractionWidget* Fleet,UOperationalContextSubsystem* Sync,UDroneRegistrySubsystem* Registry) {
        Shell->Refresh();const auto All=Object(Sync->GetPlans(),TEXT("executions"));
        auto SelectionIs=[&](const TArray<int32>& Wanted){
            return !Fleet->bSelectionPending && Registry->GetMultiSelectedDrones()==Wanted && Registry->GetPrimarySelectedDrone()==Wanted[0];
        };
        auto CheckVideo=[&](){
            FString Video;FJsonSerializer::Serialize(Sync->GetVideoView().ToSharedRef(),TJsonWriterFactory<>::Create(&Video));
            Test->TestEqual(TEXT("All post-move selection and execution gestures preserve exact VideoView"),Video,VideoBefore);
        };
        auto ArmToolbar=[&](const TCHAR* Name){
            Fleet->Buttons[TEXT("cancel")]->OnAction.Broadcast(TEXT("cancel"),0);
            if(!ToolbarAction(Shell,Name))return false;
            Next=FPlatformTime::Seconds()+.2;return true;
        };
        auto GroupState=[&](const FString& Id,const TCHAR* State){
            int Count=0;bool Match=true;
            for(const auto& Item:All->Values){const auto E=Item.Value->AsObject();if(Field(E,TEXT("group_id"))!=Id)continue;
                ++Count;const FString Actual=Field(E,TEXT("state"));
                if(Actual==TEXT("FAILED")){Test->AddError(TEXT("Reselection execution failed before expected terminal state: ")+Id);return -1;}
                Match&=Actual==State;
            }return Count>0 && Match?1:0;
        };
        auto ConfirmFreshSingle=[&](int Id,int ExpectedBefore){
            FVector2D Anchor;if(!Test->TestTrue(TEXT("Reselected UAV projects before next destination"),Fleet->Project(Id,Anchor)))return false;
            Fleet->Buttons[TEXT("cancel")]->OnAction.Broadcast(TEXT("cancel"),0);
            if(!RightClick(Shell,Fleet,Anchor+FVector2D(-100,80)))return false;
            if(!Test->TestTrue(TEXT("Right click stages a fresh single-UAV preview"),Fleet->bHasTarget && Fleet->TargetMembers==TArray<int32>{Id}))return false;
            Test->TestEqual(TEXT("New right-click preview does not create an execution"),All->Values.Num(),ExpectedBefore);
            Fleet->Buttons[TEXT("confirm")]->OnAction.Broadcast(TEXT("confirm"),0);
            if(!Test->TestTrue(TEXT("Explicit second-move confirmation waits for Backend ACK"),Fleet->bPending))return false;
            return true;
        };
        if(Step==10){
            FVector2D Blank=FVector2D::ZeroVector;
            if(!Test->TestTrue(TEXT("Executing fleet has a rendered blank map point"),BlankMapPoint(Shell,Fleet,Blank)))return true;
            Test->TestFalse(TEXT("Executing fleet remains reserved for movement"),Fleet->UnavailableReason().IsEmpty());
            const auto PreviousSelection=Registry->GetMultiSelectedDrones();
            if(!RightClick(Shell,Fleet,Blank))return true;
            Test->TestEqual(TEXT("Rejected occupied right click keeps Browse selection available"),Fleet->GetMode(),EMapFleetMode::Browse);
            Test->TestFalse(TEXT("Occupied right click does not create a preview"),Fleet->bHasTarget);
            Test->TestTrue(TEXT("Occupied right click does not retarget selection"),Registry->GetMultiSelectedDrones()==PreviousSelection && !Fleet->bSelectionPending);
            if(!ArmToolbar(TEXT("group_move")))return true;
            Test->TestEqual(TEXT("Actual group move toolbar delegate arms destination mode"),Fleet->GetMode(),EMapFleetMode::GroupMove);
            ++Step;return false;
        }
        if(Step==11){
            if(!ReselectByRenderedBox(Shell,Fleet,{1}))return true;
            Test->TestEqual(TEXT("Selection request does not optimistically replace the accepted four UAVs"),Registry->GetMultiSelectedDrones().Num(),4);
            ++Step;Next=FPlatformTime::Seconds()+.1;return false;
        }
        if(Fleet->bSelectionPending)return false;
        if(Step==12){
            if(!Test->TestTrue(TEXT("Backend ACK applies the executing fleet subset"),SelectionIs({1})))return true;
            CheckVideo();Test->TestEqual(TEXT("Selection creates no second execution"),All->Values.Num(),BeforeExecutions+4);
            Capture(TEXT("reselection-executing-subset"));
            if(!ArmToolbar(TEXT("box_select")))return true;++Step;return false;
        }
        if(Step==13){if(!ReselectByRenderedBox(Shell,Fleet,{1,2,3,4}))return true;++Step;return false;}
        if(Step==14){
            if(!Test->TestTrue(TEXT("Actual box-select toolbar restores group membership after ACK"),SelectionIs({1,2,3,4})))return true;
            Fleet->Buttons[TEXT("abort")]->OnAction.Broadcast(TEXT("abort"),0);
            if(!Test->TestTrue(TEXT("Group abort requires its visible confirmation"),Fleet->bAbortConfirm))return true;
            Fleet->Buttons[TEXT("abort_confirm")]->OnAction.Broadcast(TEXT("abort_confirm"),0);++Step;return false;
        }
        if(Step==15){
            const int State=GroupState(Group,TEXT("ABORTED"));if(State<0)return true;if(!State)return false;
            if(!ArmToolbar(TEXT("box_select")))return true;++Step;return false;
        }
        if(Step==16){if(!ReselectByRenderedBox(Shell,Fleet,{1}))return true;++Step;return false;}
        if(Step==17){
            if(!Test->TestTrue(TEXT("ABORTED fleet can be reselected to a single UAV after ACK"),SelectionIs({1})))return true;
            CheckVideo();if(!ConfirmFreshSingle(1,BeforeExecutions+4))return true;++Step;return false;
        }
        if(Step==18 || Step==22){
            if(Fleet->MessageKey.StartsWith(TEXT("Errors."))){Test->AddError(TEXT("Repeated move refused: ")+Fleet->MessageKey+TEXT(" / ")+Fleet->ErrorDetails.ToString());return true;}
            const int Id=Step==18?1:2;TSharedPtr<FJsonObject> Created;
            for(const auto& Item:All->Values){const auto E=Item.Value->AsObject();const auto CurrentGroup=Field(E,TEXT("group_id"));
                if(Field(E,TEXT("uav_id"))==FString::Printf(TEXT("UAV-%02d"),Id) && CurrentGroup!=Group && CurrentGroup!=SecondGroup){Created=E;break;}}
            if(!Created)return false;
            FString& AcceptedGroup=Step==18?SecondGroup:ThirdGroup;AcceptedGroup=Field(Created,TEXT("group_id"));
            int Count=0;for(const auto& Item:All->Values)if(Field(Item.Value->AsObject(),TEXT("group_id"))==AcceptedGroup)++Count;
            Test->TestEqual(TEXT("Each repeated single move creates exactly one execution after ACK"),Count,1);
            Test->TestEqual(TEXT("Only one new execution was added by the explicit confirmation"),All->Values.Num(),BeforeExecutions+(Step==18?5:6));
            Test->TestFalse(TEXT("Repeated move ACK retires preview"),Fleet->bHasTarget);
            Test->TestEqual(TEXT("Repeated move ACK restores Browse"),Fleet->GetMode(),EMapFleetMode::Browse);
            CheckVideo();++Step;return false;
        }
        if(Step==19){
            const int State=GroupState(SecondGroup,TEXT("COMPLETED"));if(State<0)return true;if(!State)return false;
            Capture(TEXT("reselection-second-move-completed"));
            if(!ArmToolbar(TEXT("group_move")))return true;++Step;return false;
        }
        if(Step==20){if(!ReselectByRenderedBox(Shell,Fleet,{2}))return true;++Step;return false;}
        if(Step==21){
            if(!Test->TestTrue(TEXT("COMPLETED fleet allows group-move plus drag to select another UAV after ACK"),SelectionIs({2})))return true;
            CheckVideo();if(!ConfirmFreshSingle(2,BeforeExecutions+5))return true;++Step;return false;
        }
        if(Step==23){
            const int Completed=GroupState(ThirdGroup,TEXT("COMPLETED"));if(Completed<0)return true;if(!Completed)return false;
            CheckVideo();Capture(TEXT("reselection-completed-repeat"));
            Test->AddInfo(TEXT("Reselection covered EXECUTING, ABORTED and COMPLETED snapshots with real toolbar delegates, owned-window Slate hit paths and ACKs; not OS-native mouse evidence."));
            return true;
        }
        return false;
    }
    void Capture(const TCHAR* Name) {
        if(MemberCount!=1 && !bReselection && !View3D)return;
        if(CaptureDirectory.IsEmpty()) {
            CaptureDirectory=FPaths::ProjectSavedDir()/TEXT("MapInteractionQA")/FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"));
            IFileManager::Get().MakeDirectory(*CaptureDirectory,true);
            Test->AddInfo(TEXT("Rendered component evidence (not native mouse verification): ")+CaptureDirectory);
        }
        FSlateApplication::Get().InvalidateAllWidgets(false);
        TArray<FColor> Pixels;FIntVector Size=FIntVector::ZeroValue;
        auto Viewport=GEngine->GameViewport?GEngine->GameViewport->GetGameViewportWidget():nullptr;
        if(!Viewport || !FSlateApplication::Get().TakeScreenshot(Viewport.ToSharedRef(),Pixels,Size)) {
            Test->AddError(TEXT("Rendered Map workflow screenshot unavailable"));return;
        }
        TArray64<uint8> Bytes;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Bytes);
        Test->TestTrue(TEXT("Map workflow screenshot persisted"),FFileHelper::SaveArrayToFile(Bytes,*(CaptureDirectory/(FString(Name)+TEXT(".png")))));
        Test->AddInfo(FString::Printf(TEXT("Rendered Map screenshot %s %dx%d"),Name,Size.X,Size.Y));
    }
public:
    explicit FFleetMapWorkflow(FAutomationTestBase* In,bool In3D=false,int InMembers=4,bool InReselection=false):Test(In),MemberCount(InMembers),View3D(In3D),bReselection(InReselection){}
    bool Update() override {
        if(FPlatformTime::Seconds()-Began>(bReselection?180.:90.)){Test->AddError(FString::Printf(TEXT("Fleet workflow timeout, step %d"),Step));return true;}
        if(FPlatformTime::Seconds()<Next)return false;
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();if(!W)return false;
        auto* PC=Cast<ADroneOpsPlayerController>(W->GetFirstPlayerController());if(!PC || !PC->GetCommandScreenManager())return false;
        auto* Manager=PC->GetCommandScreenManager();auto* Shell=Manager->GetMapShell();auto* F=Shell?Shell->GetFleetInteraction():nullptr;
        auto* S=W->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();auto* R=W->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
        if(!F || !S->IsHydrated() || F->bPending)return false;F->Refresh();
        auto All=Object(S->GetPlans(),TEXT("executions"));
        if(bReselection && Step>=10)return UpdateReselection(Shell,F,S,R);
        if(Step==0) {
            // Exercise the requested workflow explicitly, independently of the
            // production city's configurable startup mode. Settle real frames
            // before recording the camera used by selection invariance checks.
            auto* Map=Manager->GetMapService();
            if(!bRequestedMode){Map->SetMapMode(View3D?ECommandMapMode::Map3D:ECommandMapMode::Map2D);bRequestedMode=true;return false;}
            if(FMath::Abs(Map->GetMapCamera()->GetActorRotation().Pitch+(View3D?55.f:89.9f))>.05f)return false;
            const FVector Camera=Map->GetMapCamera()->GetActorLocation();StableCameraFrames=Camera.Equals(LastCameraSample,.001)?StableCameraFrames+1:0;LastCameraSample=Camera;if(StableCameraFrames<6)return false;
            if(View3D)Test->TestEqual(TEXT("real map service is in 3D"),Map->GetMapMode(),ECommandMapMode::Map3D);
            else Test->TestEqual(TEXT("real map service explicitly entered 2D"),Map->GetMapMode(),ECommandMapMode::Map2D);
            if(R->GetFriendlyDroneDescriptors().Num()!=4 || F->GetCachedGeometry().GetLocalSize().X<100)return false;
            FVector2D Min(DBL_MAX,DBL_MAX),Max(-DBL_MAX,-DBL_MAX);
            for(int Id=1;Id<=MemberCount;++Id){FVector2D P;if(!F->Project(Id,P))return false;Min.X=FMath::Min(Min.X,P.X);Min.Y=FMath::Min(Min.Y,P.Y);Max.X=FMath::Max(Max.X,P.X);Max.Y=FMath::Max(Max.Y,P.Y);}
            FJsonSerializer::Serialize(S->GetVideoView().ToSharedRef(),TJsonWriterFactory<>::Create(&VideoBefore));
            CameraBefore=Manager->GetMapService()->GetMapCamera()->GetActorLocation();
            F->SetMode(EMapFleetMode::BoxSelect);
            if(MemberCount==1)S->SetLocalSelection(1,{1});
            else {const auto& G=F->GetCachedGeometry();Test->TestTrue(TEXT("native event entry accepts box gesture"),F->BeginPointer(G.LocalToAbsolute(Max+FVector2D(10,10)),false,false));
                F->EndPointer(G.LocalToAbsolute(Min-FVector2D(10,10)),true);}
            TargetLocal=(Min+Max)*.5+FVector2D(-240,140);
            ++Step;return false;
        }
        if(Step==1) {
            if(R->GetMultiSelectedDrones().Num()!=MemberCount || (MemberCount==1 && R->GetPrimarySelectedDrone()!=1) || F->bSelectionPending)return false;
            Test->TestTrue(TEXT("box selection leaves map camera unchanged"),Manager->GetMapService()->GetMapCamera()->GetActorLocation().Equals(CameraBefore,.01));
            FString Video;FJsonSerializer::Serialize(S->GetVideoView().ToSharedRef(),TJsonWriterFactory<>::Create(&Video));Test->TestEqual(TEXT("selection never retargets Video"),Video,VideoBefore);
            Capture(TEXT("01-selected-fixed-size-symbols"));
            if(!RightDragAndEditLockChecks(Shell,F))return true;
            BeforeExecutions=All->Values.Num();Test->TestTrue(TEXT("selection ACK enables guided next step"),F->Buttons[TEXT("next")]->GetIsEnabled());F->Action(TEXT("next"),0);
            Test->TestEqual(TEXT("guided next step arms destination picking"),F->GetMode(),EMapFleetMode::GroupMove);
            // This regression asserts exact ellipsoid heights. Select that user
            // reference explicitly now that the normal 3D default is AGL.
            if(View3D){F->Action(TEXT("height_mode"),0);Test->TestFalse(TEXT("advanced ellipsoid reference is selectable"),F->bHeightAGL);}
            if(!RightClick(Shell,F,TargetLocal))return true;
            Test->TestFalse(TEXT("Right-click preview never submits before explicit confirmation"),F->bPending);
            Test->TestEqual(TEXT("Right-click preview leaves Backend execution count unchanged"),All->Values.Num(),BeforeExecutions);
            if(View3D){const FVector Horizontal=F->TargetGeographic;const double StartHeight=Horizontal.Z;
                for(const auto& Bad:TArray<FString>{TEXT("nan"),TEXT("1e308"),FString::SanitizeFloat(StartHeight+10001.)}){
                    F->TargetHeightInput->SetText(User(Bad));F->TargetHeightInput->OnTextChanged.Broadcast(F->TargetHeightInput->GetText());F->Action(TEXT("confirm"),0);
                    Test->TestFalse(TEXT("invalid 3D height disables execution confirmation"),F->Buttons[TEXT("confirm")]->GetIsEnabled());Test->TestFalse(TEXT("invalid height never submits Backend execution"),F->bPending);
                    Test->TestEqual(TEXT("invalid height retains last valid target"),F->TargetGeographic.Z,StartHeight);
                }
                Test->TestTrue(TEXT("slider remains available to recover invalid text"),F->HeightSlider->GetIsEnabled());F->HeightSliderChanged(0.f);
                Test->TestTrue(TEXT("valid slider gesture restores confirmation"),F->Buttons[TEXT("confirm")]->GetIsEnabled());
                ExpectedHeight=StartHeight+50.;F->TargetHeightInput->SetText(User(FString::SanitizeFloat(ExpectedHeight)));F->TargetHeightInput->OnTextChanged.Broadcast(F->TargetHeightInput->GetText());
                Test->TestEqual(TEXT("3D height edits preserve longitude"),F->TargetGeographic.X,Horizontal.X);Test->TestEqual(TEXT("3D height edits preserve latitude"),F->TargetGeographic.Y,Horizontal.Y);
                Test->TestEqual(TEXT("3D target takes the requested ellipsoid height"),F->TargetGeographic.Z,ExpectedHeight);
                F->Action(TEXT("height_up"),0);Test->TestEqual(TEXT("raise by ten metres"),F->TargetGeographic.Z,ExpectedHeight+10.);
                F->Action(TEXT("height_down"),0);Test->TestEqual(TEXT("lower by ten metres"),F->TargetGeographic.Z,ExpectedHeight);
                F->HeightSliderChanged(25.f);Test->TestEqual(TEXT("relative height slider uses current anchor altitude"),F->TargetGeographic.Z,StartHeight+25.);
                Test->TestEqual(TEXT("slider leaves horizontal target unchanged"),F->TargetGeographic.X,Horizontal.X);
                F->Action(TEXT("reselect"),0);Test->TestFalse(TEXT("choose again clears only the target"),F->bHasTarget);double Retained=0;F->TargetHeight(Retained);
                Test->TestEqual(TEXT("choose again keeps height preference"),Retained,StartHeight+25.);Test->TestEqual(TEXT("choose again retires old preview actors"),F->PreviewRoutes.Num(),0);
                if(!RightClick(Shell,F,TargetLocal))return true;F->Action(TEXT("height_current"),0);Test->TestEqual(TEXT("current height resets the preview"),F->TargetGeographic.Z,StartHeight);
                F->HeightSliderChanged(50.f);const auto BeforeFrame=F->TargetGeographic;F->Action(TEXT("fit_preview"),0);
                Test->TestTrue(TEXT("explicit preview framing preserves geographic destination"),F->TargetGeographic.Equals(BeforeFrame,.000001));
                Test->TestEqual(TEXT("slider/reselect/framing never create execution"),All->Values.Num(),BeforeExecutions);
                Manager->GetMapService()->Enter2DMode();F->Refresh();Test->TestFalse(TEXT("switching map mode invalidates pending 3D target"),F->bHasTarget);
                Manager->GetMapService()->Enter3DMode();F->Refresh();Test->TestFalse(TEXT("switching back never resurrects old target"),F->bHasTarget);
                // Camera settles before the next projection; verify height and
                // snapshot integration after restaging from a stable 3D view.
                F->CancelMode();Step=5;return false;
            }
            Test->TestTrue(TEXT("target click creates preview"),F->bHasTarget);Test->TestEqual(TEXT("one Route V2 preview per selected UAV"),F->PreviewRoutes.Num(),MemberCount);
            for(int Id=1;Id<=MemberCount;++Id){const auto* Path=F->PreviewRoutes.Find(FString::FromInt(Id));const auto* Display=R->GetReceiverActor(Id);
                Test->TestTrue(TEXT("preview begins at visible acknowledged UAV position"),Path && Display && !Path->Waypoints.IsEmpty() && Path->Waypoints[0].Location.Equals(Display->GetActorLocation(),.1));
                if(Path && !Path->GetSegmentVisuals().IsEmpty())Test->TestEqual(TEXT("preview speed flow matches Backend execution speed"),Path->GetSegmentVisuals()[0].EffectiveSpeed,float(Object(S->GetPlans(),TEXT("mock_execution"))->GetNumberField(TEXT("default_speed_mps"))));}
            Test->TestEqual(TEXT("preview never creates Backend execution"),All->Values.Num(),BeforeExecutions);
            Test->TestTrue(TEXT("Esc cancels armed target"),F->CancelMode());Test->TestEqual(TEXT("cancel destroys preview actors"),F->PreviewRoutes.Num(),0);
            F->SetMode(MemberCount==1?EMapFleetMode::Browse:EMapFleetMode::GroupMove);if(!RightClick(Shell,F,TargetLocal))return true;
            Test->TestTrue(TEXT("selected aircraft plus empty map right click stages a destination"),F->bHasTarget);
            if(MemberCount==1){Next=FPlatformTime::Seconds()+.35;Step=6;return false;}
            F->Action(TEXT("confirm"),0);++Step;return false;
        }
        if(Step==2) {
            if(F->MessageKey.StartsWith(TEXT("Errors."))) {
                Test->AddError(TEXT("Map move refused: ")+F->MessageKey+TEXT(" / ")+F->ErrorDetails.ToString());return true;
            }
            auto Latest=ExecutionUI::Latest(S->GetPlans(),TEXT(""),true);if(!Latest)return false;
            if(Field(Latest,TEXT("control_kind"))!=TEXT("GROUP_MOVE")){Test->AddError(TEXT("Wrong execution owner"));return true;}
            Group=Field(Latest,TEXT("group_id"));if(Field(Latest,TEXT("state"))!=TEXT("EXECUTING"))return false;
            int Count=0;for(const auto& Item:All->Values)if(Field(Item.Value->AsObject(),TEXT("group_id"))==Group)++Count;
            Test->TestEqual(TEXT("Map role creates exactly the confirmed number of executions"),Count,MemberCount);Test->TestFalse(TEXT("ACK retires preview ownership"),F->bHasTarget);
            Test->TestEqual(TEXT("ACK switches card from editing to execution controls"),F->GetMode(),EMapFleetMode::Browse);
            Capture(TEXT("03-accepted-single-move"));
            if(bReselection){Step=10;return false;}
            if(View3D)for(const auto& Item:All->Values){const auto E=Item.Value->AsObject();if(Field(E,TEXT("group_id"))==Group){const auto Route=Object(E,TEXT("route_snapshot"));
                const auto End=Route->GetArrayField(TEXT("waypoints")).Last()->AsObject();Test->TestEqual(TEXT("Backend immutable trajectory receives requested 3D height"),End->GetNumberField(TEXT("altitude")),ExpectedHeight);}}
            F->Action(TEXT("pause"),0);++Step;return false;
        }
        if(Step==3) {
            for(const auto& Item:All->Values)if(Field(Item.Value->AsObject(),TEXT("group_id"))==Group && Field(Item.Value->AsObject(),TEXT("state"))!=TEXT("PAUSED"))return false;
            Test->TestFalse(TEXT("occupied UAVs cannot be moved again"),F->UnavailableReason().IsEmpty());F->Action(TEXT("abort"),0);
            Test->TestTrue(TEXT("stop group first opens inline confirmation"),F->bAbortConfirm);Test->TestFalse(TEXT("opening stop confirmation never submits"),F->bPending);
            F->Action(TEXT("abort_cancel"),0);Test->TestFalse(TEXT("keep task dismisses stop confirmation"),F->bAbortConfirm);Test->TestFalse(TEXT("keeping task never submits"),F->bPending);
            F->Action(TEXT("abort"),0);F->Action(TEXT("abort_confirm"),0);++Step;return false;
        }
        if(Step==4) {
            for(const auto& Item:All->Values)if(Field(Item.Value->AsObject(),TEXT("group_id"))==Group && Field(Item.Value->AsObject(),TEXT("state"))!=TEXT("ABORTED"))return false;
            Test->TestTrue(TEXT("abort releases entire selection"),F->UnavailableReason().IsEmpty());F->Action(TEXT("again"),0);
            FVector Accepted;double Default=0;F->Telemetry(R->GetPrimarySelectedDrone(),Accepted);F->TargetHeight(Default);
            Test->TestEqual(TEXT("move again starts at last acknowledged height"),Default,Accepted.Z);Test->TestFalse(TEXT("move again requires a fresh destination"),F->bHasTarget);F->SetMode(EMapFleetMode::Browse);
            auto* Map=Manager->GetMapService();Map->SetRouteEditCameraLocked(true);F->SetMode(EMapFleetMode::BoxSelect);
            Test->TestEqual(TEXT("route edit camera lock blocks fleet mode"),F->GetMode(),EMapFleetMode::Browse);Map->SetRouteEditCameraLocked(false);return true;
        }
        if(Step==5){if(FMath::Abs(Manager->GetMapService()->GetMapCamera()->GetActorRotation().Pitch+55.f)>.05f)return false;
            F->SetMode(EMapFleetMode::GroupMove);if(!RightClick(Shell,F,TargetLocal))return true;
            if(!Test->TestTrue(TEXT("3D target restaged after mode change"),F->bHasTarget))return true;
            F->TargetHeightInput->SetText(User(FString::SanitizeFloat(ExpectedHeight)));F->TargetHeightInput->OnTextChanged.Broadcast(F->TargetHeightInput->GetText());Test->TestEqual(TEXT("3D preview has four world trajectories"),F->PreviewRoutes.Num(),4);
            for(int Id=1;Id<=4;++Id){const auto* Path=F->PreviewRoutes.Find(FString::FromInt(Id));Test->TestTrue(TEXT("3D preview rises from each actual aircraft"),Path && Path->Waypoints.Num()==2 && Path->Waypoints[1].Location.Z>Path->Waypoints[0].Location.Z+1000);}
            Test->TestEqual(TEXT("3D height preview has no execution side effects"),All->Values.Num(),BeforeExecutions);Next=FPlatformTime::Seconds()+.35;Step=6;return false;
        }
        if(Step==6){CheckPreviewButtons(F);Capture(View3D?TEXT("02-3d-move-preview"):TEXT("02-single-point-preview"));F->Action(TEXT("confirm"),0);Test->TestTrue(TEXT("explicit Map confirmation waits for Backend ACK"),F->bPending);Step=2;return false;}
        return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFleetWorkflowTest,"DroneOps.Fleet.Workflow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFleetWorkflowTest::RunTest(const FString&) {
    if(FParse::Param(FCommandLine::Get(),TEXT("NullRHI"))){AddError(TEXT("Fleet screen projection requires an RHI viewport; rerun without -NullRHI."));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FFleetMapWorkflow(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFleetWorkflow3DTest,"DroneOps.Fleet.ThreeDimensionalWorkflow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFleetWorkflow3DTest::RunTest(const FString&) {
    if(FParse::Param(FCommandLine::Get(),TEXT("NullRHI"))){AddError(TEXT("Fleet screen projection requires an RHI viewport; rerun without -NullRHI."));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FFleetMapWorkflow(this,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSingleMapMoveTest,"DroneOps.Fleet.SinglePointMove",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSingleMapMoveTest::RunTest(const FString&) {
    if(FParse::Param(FCommandLine::Get(),TEXT("NullRHI"))){AddError(TEXT("Fleet screen projection requires an RHI viewport; rerun without -NullRHI."));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FFleetMapWorkflow(this,false,1));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFleetReselectionTest,"DroneOps.Fleet.Reselection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFleetReselectionTest::RunTest(const FString&) {
    if(FParse::Param(FCommandLine::Get(),TEXT("NullRHI"))){AddError(TEXT("Fleet reselection requires rendered map and Slate hit geometry; rerun without -NullRHI."));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FFleetMapWorkflow(this,false,4,true));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
