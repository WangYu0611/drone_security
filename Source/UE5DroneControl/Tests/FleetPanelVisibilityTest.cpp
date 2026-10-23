#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Map/MapFleetInteractionWidget.h"
#include "Map/MapShellWidget.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandMapInteractionService.h"
#include "Command/CommandActionButton.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Shared/PlanWidgetSupport.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Serialization/JsonSerializer.h"
using namespace PlanUI;

// Real Map widget and isolated Backend selection ACKs. Calls the pointer entry
// points with actual rendered geometry; it does not move an OS mouse or fly UAVs.
class FFleetPanelVisibility : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int Step=0,OriginalPrimary=0,ExecutionsBefore=0;
    TArray<int32> OriginalSelection;
    FString VideoBefore;
    int64 NoDragVersion=-1;
    double Started=FPlatformTime::Seconds(),Next=0;
    static FString Video(UOperationalContextSubsystem* Sync)
    {
        FString Json;FJsonSerializer::Serialize(Sync->GetVideoView().ToSharedRef(),TJsonWriterFactory<>::Create(&Json));return Json;
    }
    static bool MapHit(UMapShellWidget* Shell,UMapFleetInteractionWidget* Fleet,const FVector2D& Local)
    {
        const auto Screen=Fleet->GetCachedGeometry().LocalToAbsolute(Local);
        if(!Shell->IsPointOverMap(Screen))return false;
        auto* Client=Fleet->GetWorld()->GetGameViewport();const auto View=Client?Client->GetGameViewportWidget():nullptr;
        const auto Window=Client?Client->GetWindow():nullptr;if(!View || !Window)return false;
        const TArray<TSharedRef<SWindow>> Windows{Window.ToSharedRef()};
        const auto Path=FSlateApplication::Get().LocateWindowUnderMouse(Screen,Windows);
        return Path.IsValid() && Path.Widgets.Last().Widget==View;
    }
    bool Toolbar(UMapShellWidget* Shell,const TCHAR* Name)
    {
        auto* Button=Cast<UCommandActionButton>(Shell->WidgetTree->FindWidget(FName(Name)));
        if(!Test->TestTrue(TEXT("Explicit toolbar action is available"),Button && Button->IsVisible() && Button->GetIsEnabled()))return false;
        Button->OnAction.Broadcast(FName(Name),0);return true;
    }
    void RefreshClosed(UMapFleetInteractionWidget* Fleet,const TCHAR* Assertion)
    {
        for(int I=0;I<12;++I)Fleet->Refresh();
        Test->TestEqual(Assertion,Fleet->Panel->GetVisibility(),ESlateVisibility::Collapsed);
    }
    bool UnarmedDrag(UMapShellWidget* Shell,UMapFleetInteractionWidget* Fleet,bool ReturnToStart)
    {
        auto* Sync=Fleet->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
        auto* Registry=Fleet->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
        const auto Mode=Fleet->GetMode();
        if(!Test->TestTrue(TEXT("Ordinary drag starts without box-select authorization"),Mode!=EMapFleetMode::BoxSelect))return false;
        const auto& Frame=Shell->GetMapViewportGeometry();const auto& Geometry=Fleet->GetCachedGeometry();
        FVector2D A=FVector2D::ZeroVector,B=FVector2D::ZeroVector;bool Found=false;
        for(const FVector2D Fraction:TArray<FVector2D>{{.3,.6},{.4,.7},{.2,.4},{.5,.8}}){
            A=Geometry.AbsoluteToLocal(Frame.LocalToAbsolute(Frame.GetLocalSize()*Fraction));B=A+FVector2D(24,12);
            if(MapHit(Shell,Fleet,A) && MapHit(Shell,Fleet,B)){Found=true;break;}
        }
        if(!Test->TestTrue(TEXT("Unarmed drag stays on actual map hit geometry"),Found))return false;
        const auto Selected=Registry->GetMultiSelectedDrones();const int Primary=Registry->GetPrimarySelectedDrone();
        const bool HasTarget=Fleet->bHasTarget;const auto Target=Fleet->TargetGeographic;const auto Members=Fleet->TargetMembers;
        const int Routes=Fleet->PreviewRoutes.Num();NoDragVersion=Sync->GetVersion();
        auto Check=[&](){
            Test->TestFalse(TEXT("Browse/group-move drag does not send a selection request"),Fleet->bSelectionPending || Sync->IsMapSelectionPending());
            Test->TestEqual(TEXT("Unarmed drag does not change context version"),Sync->GetVersion(),NoDragVersion);
            Test->TestTrue(TEXT("Unarmed drag preserves acknowledged primary and members"),Registry->GetPrimarySelectedDrone()==Primary && Registry->GetMultiSelectedDrones()==Selected);
            Test->TestEqual(TEXT("Unarmed drag never changes selection mode"),Fleet->GetMode(),Mode);
            Test->TestEqual(TEXT("Unarmed press and drag retain destination presence"),Fleet->bHasTarget,HasTarget);
            Test->TestTrue(TEXT("Unarmed drag retains destination and preview members"),Fleet->TargetGeographic.Equals(Target,.000001) && Fleet->TargetMembers==Members);
            Test->TestEqual(TEXT("Unarmed drag retains preview routes"),Fleet->PreviewRoutes.Num(),Routes);
            Test->TestFalse(TEXT("Unarmed drag never submits a move"),Fleet->bPending);
        };
        if(!Test->TestTrue(TEXT("Ordinary pointer press is accepted without arming a box"),Fleet->BeginPointer(Geometry.LocalToAbsolute(A),false,false)))return false;Check();
        Fleet->MovePointer(Geometry.LocalToAbsolute(B),true);Check();
        if(ReturnToStart)Fleet->MovePointer(Geometry.LocalToAbsolute(A),true);
        Fleet->EndPointer(Geometry.LocalToAbsolute(ReturnToStart?A:B),true);Check();
        Test->TestFalse(TEXT("Unarmed release retires the gesture"),Fleet->IsDragging());return true;
    }
    bool Select(UMapShellWidget* Shell,UMapFleetInteractionWidget* Fleet,int32 Id,bool Box,bool Shift=false,bool Control=false)
    {
        FVector2D Point=FVector2D::ZeroVector;
        if(!Test->TestTrue(TEXT("Selection fixture has a real projected map position"),Fleet->Project(Id,Point)))return false;
        FVector2D A=Point,B=Point;
        if(Box){
            if(!Test->TestEqual(TEXT("Only the real box-select toolbar authorizes rectangle selection"),Fleet->GetMode(),EMapFleetMode::BoxSelect))return false;
            bool Exact=false;
            for(const FVector2D Half:TArray<FVector2D>{{3,3},{8,.25},{.25,8},{12,.5},{.5,12}}){
                A=Point-Half;B=Point+Half;Exact=MapHit(Shell,Fleet,A) && MapHit(Shell,Fleet,B);
                for(const auto& Drone:Fleet->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetFriendlyDroneDescriptors()){
                    FVector2D Projected;if(Fleet->Project(Drone.DroneId,Projected) && FleetSelection::Contains(A,B,Projected)!=(Drone.DroneId==Id))Exact=false;
                }
                if(Exact)break;
            }
            if(!Test->TestTrue(TEXT("Box encloses exactly the requested real UAV"),Exact))return false;
        }
        if(!Test->TestTrue(TEXT("Selection gesture endpoints reach the actual Slate viewport"),MapHit(Shell,Fleet,A) && MapHit(Shell,Fleet,B)))return false;
        const auto& G=Fleet->GetCachedGeometry();
        if(!Test->TestTrue(TEXT("Left selection entry accepts the gesture"),Fleet->BeginPointer(G.LocalToAbsolute(A),Shift,Control)))return false;
        if(Box)Fleet->MovePointer(G.LocalToAbsolute((A+B)*.5),true);
        Fleet->EndPointer(G.LocalToAbsolute(B),true);
        Test->TestTrue(TEXT("Selection waits for Backend ACK"),Fleet->bSelectionPending);
        RefreshClosed(Fleet,TEXT("Ordinary left click or box selection never opens the move card"));return true;
    }
public:
    explicit FFleetPanelVisibility(FAutomationTestBase* InTest):Test(InTest){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>90.){Test->AddError(FString::Printf(TEXT("Fleet panel visibility timed out at step %d"),Step));return true;}
        if(FPlatformTime::Seconds()<Next)return false;
        UWorld* World=nullptr;for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE)World=Context.World();
        auto* PC=World?Cast<ADroneOpsPlayerController>(World->GetFirstPlayerController()):nullptr;
        auto* Manager=PC?PC->GetCommandScreenManager():nullptr;auto* Shell=Manager?Manager->GetMapShell():nullptr;
        auto* Fleet=Shell?Shell->GetFleetInteraction():nullptr;if(!Fleet)return false;
        auto* Sync=World->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
        auto* Registry=World->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
        if(!Sync->IsHydrated() || Fleet->bPending)return false;
        Fleet->Refresh();
        const auto Executions=Object(Sync->GetPlans(),TEXT("executions"));
        if(!Executions)return false;
        if(Step==0)
        {
            if(!Registry->IsDroneRegistered(1) || !Registry->IsDroneRegistered(2) || Fleet->GetCachedGeometry().GetLocalSize().X<100)return false;
            auto* Map=Manager->GetMapService();Map->Enter2DMode();if(!Map->FocusFleet())return false;
            OriginalPrimary=Registry->GetPrimarySelectedDrone();OriginalSelection=Registry->GetMultiSelectedDrones();
            ExecutionsBefore=Executions->Values.Num();Fleet->SetMode(EMapFleetMode::Browse);
            if(!Test->TestTrue(TEXT("Establish a different explicit video target before the Map click"),Sync->OpenVideo(TEXT("UAV-02"))))return true;
            if(!Test->TestTrue(TEXT("Initial empty selection request is sent to isolated Backend"),Sync->SetLocalSelection(0,{})))return true;
            ++Step;Next=FPlatformTime::Seconds()+1.5;return false;
        }
        if(Step==1)
        {
            if(Registry->GetPrimarySelectedDrone()!=0 || !Registry->GetMultiSelectedDrones().IsEmpty() || Field(Sync->GetVideoView(),TEXT("video_target_uav_id"))!=TEXT("UAV-02"))return false;
            RefreshClosed(Fleet,TEXT("Initial browse card is closed"));
            if(!UnarmedDrag(Shell,Fleet,false) || !UnarmedDrag(Shell,Fleet,true) || !Toolbar(Shell,TEXT("group_move")))return true;
            for(int I=0;I<12;++I)Fleet->Refresh();
            Test->TestEqual(TEXT("Empty selection keeps explicitly requested GroupMove mode"),Fleet->GetMode(),EMapFleetMode::GroupMove);
            Test->TestEqual(TEXT("Empty GroupMove shows the explicit click or box-button selection hint"),Fleet->Summary->GetText().ToString(),T(TEXT("FleetUX.SelectHint")).ToString());
            if(!UnarmedDrag(Shell,Fleet,false) || !UnarmedDrag(Shell,Fleet,true))return true;
            Fleet->Buttons[TEXT("cancel")]->OnAction.Broadcast(TEXT("cancel"),0);
            RefreshClosed(Fleet,TEXT("Empty movement card can be cancelled without choosing a UAV"));
            ++Step;Next=FPlatformTime::Seconds()+.3;return false;
        }
        if(Step==2)
        {
            Test->TestEqual(TEXT("Ordinary drags produce no later Backend context update"),Sync->GetVersion(),NoDragVersion);
            Test->TestTrue(TEXT("Ordinary drags leave empty selection acknowledged"),Registry->GetPrimarySelectedDrone()==0 && Registry->GetMultiSelectedDrones().IsEmpty());
            if(!Select(Shell,Fleet,1,false))return true;++Step;return false;
        }
        if(Step==3)
        {
            if(Fleet->bSelectionPending || Sync->IsMapSelectionPending() || Registry->GetPrimarySelectedDrone()!=1 || Registry->GetMultiSelectedDrones()!=TArray<int32>{1})return false;
            if(Field(Sync->GetVideoView(),TEXT("video_target_uav_id"))!=TEXT("UAV-01"))return false;
            Test->TestEqual(TEXT("Ordinary Map click synchronizes Video after its selection ACK"),Field(Sync->GetVideoView(),TEXT("video_target_uav_id")),FString(TEXT("UAV-01")));
            VideoBefore=Video(Sync);
            FDroneTelemetrySnapshot Telemetry;if(Registry->GetTelemetry(1,Telemetry))Registry->UpdateTelemetry(1,Telemetry);
            RefreshClosed(Fleet,TEXT("Selection ACK and telemetry refresh do not open the move card"));
            if(!Toolbar(Shell,TEXT("group_move")))return true;
            Test->TestEqual(TEXT("Explicit group-move action opens the card"),Fleet->Panel->GetVisibility(),ESlateVisibility::Visible);
            Test->TestFalse(TEXT("Opening the card does not invent a destination"),Fleet->bHasTarget);
            Fleet->Buttons[TEXT("cancel")]->OnAction.Broadcast(TEXT("cancel"),0);
            RefreshClosed(Fleet,TEXT("Cancel stays closed with the accepted selection retained"));
            Test->TestTrue(TEXT("Cancel preserves the singleton selection"),Registry->GetMultiSelectedDrones()==TArray<int32>{1});
            if(!Toolbar(Shell,TEXT("box_select")))return true;
            RefreshClosed(Fleet,TEXT("Explicit box-select toolbar does not open movement controls"));
            if(!Select(Shell,Fleet,2,true))return true;++Step;return false;
        }
        if(Step==4)
        {
            if(Fleet->bSelectionPending || Registry->GetPrimarySelectedDrone()!=2 || Registry->GetMultiSelectedDrones()!=TArray<int32>{2})return false;
            RefreshClosed(Fleet,TEXT("A later box-selection ACK does not reopen the cancelled card"));
            Test->TestEqual(TEXT("Rectangle selection preserves the explicit Video target"),Video(Sync),VideoBefore);
            if(!Select(Shell,Fleet,1,true,true,false))return true;++Step;return false;
        }
        if(Step==5)
        {
            if(Fleet->bSelectionPending)return false;
            if(!Test->TestTrue(TEXT("Shift box adds a UAV after Backend ACK and keeps primary"),Registry->GetPrimarySelectedDrone()==2 && Registry->GetMultiSelectedDrones()==TArray<int32>{1,2}))return true;
            Test->TestEqual(TEXT("Shift rectangle does not switch Video"),Video(Sync),VideoBefore);
            if(!Select(Shell,Fleet,1,true,false,true))return true;++Step;return false;
        }
        if(Step==6)
        {
            if(Fleet->bSelectionPending)return false;
            if(!Test->TestTrue(TEXT("Ctrl box toggles a UAV off after Backend ACK and keeps primary"),Registry->GetPrimarySelectedDrone()==2 && Registry->GetMultiSelectedDrones()==TArray<int32>{2}))return true;
            Test->TestEqual(TEXT("Ctrl rectangle does not switch Video"),Video(Sync),VideoBefore);
            if(!Toolbar(Shell,TEXT("box_select")))return true;
            Test->TestEqual(TEXT("Clicking the active box-select toolbar exits to Browse"),Fleet->GetMode(),EMapFleetMode::Browse);
            if(!UnarmedDrag(Shell,Fleet,false) || !UnarmedDrag(Shell,Fleet,true))return true;
            ++Step;Next=FPlatformTime::Seconds()+.3;return false;
        }
        if(Step==7)
        {
            Test->TestEqual(TEXT("Dragging after toolbar exit has no delayed selection side effect"),Sync->GetVersion(),NoDragVersion);
            FVector2D Anchor=FVector2D::ZeroVector;if(!Fleet->Project(2,Anchor))return false;
            FVector2D Target=FVector2D::ZeroVector;bool Found=false;
            for(const auto Offset:TArray<FVector2D>{{-100,80},{-100,-80},{100,80}})
                if(MapHit(Shell,Fleet,Anchor+Offset)){Target=Anchor+Offset;Found=true;break;}
            if(!Test->TestTrue(TEXT("Explicit right-click destination has unobstructed map geometry"),Found))return true;
            Test->TestTrue(TEXT("Right-click destination entry stages a singleton move"),Fleet->PickDestinationAtScreen(Fleet->GetCachedGeometry().LocalToAbsolute(Target)));
            Test->TestEqual(TEXT("Right-click intent opens the preview card"),Fleet->Panel->GetVisibility(),ESlateVisibility::Visible);
            Test->TestTrue(TEXT("Singleton uses the existing group preview members"),Fleet->GetPreviewMembers()==TArray<int32>{2});
            Test->TestTrue(TEXT("Active preview retains its explicit confirm action"),Fleet->Buttons[TEXT("confirm")]->GetIsEnabled() && Fleet->Buttons[TEXT("confirm")]->IsVisible());
            Test->TestFalse(TEXT("Preview does not submit movement"),Fleet->bPending);
            Test->TestEqual(TEXT("Preview leaves Backend execution count unchanged"),Executions->Values.Num(),ExecutionsBefore);
            ++Step;Next=FPlatformTime::Seconds()+.3;return false;
        }
        if(Step==8)
        {
            if(!UnarmedDrag(Shell,Fleet,false) || !UnarmedDrag(Shell,Fleet,true))return true;
            Test->TestTrue(TEXT("GroupMove preview remains confirmable after ordinary and round-trip drags"),Fleet->Buttons[TEXT("confirm")]->GetIsEnabled());
            ++Step;Next=FPlatformTime::Seconds()+.3;return false;
        }
        if(Step==9)
        {
            Test->TestEqual(TEXT("Movement-preview drags do not produce a later context ACK"),Sync->GetVersion(),NoDragVersion);
            Test->TestTrue(TEXT("Escape cancellation handler accepts an open preview"),Fleet->CancelMode());
            Test->TestTrue(TEXT("Escape clears provisional preview members"),Fleet->GetPreviewMembers().IsEmpty());
            RefreshClosed(Fleet,TEXT("Escape remains closed through repeated refreshes"));
            if(!UnarmedDrag(Shell,Fleet,false) || !UnarmedDrag(Shell,Fleet,true))return true;
            if(!Toolbar(Shell,TEXT("group_move")))return true;
            Test->TestEqual(TEXT("A new explicit action can reopen after Escape"),Fleet->Panel->GetVisibility(),ESlateVisibility::Visible);
            // The post-ACK execution view uses Browse. Verify Escape can close
            // that presentation state without requiring pointer-mode ownership.
            Fleet->Mode=EMapFleetMode::Browse;Fleet->Refresh();
            Test->TestTrue(TEXT("Escape can close an explicitly opened Browse card"),Fleet->CancelMode());
            RefreshClosed(Fleet,TEXT("Closing a Browse card also persists across refreshes"));
            if(!Test->TestTrue(TEXT("Restore original selection through Backend"),Sync->SetLocalSelection(OriginalPrimary,OriginalSelection)))return true;
            ++Step;return false;
        }
        if(Registry->GetPrimarySelectedDrone()!=OriginalPrimary || Registry->GetMultiSelectedDrones()!=OriginalSelection)return false;
        RefreshClosed(Fleet,TEXT("Restoring selection does not reopen movement controls"));
        Test->TestEqual(TEXT("Panel interactions never execute a movement"),Executions->Values.Num(),ExecutionsBefore);
        Test->TestEqual(TEXT("Rectangle selection and panel actions preserve the accepted Map-click Video target"),Video(Sync),VideoBefore);
        Test->AddInfo(TEXT("Real rendered Map widgets and isolated Backend selection ACKs; pointer entry-point automation, not native mouse or physical flight evidence."));return true;
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFleetPanelVisibilityTest,"DroneOps.Fleet.PanelVisibility",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFleetPanelVisibilityTest::RunTest(const FString&)
{
    if(FParse::Param(FCommandLine::Get(),TEXT("NullRHI"))){AddError(TEXT("Fleet panel visibility requires a rendered Slate viewport."));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FFleetPanelVisibility(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
