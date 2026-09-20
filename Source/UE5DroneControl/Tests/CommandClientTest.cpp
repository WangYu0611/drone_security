#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Command/CommandAlertStore.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandShellWidget.h"
#include "Command/CommandMapInteractionService.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Network/DroneNetworkManager.h"
#include "DroneOps/Network/DroneWebSocketClient.h"
#include "PathEditor/DronePlaybackManager.h"
#include "PathEditor/DronePathActor.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SViewport.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UI/DroneListWidget.h"
#include "UI/DroneListItemWidget.h"
#include "UI/DroneVideoWindowManager.h"
#include "UI/DroneVideoWindowWidget.h"
#include "UI/SequenceDispatchPanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"
#if WITH_EDITOR
#include "Tests/AutomationEditorCommon.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommandAlertStoreTest, "DroneOps.Command.AlertStore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommandAlertStoreTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>();
    UCommandAlertStore* Store = NewObject<UCommandAlertStore>(GI);
    const int32 Id = Store->AddAlert(3, TEXT("low_battery"), TEXT("test fixture"));
    TestEqual(TEXT("new alert is unhandled"), Store->GetUnhandledCount(), 1);
    TestTrue(TEXT("mark handled"), Store->MarkHandled(Id));
    TestTrue(TEXT("mark handled is idempotent"), Store->MarkHandled(Id));
    TestEqual(TEXT("handled removed from active count"), Store->GetUnhandledCount(), 0);
    TestTrue(TEXT("clear known"), Store->ClearAlert(Id));
    TestFalse(TEXT("clear missing"), Store->ClearAlert(Id));
    for (int32 Index = 0; Index < 205; ++Index) Store->AddAlert(3, TEXT("event"), TEXT("bounded history test"));
    TestEqual(TEXT("bounded history"), Store->GetAlerts().Num(), UCommandAlertStore::MaxAlerts);
    TestTrue(TEXT("oldest evicted, ids never reused"), Store->GetAlerts()[0].Id > Id + 1);
    return true;
}

#if WITH_EDITOR
DEFINE_LATENT_AUTOMATION_COMMAND(FCommandStartPIE);
bool FCommandStartPIE::Update()
{
    FRequestPlaySessionParams Params;
    Params.EditorPlaySettings = DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage());
    Params.EditorPlaySettings->NewWindowWidth = 1920;
    Params.EditorPlaySettings->NewWindowHeight = 1080;
    Params.EditorPlaySettings->CenterNewWindow = true;
    GUnrealEd->RequestPlaySession(Params);
    return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCommandCapture, FAutomationTestBase*, Test);
bool FCommandCapture::Update()
{
    TArray<FColor> Pixels;
    FIntVector Size = FIntVector::ZeroValue;
    const TSharedPtr<SViewport> Viewport = GEngine->GameViewport ? GEngine->GameViewport->GetGameViewportWidget() : nullptr;
    if (!Viewport || !FSlateApplication::Get().TakeScreenshot(Viewport.ToSharedRef(), Pixels, Size))
    {
        Test->AddError(TEXT("PIE window screenshot unavailable"));
        return true;
    }
    TArray64<uint8> Png;
    FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
    Test->TestTrue(TEXT("PIE screenshot saved"), FFileHelper::SaveArrayToFile(Png, *(FPaths::ProjectSavedDir() / TEXT("CommandQA/command-shell.png"))));
    Test->AddInfo(FString::Printf(TEXT("PIE screenshot dimensions: %dx%d"), Size.X, Size.Y));
    Test->TestEqual(TEXT("viewport width"), Size.X, 1920);
    Test->TestEqual(TEXT("viewport height"), Size.Y, 1080);
    return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCommandClientCheck, FAutomationTestBase*, Test);
bool FCommandClientCheck::Update()
{
    FAutomationTestExecutionInfo BeforeChecks;
    Test->GetExecutionInfo(BeforeChecks);
    UWorld* World = nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
        if (Context.WorldType == EWorldType::PIE) { World = Context.World(); break; }
    if (!World) { Test->AddError(TEXT("PIE world not created")); return true; }
    ADroneOpsPlayerController* PC = Cast<ADroneOpsPlayerController>(World->GetFirstPlayerController());
    if (!PC) { Test->AddError(TEXT("CesiumWorld did not create DroneOpsPlayerController")); return true; }
    UCommandScreenManager* Manager = PC->GetCommandScreenManager();
    if (!Manager || !Manager->GetShell()) { Test->AddError(TEXT("CommandShell missing")); return true; }
    UDroneRegistrySubsystem* Registry = Manager->GetRegistry();
    UDroneNetworkManager* Network = PC->GetGameInstance()->GetSubsystem<UDroneNetworkManager>();
    UCommandAlertStore* Store = PC->GetGameInstance()->GetSubsystem<UCommandAlertStore>();
    Test->TestNotNull(TEXT("single registry"), Registry);
    if (!Registry || !Network || !Store) return true;
    if (FParse::Param(FCommandLine::Get(), TEXT("CommandBackendQA")))
    {
        FDroneDescriptor Desc;
        FDroneTelemetrySnapshot Telemetry;
        Test->TestTrue(TEXT("HTTP registry from loopback backend"), Registry->GetDroneDescriptor(3, Desc) && Desc.Name == TEXT("QA UAV-3"));
        Test->TestTrue(TEXT("real backend WebSocket connected"), Network->GetWebSocketClient() && Network->GetWebSocketClient()->IsConnected());
        Test->TestTrue(TEXT("UDP to backend to WebSocket to Registry telemetry"), Registry->GetTelemetry(3, Telemetry)
            && Telemetry.Availability == EDroneAvailability::Online && Telemetry.bGpsFix);
        Test->TestTrue(TEXT("real backend low battery alert"), Store->GetAlerts().ContainsByPredicate([](const FCommandAlert& Alert) { return Alert.DroneId == 3 && Alert.Type == TEXT("low_battery"); }));
    }
    const bool bWasIsolated = Network->IsStrictLocalPreviewIsolation();
    Network->SetStrictLocalPreviewIsolation(true); // UI fixtures never dispatch to hardware.
    const TArray<int32> OldSelection = Registry->GetMultiSelectedDrones();
    const int32 OldPrimary = Registry->GetPrimarySelectedDrone();
    for (int32 Id : {9001, 9002})
    {
        FDroneDescriptor Desc; Desc.DroneId = Id; Desc.Name = FString::Printf(TEXT("TEST UAV-%d"), Id);
        Registry->RegisterDrone(Desc);
        FDroneTelemetrySnapshot Telemetry; Telemetry.DroneId = Id; Telemetry.Availability = EDroneAvailability::Offline;
        Telemetry.BatteryPercent = Id == 9001 ? 64 : 82;
        Registry->UpdateTelemetry(Id, Telemetry);
    }
    UDroneListWidget* List = nullptr;
    Manager->GetShell()->WidgetTree->ForEachWidget([&](UWidget* Widget) { if (auto* Candidate = Cast<UDroneListWidget>(Widget)) List = Candidate; });
    Test->TestNotNull(TEXT("reused DroneListWidget"), List);
    UDroneListItemWidget* Row = nullptr;
    if (List)
    {
        List->RefreshFromRegistry();
        for (UWidget* Child : List->DroneScrollBox->GetAllChildren())
            if (auto* Item = Cast<UDroneListItemWidget>(Child); Item && Item->GetDroneId() == 9001) Row = Item;
    }
    Test->TestNotNull(TEXT("registry fixture becomes list row"), Row);
    if (Row)
    {
        UButton* SelectButton = Cast<UButton>(Row->WidgetTree->FindWidget(TEXT("SelectButton")));
        Test->TestNotNull(TEXT("real row selection button"), SelectButton);
        if (SelectButton) SelectButton->OnClicked.Broadcast();
        Test->TestEqual(TEXT("list selection reaches registry"), Registry->GetPrimarySelectedDrone(), 9001);
    }
    Test->TestTrue(TEXT("map adapter selects"), Manager->GetMapService()->SelectDrone(9002));
    Test->TestEqual(TEXT("map uses same primary"), Registry->GetPrimarySelectedDrone(), 9002);
    Test->TestTrue(TEXT("single selection clears previous"), !Registry->IsDroneSelected(9001) && Registry->IsDroneSelected(9002));
    if (Row)
    {
        const UButton* RowButton = Cast<UButton>(Row->WidgetTree->FindWidget(TEXT("SelectButton")));
        const UTextBlock* RowLabel = RowButton ? Cast<UTextBlock>(RowButton->GetContent()) : nullptr;
        Test->TestNotNull(TEXT("row selection label"), RowLabel);
        if (RowLabel) Test->TestEqual(TEXT("map selection updates previous list row"), RowLabel->GetText().ToString(), FString(TEXT("选择无人机")));
    }
    Test->TestNotNull(TEXT("existing drone actor spawned"), Registry->GetSenderPawn(9002));
    Test->TestTrue(TEXT("existing focus API"), Manager->GetMapService()->FocusDrone(9002));
    const int32 AlertCount = Store->GetAlerts().Num();
    Network->OnDroneWsAlert.Broadcast(9001, TEXT("low_battery"), 19);
    Test->TestEqual(TEXT("network alert enters store"), Store->GetAlerts().Num(), FMath::Min(AlertCount + 1, UCommandAlertStore::MaxAlerts));
    const int32 AlertId = Store->GetAlerts().Last().Id;
    Test->TestTrue(TEXT("alert selects drone"), Manager->GetMapService()->FocusAlertOnMap(AlertId));
    Test->TestEqual(TEXT("alert selection"), Registry->GetPrimarySelectedDrone(), 9001);
    Manager->Show(); Manager->Show();
    Test->TestNotNull(TEXT("idempotent show"), Manager->GetShell());
    Manager->Hide(); Manager->Show();
    Test->TestTrue(TEXT("alert survives hide/show"), Store->GetAlerts().ContainsByPredicate([AlertId](const FCommandAlert& Alert) { return Alert.Id == AlertId; }));
    Test->TestTrue(TEXT("path entry uses existing editor"), Manager->GetMapService()->BeginPathPlanning());
    Test->TestTrue(TEXT("path editor active"), PC->IsPathEditMode());
    Test->TestFalse(TEXT("one-point route cannot dispatch"), Manager->GetMapService()->ConfirmPath());
    const FGeographicDispatchResult Added = PC->AddGeographicWaypointInEditMode(EGeographicCoordinateSystem::WGS84, 121.474, 31.2304, 60.0);
    Test->TestTrue(TEXT("existing coordinate service adds waypoint"), Added.bSuccess);
    Test->TestTrue(TEXT("valid route opens existing confirmation"), Manager->GetMapService()->ConfirmPath());
    TArray<UUserWidget*> Popups;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Popups, UPreviewConfirmPopupWidget::StaticClass(), true);
    Test->TestEqual(TEXT("one original confirmation popup"), Popups.Num(), 1);
    if (!Popups.IsEmpty())
    {
        auto* Popup = Cast<UPreviewConfirmPopupWidget>(Popups[0]);
        if (Popup && Popup->LocalPreviewButton) Popup->LocalPreviewButton->OnClicked.Broadcast();
        bool bPlayback = false;
        // SequenceDispatch plays existing shadow drones through DronePathActor directly.
        for (TActorIterator<ADronePathActor> It(World); It; ++It) bPlayback |= It->bIsMoving;
        Test->TestTrue(TEXT("original local Playback starts"), bPlayback);
    }
    Test->TestTrue(TEXT("cancel existing path editor"), Manager->GetMapService()->CancelPathPlanning());
    Test->TestTrue(TEXT("cancel clears temporary paths"), PC->BuildEditingPathsData().IsEmpty());
    Test->TestFalse(TEXT("isolated UI cannot send pause"), Manager->GetMapService()->SetSelectedPaused(true));
    // Video regression is isolated from Command: exercise unchanged manager directly, no stream/flight claim.
    UDroneVideoWindowManager* Video = NewObject<UDroneVideoWindowManager>(PC);
    UClass* VideoClass = LoadClass<UDroneVideoWindowWidget>(nullptr, TEXT("/Game/DroneOps/UI/WBP_DroneVideoWindow.WBP_DroneVideoWindow_C"));
    Test->TestNotNull(TEXT("original video Blueprint exists"), VideoClass);
    Video->SetWindowContentClass(VideoClass);
    FString Error;
    if (VideoClass)
    {
        Test->TestTrue(TEXT("original video window opens"), Video->OpenVideoWindow(9001, TEXT("Lifecycle test"), TEXT("about:blank"), Error));
        Video->OpenVideoWindow(9001, TEXT("Lifecycle test"), TEXT("about:blank"), Error);
        Test->TestEqual(TEXT("video remains deduplicated"), Video->GetOpenWindowCount(), 1);
        Video->CloseAllVideoWindows(); Video->CloseAllVideoWindows();
        Test->TestEqual(TEXT("video close idempotent"), Video->GetOpenWindowCount(), 0);
    }
    Manager->Destroy();
    Test->TestNull(TEXT("destroy releases shell"), Manager->GetShell());
    Manager->Initialize(PC);
    Manager->ShowPathPanel();
    Manager->ShowGeographicPanel();
    Test->TestNotNull(TEXT("shell recreated"), Manager->GetShell());
    Test->TestTrue(TEXT("alert survives full shell recreation"), Store->GetAlerts().ContainsByPredicate([AlertId](const FCommandAlert& Alert) { return Alert.Id == AlertId; }));
    Store->ClearAlert(AlertId);
    for (int32 Id : {9001, 9002}) if (APawn* FixturePawn = Registry->GetSenderPawn(Id)) FixturePawn->Destroy();
    Registry->UnregisterDrone(9001); Registry->UnregisterDrone(9002);
    Registry->SetMultiSelectedDrones(OldSelection);
    if (OldPrimary > 0) Registry->SetPrimarySelectedDrone(OldPrimary);
    Network->SetStrictLocalPreviewIsolation(bWasIsolated);
    if (FParse::Param(FCommandLine::Get(), TEXT("CommandBackendQA"))) Manager->GetMapService()->SelectDrone(3);
    Manager->GetShell()->Refresh();
    FAutomationTestExecutionInfo AfterChecks;
    Test->GetExecutionInfo(AfterChecks);
    Test->AddInfo(FString::Printf(TEXT("Command synchronous checks completed; new errors=%d"), AfterChecks.GetErrorTotal() - BeforeChecks.GetErrorTotal()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommandClientIntegrationTest, "DroneOps.Command.CesiumWorld",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommandClientIntegrationTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Level/CesiumWorld")));
    ADD_LATENT_AUTOMATION_COMMAND(FCommandStartPIE());
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(12.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FCommandClientCheck(this));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FCommandCapture(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
#endif
