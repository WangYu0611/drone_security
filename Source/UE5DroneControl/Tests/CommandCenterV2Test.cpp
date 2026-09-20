#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Command/CommandShellWidget.h"
#include "Command/CommandTacticalMap.h"
#include "Command/OperationalLogWidget.h"
#include "Command/CommandAlertStore.h"
#include "Command/CommandScreenManager.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Shared/OperationalEventStore.h"
#include "Video/VideoShellWidget.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/DroneListWidget.h"
#include "Command/CommandAlarmPanel.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"

namespace {
TSharedPtr<FJsonObject> P2Snapshot(int64 Version,int32 Id) {
    auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("context_version"),Version);
    J->SetStringField(TEXT("active_uav_id"),FString::Printf(TEXT("UAV-%02d"),Id));J->SetStringField(TEXT("operation_mode"),TEXT("MONITOR"));
    for(const TCHAR* F:{TEXT("active_alert_id"),TEXT("active_mission_id"),TEXT("active_security_plan_id"),TEXT("active_area_id")})J->SetField(F,MakeShared<FJsonValueNull>());
    return J;
}
class FP2Check : public IAutomationLatentCommand {
    FAutomationTestBase* Test;int32 Mode,Phase=0;double Start=0;
public:
    FP2Check(FAutomationTestBase* T,int32 M):Test(T),Mode(M){}
    bool Update() override {
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();
        if(!W || !W->GetGameInstance()){Test->AddError(TEXT("PIE missing"));return true;}
        auto* GI=W->GetGameInstance();auto* R=GI->GetSubsystem<UDroneRegistrySubsystem>();auto* S=GI->GetSubsystem<UOperationalContextSubsystem>();
        auto* PC=Cast<ADroneOpsPlayerController>(W->GetFirstPlayerController());auto* M=PC?PC->GetCommandScreenManager():nullptr;
        if(!M){Test->AddError(TEXT("role manager missing"));return true;}
        if(Start==0)Start=FPlatformTime::Seconds();
        if(Mode>=4) {
            if(FPlatformTime::Seconds()-Start>18){Test->AddError(TEXT("Live Backend authority test timed out"));return true;}
            if(!S->IsReady())return false;
            if(Phase==0){for(int32 Id:{1,2,3}){FDroneDescriptor D;D.DroneId=Id;D.Name=TEXT("P2 QA");R->RegisterDrone(D);}
                if(!S->SetActiveUAV(3)){Test->AddError(TEXT("initial request rejected"));return true;}Phase=1;return false;}
            if(Phase==1){if(R->GetPrimarySelectedDrone()!=3)return false;
                bool Accepted=false;
                if(Mode==4){auto* Map=CreateWidget<UCommandTacticalMap>(PC,UCommandTacticalMap::StaticClass());Accepted=Map->SelectUAV(2);}
                else {auto* V=M->GetVideoShell();if(!V){Test->AddError(TEXT("Video role required"));return true;}V->SelectDrone(2);Test->TestEqual(TEXT("source browse retains global"),R->GetPrimarySelectedDrone(),3);Accepted=V->SetAsActiveUAV();}
                Test->TestTrue(TEXT("Backend request submitted"),Accepted);Test->TestEqual(TEXT("no optimistic Registry mutation"),R->GetPrimarySelectedDrone(),3);Phase=2;return false;}
            if(R->GetPrimarySelectedDrone()!=2)return false;
            FString Id;S->GetContext()->TryGetStringField(TEXT("active_uav_id"),Id);Test->TestEqual(TEXT("Backend acknowledged target"),Id,FString(TEXT("UAV-02")));return true;
        }
        if(Mode==0) {
            Test->TestNotNull(TEXT("Command V2 shell"),M->GetShell());
            for(UClass* Class:{UDroneListWidget::StaticClass(),UCommandAlarmPanel::StaticClass(),UCommandTacticalMap::StaticClass(),UOperationalLogWidget::StaticClass()}){
                TArray<UUserWidget*> Widgets;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,Class,false);Test->TestEqual(*Class->GetName(),Widgets.Num(),1);
                for(auto* Widget:Widgets)if(auto* Map=Cast<UCommandTacticalMap>(Widget))Test->TestTrue(TEXT("valid configured XYZ basemap"),Map->HasConfiguredBasemap());}
            const auto Equator=UCommandTacticalMap::Project(0,0,0);Test->TestTrue(TEXT("WebMercator equator"),Equator.Equals(FVector2D(128,128),.001));return true;
        }
        auto* Events=GI->GetSubsystem<UOperationalEventStore>();
        if(Mode==1){const uint64 Before=Events->GetLatestSequence();Events->ProjectPresence(TEXT("historical-offline"),TEXT("Map"),TEXT("OFFLINE"));Test->TestEqual(TEXT("offline snapshot is not a new disconnect event"),Events->GetLatestSequence(),Before);Events->ProjectPresence(TEXT("p2-qa"),TEXT("Map"),TEXT("ONLINE"));
            S->ApplyContext(P2Snapshot(S->GetVersion()+1,3));GI->GetSubsystem<UCommandAlertStore>()->AddAlert(3,TEXT("lost_connection"),TEXT("P2 QA communication lost"));
            bool Presence=false,Selection=false,Alert=false;for(const auto& E:Events->GetEvents())if(E.Sequence>Before){Presence|=E.EventType==TEXT("client_connected");Selection|=E.EventType==TEXT("active_uav_changed");Alert|=E.EventType==TEXT("alert_raised");}
            Test->TestTrue(TEXT("presence projection"),Presence);Test->TestTrue(TEXT("context projection"),Selection);Test->TestTrue(TEXT("alert projection"),Alert);
            const uint64 Once=Events->GetLatestSequence();Events->ProjectPresence(TEXT("p2-qa"),TEXT("Map"),TEXT("ONLINE"));Test->TestEqual(TEXT("duplicate presence suppressed"),Events->GetLatestSequence(),Once);return true;}
        auto* V=M->GetVideoShell();if(!V){Test->AddError(TEXT("Video role required"));return true;}
        for(int32 Id:{1,2,3}){FDroneDescriptor D;D.DroneId=Id;D.Name=TEXT("P2 QA");R->RegisterDrone(D);}
        S->ApplyContext(P2Snapshot(S->GetVersion()+1,3));
        Test->TestFalse(TEXT("follow defaults off"),V->IsFollowingTarget());V->SelectDrone(1);Test->TestEqual(TEXT("local source"),V->GetDisplayedDroneId(),1);
        Test->TestEqual(TEXT("source never patches global"),R->GetPrimarySelectedDrone(),3);
        S->ApplyContext(P2Snapshot(S->GetVersion()+1,2));V->Refresh();Test->TestEqual(TEXT("follow off retains source"),V->GetDisplayedDroneId(),1);
        if(Mode==3){V->SetFollowTarget(true);Test->TestEqual(TEXT("enabling follow applies current context"),V->GetDisplayedDroneId(),2);
            S->ApplyContext(P2Snapshot(S->GetVersion()+1,3));V->Refresh();Test->TestEqual(TEXT("follow on tracks changes"),V->GetDisplayedDroneId(),3);
            V->SetFollowTarget(false);S->ApplyContext(P2Snapshot(S->GetVersion()+1,2));V->Refresh();Test->TestEqual(TEXT("disabled follow retains source"),V->GetDisplayedDroneId(),3);}
        return true;
    }
};
}
#define P2_TEST(Class,Name,Mode) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,"DroneOps.P2." Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
bool Class::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Level/CesiumWorld")));ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));ADD_LATENT_AUTOMATION_COMMAND(FP2Check(this,Mode));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
P2_TEST(FP2Layout,"CommandLayoutSmoke",0)
P2_TEST(FP2Events,"EventLogProjection",1)
P2_TEST(FP2Independent,"VideoIndependentSelection",2)
P2_TEST(FP2Follow,"VideoFollowTarget",3)
P2_TEST(FP2MapSelection,"CommandMapSelection",4)
P2_TEST(FP2SetActive,"VideoSetActiveTarget",5)
#undef P2_TEST
#endif
