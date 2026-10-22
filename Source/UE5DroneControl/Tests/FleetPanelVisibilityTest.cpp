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
    bool Select(UMapShellWidget* Shell,UMapFleetInteractionWidget* Fleet,int32 Id,bool Box)
    {
        FVector2D Point=FVector2D::ZeroVector;
        if(!Test->TestTrue(TEXT("Selection fixture has a real projected map position"),Fleet->Project(Id,Point)))return false;
        const FVector2D A=Point-(Box?FVector2D(3):FVector2D::ZeroVector),B=Point+(Box?FVector2D(3):FVector2D::ZeroVector);
        if(!Test->TestTrue(TEXT("Selection gesture endpoints reach the actual Slate viewport"),MapHit(Shell,Fleet,A) && MapHit(Shell,Fleet,B)))return false;
        const auto& G=Fleet->GetCachedGeometry();
        if(!Test->TestTrue(TEXT("Left selection entry accepts the gesture"),Fleet->BeginPointer(G.LocalToAbsolute(A),false,false)))return false;
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
            ExecutionsBefore=Executions->Values.Num();VideoBefore=Video(Sync);
            Fleet->SetMode(EMapFleetMode::Browse);
            if(!Test->TestTrue(TEXT("Initial empty selection request is sent to isolated Backend"),Sync->SetLocalSelection(0,{})))return true;
            ++Step;Next=FPlatformTime::Seconds()+1.5;return false;
        }
        if(Step==1)
        {
            if(Registry->GetPrimarySelectedDrone()!=0 || !Registry->GetMultiSelectedDrones().IsEmpty())return false;
            RefreshClosed(Fleet,TEXT("Initial browse card is closed"));
            if(!Select(Shell,Fleet,1,false))return true;++Step;return false;
        }
        if(Step==2)
        {
            if(Fleet->bSelectionPending || Registry->GetPrimarySelectedDrone()!=1 || Registry->GetMultiSelectedDrones()!=TArray<int32>{1})return false;
            FDroneTelemetrySnapshot Telemetry;
            if(Registry->GetTelemetry(1,Telemetry))Registry->UpdateTelemetry(1,Telemetry);
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
        if(Step==3)
        {
            if(Fleet->bSelectionPending || Registry->GetPrimarySelectedDrone()!=2 || Registry->GetMultiSelectedDrones()!=TArray<int32>{2})return false;
            RefreshClosed(Fleet,TEXT("A later box-selection ACK does not reopen the cancelled card"));
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
        if(Step==4)
        {
            Test->TestTrue(TEXT("Escape cancellation handler accepts an open preview"),Fleet->CancelMode());
            Test->TestTrue(TEXT("Escape clears provisional preview members"),Fleet->GetPreviewMembers().IsEmpty());
            RefreshClosed(Fleet,TEXT("Escape remains closed through repeated refreshes"));
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
        Test->TestEqual(TEXT("Selection and panel actions preserve explicit Video target"),Video(Sync),VideoBefore);
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
