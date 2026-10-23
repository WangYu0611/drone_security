#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Map/MapFleetInteractionWidget.h"
#include "Map/MapShellWidget.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandMapInteractionService.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Shared/PlanWidgetSupport.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
using namespace PlanUI;

// Rendered Slate layout and real selection ACKs. This does not drive an OS
// pointer or resize the user's desktop; native narrow-window QA is separate.
class FMapFleetCardLayout : public IAutomationLatentCommand {
    FAutomationTestBase* Test;
    FString Culture;
    int Step=0,OriginalPrimary=0;
    TArray<int32> OriginalSelection;
    double Started=FPlatformTime::Seconds(),Next=0;
    bool bCultureChanged=false,bHadPreview=false;
    FString PreviousPreview;
    FInternationalization::FCultureStateSnapshot PreviousCulture;
    void CheckLayout(UMapFleetInteractionWidget* Fleet,const TCHAR* State) {
        auto Rect=[](const UWidget* Widget){const auto& G=Widget->GetCachedGeometry();return FBox2D(G.LocalToAbsolute(FVector2D::ZeroVector),G.LocalToAbsolute(G.GetLocalSize()));};
        const auto Panel=Rect(Fleet->Panel),Summary=Rect(Fleet->Summary),Selection=Rect(Fleet->SelectionText);
        const auto Confirm=Rect(Fleet->Buttons[TEXT("confirm")]),Cancel=Rect(Fleet->Buttons[TEXT("cancel")]);
        Test->AddInfo(FString::Printf(TEXT("CardLayout culture=%s state=%s viewport=%s panel=%s summary=%s selection=%s confirm=%s cancel=%s"),
            *Culture,State,*Fleet->GetCachedGeometry().GetLocalSize().ToString(),*Panel.GetSize().ToString(),*Summary.GetSize().ToString(),*Selection.GetSize().ToString(),*Confirm.GetSize().ToString(),*Cancel.GetSize().ToString()));
        Test->TestTrue(TEXT("Summary and selected-aircraft row never overlap"),Summary.Max.Y<=Selection.Min.Y+.5);
        for(const auto* Label:{Fleet->Summary.Get(),Fleet->SelectionText.Get(),Fleet->Result.Get()}){
            Test->TestTrue(TEXT("Wrapped text receives its full measured height"),Label->GetDesiredSize().Y<=Label->GetCachedGeometry().GetLocalSize().Y+1.);
            Test->TestTrue(TEXT("Wrapped text stays inside the card width"),Rect(Label).Min.X>=Panel.Min.X-.5 && Rect(Label).Max.X<=Panel.Max.X+.5);
        }
        Test->TestTrue(TEXT("Confirm and exit remain separate visible rows"),Confirm.bIsValid && Cancel.bIsValid && Confirm.Max.Y<=Cancel.Min.Y+.5);
        Test->TestTrue(TEXT("Confirm and exit stay inside the card"),Confirm.Min.Y>=Panel.Min.Y && Cancel.Max.Y<=Panel.Max.Y+.5);
        Test->TestTrue(TEXT("Result feedback does not overlap confirmation"),Rect(Fleet->Result).Max.Y<=Confirm.Min.Y+.5);
    }
public:
    FMapFleetCardLayout(FAutomationTestBase* In,const FString& InCulture):Test(In),Culture(InCulture){}
    ~FMapFleetCardLayout() override {
        if(!bCultureChanged)return;
        auto& I=FInternationalization::Get();auto& L=FTextLocalizationManager::Get();
        I.RestoreCultureState(PreviousCulture);
        if(bHadPreview)L.EnableGameLocalizationPreview(PreviousPreview.IsEmpty()?PreviousCulture.Language:PreviousPreview);
        else L.DisableGameLocalizationPreview();
        L.WaitForAsyncTasks();
    }
    bool Update() override {
        if(FPlatformTime::Seconds()-Started>60.){Test->AddError(FString::Printf(TEXT("Card layout timed out at step %d"),Step));return true;}
        if(FPlatformTime::Seconds()<Next)return false;
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();
        auto* PC=W?Cast<ADroneOpsPlayerController>(W->GetFirstPlayerController()):nullptr;
        auto* Manager=PC?PC->GetCommandScreenManager():nullptr;auto* Shell=Manager?Manager->GetMapShell():nullptr;
        auto* Fleet=Shell?Shell->GetFleetInteraction():nullptr;if(!Fleet)return false;
        auto* Sync=W->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
        auto* Registry=W->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
        if(!Sync->IsReady() || !Sync->IsHydrated() || !Registry->IsDroneRegistered(1))return false;
        if(Step==0){
            auto& I=FInternationalization::Get();auto& L=FTextLocalizationManager::Get();I.BackupCultureState(PreviousCulture);
            bHadPreview=L.IsGameLocalizationPreviewEnabled();PreviousPreview=L.GetConfiguredGameLocalizationPreviewLanguage();bCultureChanged=true;
            if(!Test->TestTrue(TEXT("Requested layout culture is available"),I.SetCurrentCulture(Culture)))return true;
            L.EnableGameLocalizationPreview(Culture);L.WaitForAsyncTasks();
            OriginalPrimary=Registry->GetPrimarySelectedDrone();OriginalSelection=Registry->GetMultiSelectedDrones();
            Manager->GetMapService()->Enter3DMode();
            if(!Test->TestTrue(TEXT("Request empty selection through Backend"),Sync->SetLocalSelection(0,{})))return true;
            ++Step;return false;
        }
        if(Step==1){
            if(Registry->GetPrimarySelectedDrone()!=0 || !Registry->GetMultiSelectedDrones().IsEmpty())return false;
            Fleet->SetMode(EMapFleetMode::GroupMove);++Step;Next=FPlatformTime::Seconds()+.3;return false;
        }
        if(Step==2){
            CheckLayout(Fleet,TEXT("empty"));
            Test->TestEqual(TEXT("Empty card uses the concise localized instruction"),Fleet->Summary->GetText().ToString(),Culture==TEXT("zh-Hans")?FString(TEXT("单击选机，或点击“框选无人机”。")):FString(TEXT("Click an aircraft, or use 'Box select UAVs'.")));
            Test->TestEqual(TEXT("Detailed selection instructions remain available on hover"),Fleet->Summary->GetToolTipText().ToString(),T(TEXT("FleetUX.SelectionHelp")).ToString());
            if(!Test->TestTrue(TEXT("Request one aircraft through Backend"),Sync->SetLocalSelection(1,{1})))return true;
            ++Step;return false;
        }
        if(Step==3){
            if(Registry->GetPrimarySelectedDrone()!=1 || Registry->GetMultiSelectedDrones()!=TArray<int32>{1})return false;
            Fleet->Refresh();++Step;Next=FPlatformTime::Seconds()+.3;return false;
        }
        if(Step==4){
            CheckLayout(Fleet,TEXT("selected-3d"));
            Test->TestEqual(TEXT("Selected card asks only for the next action"),Fleet->Summary->GetText().ToString(),T(TEXT("FleetUX.PickHint")).ToString());
            // Long actionable server feedback must not cover the pinned action
            // row, independently of the length of a user-entered plan name.
            Fleet->ErrorDetails=FText::Format(T(TEXT("Reservation.Conflict")),User(TEXT("UAV-01")),User(TEXT("A long plan name used to verify multiline server feedback in the movement card")));
            Fleet->Refresh();++Step;Next=FPlatformTime::Seconds()+.3;return false;
        }
        if(Step==5){
            CheckLayout(Fleet,TEXT("long-feedback"));Fleet->SetMode(EMapFleetMode::Browse);
            if(!Test->TestTrue(TEXT("Restore prior selection through Backend"),Sync->SetLocalSelection(OriginalPrimary,OriginalSelection)))return true;
            ++Step;return false;
        }
        return Registry->GetPrimarySelectedDrone()==OriginalPrimary && Registry->GetMultiSelectedDrones()==OriginalSelection;
    }
};

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FMapFleetCardLayoutTest,"DroneOps.Map.InteractionUX.CardLayout",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FMapFleetCardLayoutTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const {
    Names.Add(TEXT("Chinese"));Commands.Add(TEXT("zh-Hans"));Names.Add(TEXT("English"));Commands.Add(TEXT("en"));
}
bool FMapFleetCardLayoutTest::RunTest(const FString& Parameters) {
    if(FParse::Param(FCommandLine::Get(),TEXT("NullRHI"))){AddError(TEXT("Card layout requires a rendered Slate viewport."));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FMapFleetCardLayout(this,Parameters));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
