#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Video/VideoShellWidget.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandAlertStore.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Shared/ProductText.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Network/DroneNetworkManager.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "WebBrowser.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Editor.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Settings/LevelEditorPlaySettings.h"

DEFINE_LATENT_AUTOMATION_COMMAND(FFigmaVideoStartPIE);
bool FFigmaVideoStartPIE::Update()
{
    FRequestPlaySessionParams P;P.EditorPlaySettings=DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(),GetTransientPackage());
    P.EditorPlaySettings->NewWindowWidth=1920;P.EditorPlaySettings->NewWindowHeight=1080;P.EditorPlaySettings->CenterNewWindow=true;GUnrealEd->RequestPlaySession(P);return true;
}

// Real VideoView HTTP/ACK actions, controlled local descriptors and rendered UMG.
// about:blank fixtures prove source/lifecycle behavior, never decoded playback.
class FFigmaVideoChecks : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;int Step=0,First=0,Second=0,SlotIndex=0,Generation=0,AlertId=0;
    double Started=FPlatformTime::Seconds(),Next=0;FString Out;TWeakObjectPtr<UWebBrowser> Browser;
    FDroneDescriptor SavedFirst,SavedSecond;FDroneTelemetrySnapshot SavedTelemetry;bool WasIsolated=false,HadTelemetry=false;
    void Capture(const TCHAR* Name)
    {
        if(FParse::Param(FCommandLine::Get(),TEXT("NullRHI")))return;
        FSlateApplication::Get().InvalidateAllWidgets(false);TArray<FColor> Pixels;FIntVector Size=FIntVector::ZeroValue;
        auto View=GEngine->GameViewport?GEngine->GameViewport->GetGameViewportWidget():nullptr;
        if(!View || !FSlateApplication::Get().TakeScreenshot(View.ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Video render screenshot unavailable"));return;}
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
        Test->TestTrue(TEXT("Video screenshot saved"),FFileHelper::SaveArrayToFile(Png,*(Out/(FString(Name)+TEXT(".png")))));
    }
    FString Target(UOperationalContextSubsystem* Sync)
    {
        FString Value;if(Sync->GetVideoView())Sync->GetVideoView()->TryGetStringField(TEXT("video_target_uav_id"),Value);return Value;
    }
public:
    explicit FFigmaVideoChecks(FAutomationTestBase* T):Test(T)
    {
        Out=FPaths::ProjectSavedDir()/TEXT("FigmaVideoQA")/FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"));
        IFileManager::Get().MakeDirectory(*Out,true);Test->AddInfo(TEXT("Video evidence: ")+Out);
    }
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Now-Started>90){Test->AddError(FString::Printf(TEXT("Video workflow timeout at step %d"),Step));return true;}if(Now<Next)return false;
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();if(!World)return false;
        auto* PC=Cast<ADroneOpsPlayerController>(World->GetFirstPlayerController());auto* Manager=PC?PC->GetCommandScreenManager():nullptr;auto* Shell=Manager?Manager->GetVideoShell():nullptr;if(!Shell)return false;
        auto* GI=World->GetGameInstance();auto* Sync=GI->GetSubsystem<UOperationalContextSubsystem>();auto* Registry=GI->GetSubsystem<UDroneRegistrySubsystem>();
        auto* Network=GI->GetSubsystem<UDroneNetworkManager>();if(!Sync->IsHydrated())return false;Shell->Refresh();
        if(Step==0){
            const auto Drones=Registry->GetFriendlyDroneDescriptors();if(Drones.Num()<2)return false;
            SavedFirst=Drones[0];SavedSecond=Drones[1];First=SavedFirst.DroneId;Second=SavedSecond.DroneId;
            WasIsolated=Network->IsStrictLocalPreviewIsolation();Network->SetStrictLocalPreviewIsolation(true);
            auto D=SavedFirst;D.VideoUrl=TEXT("about:blank#figma-video-first");Registry->RegisterDrone(D);
            D=SavedSecond;D.VideoUrl=TEXT("about:blank#figma-video-second");Registry->RegisterDrone(D);
            HadTelemetry=Registry->GetTelemetry(First,SavedTelemetry);
            FDroneTelemetrySnapshot Offline;Offline.Availability=EDroneAvailability::Offline;Offline.LastUpdateTime=0;
            Registry->UpdateTelemetry(First,Offline);Shell->Refresh();
            Test->TestEqual(TEXT("zero-heartbeat Offline remains explicit connection state"),Shell->GetAircraftButton(First)->Availability->GetText().ToString(),ProductText::Get(TEXT("Fleet.Offline")).ToString());
            Browser=Shell->GetBrowser();int Count=0;Shell->WidgetTree->ForEachWidget([&](UWidget* W){if(Cast<UWebBrowser>(W))++Count;});
            Test->TestEqual(TEXT("all layouts own exactly one Browser"),Count,1);
            bool ReservedTexture=false;Shell->WidgetTree->ForEachWidget([&](UWidget* W){
                if(auto* Image=Cast<UImage>(W))if(auto* Texture=Cast<UTexture2D>(Image->GetBrush().GetResourceObject()))
                    ReservedTexture|=Texture->GetSizeX()==886 && Texture->GetSizeY()==886;});
            Test->TestTrue(TEXT("original reserved PNG is a loaded texture, not an unresolved file brush"),ReservedTexture);
            Manager->Show();Manager->Show();Test->TestTrue(TEXT("repeated Show retains Video shell"),Shell==Manager->GetVideoShell());
            TArray<UUserWidget*> Widgets;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World,Widgets,UVideoShellWidget::StaticClass(),true);
            Test->TestEqual(TEXT("unique visible Video shell"),Widgets.Num(),1);
            const int Before=Shell->GetDisplayedDroneId();Shell->SelectDrone(First);
            Test->TestEqual(TEXT("click cannot optimistically replace video before ACK"),Shell->GetDisplayedDroneId(),Before);
            Sync->SetLanguage(TEXT("zh-Hans"));++Step;return false;
        }
        if(Step==1){
            if(Target(Sync)!=FString::Printf(TEXT("UAV-%02d"),First) || Shell->GetDisplayedDroneId()!=First)return false;
            Test->TestEqual(TEXT("ACK selects exact configured source"),Shell->GetSourceUrl(),FString(TEXT("about:blank#figma-video-first")));
            Test->TestTrue(TEXT("configured source uses enabled original retry icon"),Shell->RetryReadyIcon->IsVisible() && !Shell->RetryIcon->IsVisible());
            Generation=Shell->GetBrowserGeneration();Test->TestFalse(TEXT("page URL alone is not playback"),Shell->GetMediaState()==TEXT("PLAYING"));
            Test->TestTrue(TEXT("undecoded source remains covered"),Shell->MediaCover->IsVisible());
            Sync->SetActiveUAV(Second);++Step;return false;
        }
        if(Step==2){
            FString Active;Sync->GetContext()->TryGetStringField(TEXT("active_uav_id"),Active);if(Active!=FString::Printf(TEXT("UAV-%02d"),Second))return false;
            Test->TestEqual(TEXT("active UAV ACK leaves explicit video source alone"),Shell->GetDisplayedDroneId(),First);
            Test->TestEqual(TEXT("shared selection does not reload Browser"),Shell->GetBrowserGeneration(),Generation);
            Shell->SetFollowTarget(true);Test->TestFalse(TEXT("obsolete selection follow remains disabled"),Shell->IsFollowingTarget());
            for(SlotIndex=0;SlotIndex<5;++SlotIndex)if(Shell->GetSecondaryDroneId(SlotIndex)==Second)break;
            if(!Test->TestTrue(TEXT("second registered UAV is a selectable source card"),SlotIndex<5))return true;
            Shell->GetSecondaryButton(SlotIndex)->OnClicked.Broadcast();
            Test->TestEqual(TEXT("secondary click also waits for VideoView ACK"),Shell->GetDisplayedDroneId(),First);++Step;return false;
        }
        if(Step==3){
            if(Target(Sync)!=FString::Printf(TEXT("UAV-%02d"),Second) || Shell->GetDisplayedDroneId()!=Second)return false;
            Test->TestEqual(TEXT("old primary exchanges into clicked source slot"),Shell->GetSecondaryDroneId(SlotIndex),First);
            auto D=SavedSecond;D.VideoUrl=TEXT("about:blank#figma-video-updated");Registry->RegisterDrone(D);Shell->Refresh();
            Test->TestEqual(TEXT("same UAV URL update reloads source"),Shell->GetSourceUrl(),D.VideoUrl);
            const int Before=Shell->GetBrowserGeneration();Shell->Retry->OnClicked.Broadcast();
            Test->TestEqual(TEXT("enabled retry renews media generation"),Shell->GetBrowserGeneration(),Before+1);
            D.VideoUrl.Empty();Registry->RegisterDrone(D);Shell->Refresh();
            Test->TestEqual(TEXT("source deletion clears URL"),Shell->GetSourceUrl(),FString());
            Test->TestEqual(TEXT("source deletion is explicit no source"),Shell->GetMediaState(),FString(TEXT("NO SOURCE")));
            Test->TestFalse(TEXT("retry unavailable without source"),Shell->Retry->GetIsEnabled());
            Test->TestTrue(TEXT("missing source uses disabled original retry icon"),Shell->RetryIcon->IsVisible() && !Shell->RetryReadyIcon->IsVisible());
            Test->TestFalse(TEXT("cleared source browser hidden"),Shell->Browser->IsVisible());
            for(auto B:Shell->UnsupportedButtons)Test->TestFalse(TEXT("unimplemented media actions disabled"),B->GetIsEnabled());
            Generation=Shell->GetBrowserGeneration();Shell->SetViewMode(6);Next=Now+1.5;++Step;return false;
        }
        if(Step==4){
            Capture(TEXT("01-six-slots"));Shell->SetViewMode(4);
            Test->TestEqual(TEXT("four layout hides fourth secondary"),Shell->GetSecondaryButton(3)->GetVisibility(),ESlateVisibility::Collapsed);
            Next=Now+1.5;++Step;return false;
        }
        if(Step==5){
            Capture(TEXT("02-four-slots"));Shell->SetViewMode(1);
            Test->TestEqual(TEXT("focus collapses secondary container"),Shell->SecondaryArea->GetVisibility(),ESlateVisibility::Collapsed);
            Test->TestEqual(TEXT("focus reclaims primary panel height"),Cast<UCanvasPanelSlot>(Shell->PrimaryPanel->Slot)->GetSize().Y,574.);
            Next=Now+1.5;++Step;return false;
        }
        if(Step==6){
            Capture(TEXT("03-focus"));Sync->SetLanguage(TEXT("en"));Next=Now+1.5;++Step;return false;
        }
        if(Step==7){
            FString Language;if(Sync->GetUIPreferences())Sync->GetUIPreferences()->TryGetStringField(TEXT("language"),Language);if(Language!=TEXT("en"))return false;
            Test->TestTrue(TEXT("language and layouts retain Browser object"),Shell->GetBrowser()==Browser.Get());
            Test->TestEqual(TEXT("language and layouts retain media generation"),Shell->GetBrowserGeneration(),Generation);
            Capture(TEXT("04-focus-english"));
            const int PreviousAlerts=GI->GetSubsystem<UCommandAlertStore>()->GetUnhandledCount();
            AlertId=GI->GetSubsystem<UCommandAlertStore>()->AddAlert(First,TEXT("test"),TEXT("Automation received alert fixture"));
            Shell->Refresh();Test->TestEqual(TEXT("received alert projected into sidebar"),Shell->AlertRows->GetChildrenCount(),PreviousAlerts+1);
            Test->TestFalse(TEXT("real received alert replaces empty state"),Shell->AlertEmpty->IsVisible());
            GI->GetSubsystem<UCommandAlertStore>()->ClearAlert(AlertId);Shell->Refresh();
            Test->TestEqual(TEXT("cleared fixture restores prior empty state"),Shell->AlertEmpty->IsVisible(),PreviousAlerts==0);
            Manager->Hide();Test->TestEqual(TEXT("Hide closes media"),Shell->GetMediaState(),FString(TEXT("OFFLINE / CLOSED")));
            Test->TestTrue(TEXT("Hide invalidates outstanding probes"),Shell->GetBrowserGeneration()>Generation);
            Manager->Show();Shell->Refresh();Test->TestTrue(TEXT("Show resumes same Browser"),Shell->GetBrowser()==Browser.Get());
            Registry->UnregisterDrone(Second);Shell->Refresh();
            Test->TestEqual(TEXT("removed target does not fall back to active UAV"),Shell->GetDisplayedDroneId(),0);
            Test->TestEqual(TEXT("removed target clears source"),Shell->GetSourceUrl(),FString());
            Registry->RegisterDrone(SavedFirst);Registry->RegisterDrone(SavedSecond);if(HadTelemetry)Registry->UpdateTelemetry(First,SavedTelemetry);Network->SetStrictLocalPreviewIsolation(WasIsolated);
            Shell->SetViewMode(6);Shell->Refresh();
            Test->AddInfo(TEXT("Backend ACK, widget-action and rendered evidence only; no native mouse or real-video playback claimed."));return true;
        }
        return true;
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFigmaVideoPresentation,"DroneOps.Figma.VideoPresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFigmaVideoPresentation::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FFigmaVideoStartPIE());ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FFigmaVideoChecks(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
