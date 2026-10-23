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
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
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
    double Started=FPlatformTime::Seconds(),Next=0;FString Out;TWeakObjectPtr<UWebBrowser> Browser;int RejectedTargetBefore=0;
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
            Test->TestTrue(TEXT("video switch exposes pending until ACK"),Sync->IsVideoSwitchPending());
            Test->TestEqual(TEXT("pending target is explicit"),Sync->GetPendingVideoTarget(),FString::Printf(TEXT("UAV-%02d"),First));
            Test->TestFalse(TEXT("second video request is rejected while pending"),Sync->OpenVideo(FString::Printf(TEXT("UAV-%02d"),Second)));
            Test->TestEqual(TEXT("rejected duplicate retains first pending target"),Sync->GetPendingVideoTarget(),FString::Printf(TEXT("UAV-%02d"),First));
            Test->TestFalse(TEXT("source buttons disabled while switching"),Shell->GetAircraftButton(First)->GetIsEnabled());
            Test->TestTrue(TEXT("switch feedback names requested source"),Shell->Status->GetText().ToString().Contains(FString::Printf(TEXT("UAV-%02d"),First)));
            Sync->SetLanguage(TEXT("zh-Hans"));++Step;return false;
        }
        if(Step==1){
            if(Sync->IsVideoSwitchPending() || Target(Sync)!=FString::Printf(TEXT("UAV-%02d"),First) || Shell->GetDisplayedDroneId()!=First)return false;
            Test->TestTrue(TEXT("source buttons recover after ACK"),Shell->GetAircraftButton(First)->GetIsEnabled());
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
            if(Sync->IsVideoSwitchPending() || Target(Sync)!=FString::Printf(TEXT("UAV-%02d"),Second) || Shell->GetDisplayedDroneId()!=Second)return false;
            if(Shell->Browser->GetUrl()!=Shell->SourceUrl || !Shell->bPageReady)return false;
            // Synthetic browser probes exercise presentation policy. They are
            // not evidence of a real stream or decoded hardware video.
            double Sample=FMath::Max(Shell->LastSample+1.,1.);
            auto Probe=[&](bool Paused,bool Error,double Frames){
                auto Data=MakeShared<FJsonObject>();Data->SetNumberField(TEXT("g"),Shell->Generation);
                Data->SetNumberField(TEXT("s"),Sample++);Data->SetStringField(TEXT("url"),Shell->Browser->GetUrl());
                Data->SetNumberField(TEXT("t"),Frames/30.);Data->SetNumberField(TEXT("f"),Frames);
                Data->SetNumberField(TEXT("w"),640);Data->SetNumberField(TEXT("h"),360);
                Data->SetBoolField(TEXT("p"),Paused);Data->SetBoolField(TEXT("e"),Error);
                FString Json;FJsonSerializer::Serialize(Data,TJsonWriterFactory<>::Create(&Json));
                Shell->ConsoleMessage(TEXT("V1-")+Shell->ProbeToken+TEXT(":")+Json,TEXT("Automation"),0);
            };
            Probe(false,false,300);Probe(false,false,330);
            Test->TestEqual(TEXT("progress probes enter playing"),Shell->GetMediaState(),FString(TEXT("PLAYING")));
            Probe(true,false,330);
            Test->TestEqual(TEXT("paused confirmed source stays explicit"),Shell->GetMediaState(),FString(TEXT("PAUSED / ENDED")));
            Test->TestEqual(TEXT("paused decoded frame leaves player controls uncovered"),Shell->MediaCover->GetVisibility(),ESlateVisibility::Collapsed);
            Shell->LastProgress=Now-20.;Shell->LastProbe=FPlatformTime::Seconds();Shell->Refresh();
            Test->TestEqual(TEXT("intentional pause is not converted into a stall"),Shell->GetMediaState(),FString(TEXT("PAUSED / ENDED")));
            Probe(false,false,330);
            Test->TestEqual(TEXT("resume without a new frame starts waiting"),Shell->GetMediaState(),FString(TEXT("PAGE READY / WAITING VIDEO")));
            Test->TestTrue(TEXT("resume starts a fresh progress timeout"),Shell->LastProgress>=Now);
            const double ResumeStarted=Shell->LastProgress;
            Probe(false,false,330);
            Test->TestEqual(TEXT("unchanged resumed probes do not extend timeout"),Shell->LastProgress,ResumeStarted);
            Shell->LastProgress=FPlatformTime::Seconds()-11.;Shell->LastProbe=FPlatformTime::Seconds();Shell->Refresh();
            Test->TestEqual(TEXT("resumed stream without frames times out"),Shell->GetMediaState(),FString(TEXT("STALLED / NO FRAMES")));
            Test->TestTrue(TEXT("resumed stalled stream is covered"),Shell->MediaCover->IsVisible());
            Probe(false,false,360);
            Test->TestEqual(TEXT("new frame recovers resumed playback"),Shell->GetMediaState(),FString(TEXT("PLAYING")));
            Test->TestEqual(TEXT("recovered playback exposes player controls"),Shell->MediaCover->GetVisibility(),ESlateVisibility::Collapsed);
            Probe(false,true,360);
            Test->TestEqual(TEXT("media error gives retry guidance"),Shell->EmptyHint->GetText().ToString(),ProductText::Get(TEXT("VideoUI.FailedHint")).ToString());
            Test->TestTrue(TEXT("failed media is covered"),Shell->MediaCover->IsVisible());
            Shell->Refresh();Test->TestEqual(TEXT("watchdog preserves specific media error"),Shell->GetMediaState(),FString(TEXT("OFFLINE / MEDIA ERROR")));
            Test->TestEqual(TEXT("old primary exchanges into clicked source slot"),Shell->GetSecondaryDroneId(SlotIndex),First);
            auto D=SavedSecond;D.VideoUrl=TEXT("about:blank#figma-video-updated");Registry->RegisterDrone(D);Shell->Refresh();
            Test->TestEqual(TEXT("same UAV URL update reloads source"),Shell->GetSourceUrl(),D.VideoUrl);
            Test->TestFalse(TEXT("new generation clears decoded-frame permission"),Shell->bHasDecodedFrame);
            Shell->MediaState=TEXT("PAUSED / ENDED");Shell->UpdateLabels();
            Test->TestTrue(TEXT("unconfirmed paused generation remains covered"),Shell->MediaCover->IsVisible());
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
            RejectedTargetBefore=Shell->GetDisplayedDroneId();
            Test->TestTrue(TEXT("invalid target exercises real HTTP rejection"),Sync->OpenVideo(TEXT("UX-INVALID-TARGET")));
            ++Step;return false;
        }
        if(Step==8){
            if(Sync->IsVideoSwitchPending())return false;
            Test->TestEqual(TEXT("failed request keeps last confirmed video"),Shell->GetDisplayedDroneId(),RejectedTargetBefore);
            Test->TestEqual(TEXT("failed switch names its operation"),Sync->ViewSyncError,FString(TEXT("VIDEO_SWITCH_FAILED")));
            Test->TestTrue(TEXT("failed switch restores source buttons"),Shell->GetAircraftButton(First)->GetIsEnabled());
            Test->TestEqual(TEXT("primary player owns the switch error"),Shell->Status->GetText().ToString(),ProductText::Get(TEXT("Errors.VIDEO_SWITCH_FAILED")).ToString());
            Test->TestFalse(TEXT("bottom context line does not repeat switch error"),Shell->SystemHeader->GetText().EqualTo(Shell->Status->GetText()));
            if(Sync->IsLanguageChangePending())return false;
            Test->TestTrue(TEXT("language request remains independent"),Sync->SetLanguage(TEXT("en")));++Step;return false;
        }
        if(Step==9){
            if(Sync->IsLanguageChangePending())return false;
            Test->TestEqual(TEXT("successful language ACK does not erase video error"),Sync->ViewSyncError,FString(TEXT("VIDEO_SWITCH_FAILED")));
            Test->TestTrue(TEXT("source can be retried after failure"),Sync->OpenVideo(FString::Printf(TEXT("UAV-%02d"),First)));++Step;return false;
        }
        if(Step==10){
            if(Sync->IsVideoSwitchPending())return false;
            Test->TestTrue(TEXT("confirmed video retry clears its error"),Sync->ViewSyncError.IsEmpty());
            Test->TestEqual(TEXT("retry selects requested source"),Shell->GetDisplayedDroneId(),First);
            Test->AddInfo(TEXT("Backend ACK, widget-action, synthetic media-probe and rendered evidence only; no native mouse or real-video playback claimed."));return true;
        }
        return true;
    }
};

// Pure request-state regression: no HTTP, sockets, map or browser is started.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVideoRequestFeedbackTest,"DroneOps.Video.RequestFeedback",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVideoRequestFeedbackTest::RunTest(const FString&)
{
    auto* GI=NewObject<UGameInstance>();
    auto* Sync=NewObject<UOperationalContextSubsystem>(GI);
    Sync->State=TEXT("CONNECTED");Sync->bVideoHydrated=Sync->bLanguageHydrated=true;
    auto Video=[](int Version,const TCHAR* Target){auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("video_view_version"),Version);J->SetStringField(TEXT("video_target_uav_id"),Target);return J;};
    auto Arm=[&](const TCHAR* Target){Sync->RequestedVideoTarget=Target;Sync->bVideoSwitchPending=true;return ++Sync->VideoSwitchSerial;};
    Sync->ApplyVideoView(Video(1,TEXT("UAV-01")));
    const auto Confirmed=Sync->GetVideoView();
    const auto First=Arm(TEXT("UAV-02"));
    Sync->Role=TEXT("Map");Sync->bPlansHydrated=true;
    TestFalse(TEXT("Map intent cannot overlap pending ordinary video request"),Sync->SetMapSelection(1,{1},1));
    TestFalse(TEXT("duplicate request cannot submit while pending"),Sync->OpenVideo(TEXT("UAV-03")));
    TestEqual(TEXT("duplicate preserves pending intent"),Sync->GetPendingVideoTarget(),FString(TEXT("UAV-02")));
    Sync->CompleteVideoSwitch(First-1,Video(2,TEXT("UAV-03")));
    TestTrue(TEXT("older completion cannot release newer pending"),Sync->IsVideoSwitchPending());
    TestTrue(TEXT("older completion cannot replace snapshot"),Sync->GetVideoView()==Confirmed);
    Sync->CompleteVideoSwitch(First,nullptr);
    TestFalse(TEXT("HTTP error releases pending"),Sync->IsVideoSwitchPending());
    TestEqual(TEXT("HTTP error is actionable video feedback"),Sync->ViewSyncError,FString(TEXT("VIDEO_SWITCH_FAILED")));
    TestTrue(TEXT("failure retains authoritative snapshot"),Sync->GetVideoView()==Confirmed);
    auto Preferences=MakeShared<FJsonObject>();Preferences->SetNumberField(TEXT("ui_preferences_version"),1);Preferences->SetStringField(TEXT("language"),TEXT("en"));
    Sync->UIPreferences=Preferences;Sync->RequestedLanguage=TEXT("en");Sync->bLanguageChangePending=true;
    Sync->CompleteLanguageChange(++Sync->LanguageChangeSerial,Preferences);
    TestFalse(TEXT("language ACK releases its own pending"),Sync->IsLanguageChangePending());
    TestEqual(TEXT("language success preserves failed video operation"),Sync->ViewSyncError,FString(TEXT("VIDEO_SWITCH_FAILED")));
    Sync->ApplyVideoView(Video(2,TEXT("UAV-02")));
    TestTrue(TEXT("authoritative recovery of requested target clears video error"),Sync->ViewSyncError.IsEmpty());
    const auto Interrupted=Arm(TEXT("UAV-03"));
    Sync->bLanguageChangePending=true;
    Sync->Offline();
    TestFalse(TEXT("disconnect releases video pending"),Sync->IsVideoSwitchPending());
    TestFalse(TEXT("disconnect releases language pending"),Sync->IsLanguageChangePending());
    TestEqual(TEXT("disconnect retains specific video failure"),Sync->ViewSyncError,FString(TEXT("VIDEO_SWITCH_INTERRUPTED")));
    Sync->CompleteVideoSwitch(Interrupted,Video(3,TEXT("UAV-03")));
    TestEqual(TEXT("old-generation completion does not commit"),Sync->GetVideoView()->GetStringField(TEXT("video_target_uav_id")),FString(TEXT("UAV-02")));
    TestEqual(TEXT("old-generation completion does not clear failure"),Sync->ViewSyncError,FString(TEXT("VIDEO_SWITCH_INTERRUPTED")));
    Sync->State=TEXT("CONNECTED");Sync->bVideoHydrated=true;
    Sync->ApplyVideoView(Video(3,TEXT("UAV-03")));
    TestTrue(TEXT("reconnect snapshot resolves an actually completed switch"),Sync->ViewSyncError.IsEmpty());
    Sync->ViewSyncError=TEXT("MAP_VIDEO_SYNC_FAILED");Sync->ApplyVideoView(Video(4,TEXT("UAV-01")));
    TestEqual(TEXT("ordinary recovery cannot erase Map operation error"),Sync->ViewSyncError,FString(TEXT("MAP_VIDEO_SYNC_FAILED")));
    Sync->ViewSyncError.Empty();const auto BeforeMap=Arm(TEXT("UAV-02"));
    Sync->CancelPendingViewRequests(false);Sync->ViewSyncError=TEXT("VIDEO_SELECTION_STALE");
    Sync->CompleteVideoSwitch(BeforeMap,Video(5,TEXT("UAV-02")));
    TestEqual(TEXT("cancelled callback cannot overwrite later Map error"),Sync->ViewSyncError,FString(TEXT("VIDEO_SELECTION_STALE")));
    TestEqual(TEXT("cancelled callback cannot overwrite later confirmed source"),Sync->GetVideoView()->GetStringField(TEXT("video_target_uav_id")),FString(TEXT("UAV-01")));
    Sync->ViewSyncError.Empty();
    const auto WsConfirmed=Arm(TEXT("UAV-02"));
    Sync->ApplyVideoView(Video(5,TEXT("UAV-02")));
    TestTrue(TEXT("WS confirmation retains request gate until completion"),Sync->IsVideoSwitchPending());
    Sync->CompleteVideoSwitch(WsConfirmed,nullptr);
    TestFalse(TEXT("lost video HTTP reply releases pending"),Sync->IsVideoSwitchPending());
    TestTrue(TEXT("WS-confirmed video is not misreported after HTTP loss"),Sync->ViewSyncError.IsEmpty());
    TestEqual(TEXT("HTTP loss retains the WS-confirmed target"),Sync->GetVideoView()->GetStringField(TEXT("video_target_uav_id")),FString(TEXT("UAV-02")));
    const auto Unconfirmed=Arm(TEXT("UAV-03"));
    Sync->CompleteVideoSwitch(Unconfirmed,nullptr);
    TestEqual(TEXT("unconfirmed video still reports HTTP failure"),Sync->ViewSyncError,FString(TEXT("VIDEO_SWITCH_FAILED")));
    TestEqual(TEXT("unconfirmed video cannot replace authoritative target"),Sync->GetVideoView()->GetStringField(TEXT("video_target_uav_id")),FString(TEXT("UAV-02")));
    Sync->bLanguageHydrated=true;Sync->RequestedLanguage=TEXT("zh-Hans");
    Sync->LanguageSyncError.Empty();Sync->bLanguageChangePending=true;
    const auto LanguageConfirmed=++Sync->LanguageChangeSerial;
    // A validated WS preferences snapshot, without invoking global locale services
    // on this deliberately uninitialized GameInstance.
    auto WsPreferences=MakeShared<FJsonObject>();WsPreferences->SetNumberField(TEXT("ui_preferences_version"),2);WsPreferences->SetStringField(TEXT("language"),TEXT("zh-Hans"));
    Sync->UIPreferences=WsPreferences;Sync->ReconcileLanguageFeedback();
    TestTrue(TEXT("language snapshot keeps pending until completion"),Sync->IsLanguageChangePending());
    Sync->CompleteLanguageChange(LanguageConfirmed,nullptr);
    TestFalse(TEXT("lost language HTTP reply releases pending"),Sync->IsLanguageChangePending());
    TestTrue(TEXT("WS-confirmed language is not misreported after HTTP loss"),Sync->GetLanguageSyncError().IsEmpty());
    TestEqual(TEXT("language reconciliation preserves unrelated video error"),Sync->ViewSyncError,FString(TEXT("VIDEO_SWITCH_FAILED")));
    Sync->RequestedLanguage=TEXT("en");Sync->bLanguageChangePending=true;
    Sync->CompleteLanguageChange(++Sync->LanguageChangeSerial,nullptr);
    TestEqual(TEXT("unconfirmed language still reports HTTP failure"),Sync->GetLanguageSyncError(),FString(TEXT("LANGUAGE_CHANGE_FAILED")));
    TestEqual(TEXT("unconfirmed language retains authoritative preference"),Sync->GetUIPreferences()->GetStringField(TEXT("language")),FString(TEXT("zh-Hans")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFigmaVideoPresentation,"DroneOps.Figma.VideoPresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFigmaVideoPresentation::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FFigmaVideoStartPIE());ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FFigmaVideoChecks(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
