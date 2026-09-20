#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Command/CommandScreenManager.h"
#include "Map/MapShellWidget.h"
#include "Command/CommandMapInteractionService.h"
#include "Command/CommandAlertStore.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Core/ICoordinateService.h"
#include "DroneOps/Network/DroneNetworkManager.h"
#include "PathEditor/DronePathActor.h"
#include "PathEditor/DroneWaypointActor.h"
#include "Components/SplineMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "UI/SequenceDispatchPanelWidget.h"
#include "UI/PreviewConfirmPopupWidget.h"
#include "UI/GeographicTargetPanelWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "CesiumIonRasterOverlay.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

DEFINE_LATENT_AUTOMATION_COMMAND(FA2StartPIE);
bool FA2StartPIE::Update()
{
    FRequestPlaySessionParams Params;
    Params.EditorPlaySettings = DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage());
    Params.EditorPlaySettings->NewWindowWidth = 1920;
    Params.EditorPlaySettings->NewWindowHeight = 1080;
    Params.EditorPlaySettings->CenterNewWindow = true;
    GUnrealEd->RequestPlaySession(Params);
    return true;
}

class FA2SwitchCheck : public IAutomationLatentCommand
{
public:
    explicit FA2SwitchCheck(FAutomationTestBase* InTest) : Test(InTest) {}
    virtual bool Update() override
    {
        if (FPlatformTime::Seconds() < NextTime) return false;
        if (Stage == 0)
        {
            UWorld* World = nullptr;
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
                if (Context.WorldType == EWorldType::PIE) World = Context.World();
            PC = World ? Cast<ADroneOpsPlayerController>(World->GetFirstPlayerController()) : nullptr;
            Manager = PC ? PC->GetCommandScreenManager() : nullptr;
            if (!Manager || !Manager->GetMapService() || !Manager->GetMapShell()) { Test->AddError(TEXT("A2 Map startup missing")); return true; }
            Map = Manager->GetMapService(); Registry = Manager->GetRegistry();
            Network = PC->GetGameInstance()->GetSubsystem<UDroneNetworkManager>();
            Store = PC->GetGameInstance()->GetSubsystem<UCommandAlertStore>();
            if (!Registry || !Network || !Store) { Test->AddError(TEXT("A2 business services missing")); return true; }
            Test->TestEqual(TEXT("default is 2D"), Map->GetMapMode(), ECommandMapMode::Map2D);
            Test->TestTrue(TEXT("2D camera exists"), Map->GetMapCamera() != nullptr);
            bool bActiveBasemap = false;
            for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
            {
                TArray<UCesiumIonRasterOverlay*> Rasters;
                It->GetComponents(Rasters);
                for (const auto* Raster : Rasters)
                    if (Raster->IonAssetID == 2 && Raster->IsActive()) bActiveBasemap = true;
            }
            Test->TestTrue(TEXT("default online basemap auto activation survives early mode initialization"), bActiveBasemap);
            OldSelection = Registry->GetMultiSelectedDrones(); OldPrimary = Registry->GetPrimarySelectedDrone();
            WasIsolated = Network->IsStrictLocalPreviewIsolation(); Network->SetStrictLocalPreviewIsolation(true);
            FDroneDescriptor Desc; Desc.DroneId = 9011; Desc.Name = TEXT("A2 REGRESSION FIXTURE");
            Registry->RegisterDrone(Desc);
            Map->SelectDrone(9011);
            Map->FocusDrone(9011);
            Map->ZoomView(-20.f);
            Test->TestTrue(TEXT("zoomed-out cursor ray reaches beyond map focus"), PC->HitResultTraceDistance
                > FVector::Distance(Map->GetMapCamera()->GetActorLocation(), Registry->GetSenderPawn(9011)->GetActorLocation()) * 2.0);
            Map->ZoomView(20.f);
            Test->TestTrue(TEXT("near vertical 2D camera"), Map->GetMapCamera() && Map->GetMapCamera()->GetActorRotation().Pitch < -89.f);
            const FVector BeforePan = Map->GetMapCamera()->GetActorLocation();
            Map->PanView(FVector2D(15, 10));
            Test->TestFalse(TEXT("pan moves camera"), Map->GetMapCamera()->GetActorLocation().Equals(BeforePan));
            const FVector BeforeZoom = Map->GetMapCamera()->GetActorLocation();
            Map->ZoomView(1.f);
            Test->TestFalse(TEXT("zoom moves camera"), Map->GetMapCamera()->GetActorLocation().Equals(BeforeZoom));
            const FRotator BeforeRotate = Map->GetMapCamera()->GetActorRotation();
            Map->RotateView(FVector2D(30, 30));
            Test->TestTrue(TEXT("2D rejects orbit"), Map->GetMapCamera()->GetActorRotation().Equals(BeforeRotate));
            auto* Panel = Manager->ShowPathPanel();
            Test->TestTrue(TEXT("legacy sequence retained but hidden"), Panel && !Panel->IsInViewport() && !Panel->IsVisible());
            Test->TestTrue(TEXT("begin existing path editor"), Map->BeginPathPlanning());
            const auto Coordinates = Registry->GetCoordinateService();
            const FVector OriginGps = ICoordinateService::Execute_WorldToGeographic(Coordinates.GetObject(), Registry->GetSenderPawn(9011)->GetActorLocation());
            // Cesium returns (longitude, latitude, height); fixtures stay near this world's actual origin.
            Test->AddInfo(FString::Printf(TEXT("A2 fixture lon=%.6f lat=%.6f height=%.3f"), OriginGps.X, OriginGps.Y, OriginGps.Z));
            for (int32 Index = 1; Index <= 3; ++Index)
            {
                const auto Added = PC->AddGeographicWaypointInEditMode(EGeographicCoordinateSystem::WGS84, OriginGps.X + Index * .0002, OriginGps.Y + (Index % 2) * .0002, OriginGps.Z);
                Test->TestTrue(*FString::Printf(TEXT("add geographic waypoint: %s"), *Added.Message), Added.bSuccess);
            }
            Test->TestTrue(TEXT("existing confirmation"), Map->ConfirmPath());
            TArray<UUserWidget*> Popups;
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Popups, UPreviewConfirmPopupWidget::StaticClass(), true);
            Test->TestEqual(TEXT("one confirmation"), Popups.Num(), 1);
            if (!Popups.IsEmpty())
                if (auto* Popup = Cast<UPreviewConfirmPopupWidget>(Popups[0]); Popup && Popup->LocalPreviewButton) Popup->LocalPreviewButton->OnClicked.Broadcast();
            for (TActorIterator<ADronePathActor> It(World); It; ++It)
                if (It->bIsMoving) PlaybackPaths.Add(*It);
            Test->TestFalse(TEXT("playback started"), PlaybackPaths.IsEmpty());
            Paths = PC->BuildEditingPathsData();
            Pawn = Registry->GetSenderPawn(9011);
            CoordinateService = Registry->GetCoordinateService().GetObject();
            Count = Registry->GetAllDroneDescriptors().Num();
            FDroneTaskStateSnapshot Mission; Mission.ArrayId = TEXT("A2-REGRESSION-MISSION");
            Registry->UpdateTaskState(9011, Mission);
            Network->OnDroneWsAlert.Broadcast(9011, TEXT("low_battery"), 19);
            AlertId = Store->GetAlerts().Last().Id;
            Test->TestTrue(TEXT("alert selects and focuses"), Map->FocusAlertOnMap(AlertId));
            ClickMode(TEXT("map3d"));
            Stage = 1; NextTime = FPlatformTime::Seconds() + .5; return false;
        }
        VerifyState();
        if (Stage == 1)
        {
            Test->TestEqual(TEXT("3D toolbar reaches service"), Map->GetMapMode(), ECommandMapMode::Map3D);
            Test->TestTrue(TEXT("3D camera pitch"), Map->GetMapCamera()->GetActorRotation().Pitch > -89.f);
            const FRotator Before = Map->GetMapCamera()->GetActorRotation();
            Map->RotateView(FVector2D(20, 10));
            Test->TestFalse(TEXT("3D orbit enabled"), Map->GetMapCamera()->GetActorRotation().Equals(Before));
            Capture(TEXT("3d"));
            Manager->ShowGeographicPanel(true);
            ClickMode(TEXT("map2d")); Stage = 2; NextTime = FPlatformTime::Seconds() + .5; return false;
        }
        Test->TestEqual(TEXT("2D toolbar reaches service"), Map->GetMapMode(), ECommandMapMode::Map2D);
        Test->TestTrue(TEXT("returned to top-down"), Map->GetMapCamera()->GetActorRotation().Pitch < -89.f);
        TArray<UUserWidget*> GeographicWidgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PC->GetWorld(), GeographicWidgets, UGeographicTargetPanelWidget::StaticClass(), false);
        Test->TestEqual(TEXT("one retained geographic input widget"), GeographicWidgets.Num(), 1);
        if (!GeographicWidgets.IsEmpty())
        {
            UWidget* Body = GeographicWidgets[0]->WidgetTree->FindWidget(TEXT("PanelBody"));
            const auto* Slot = Body ? Cast<UCanvasPanelSlot>(Body->Slot) : nullptr;
            Test->TestTrue(TEXT("compact geographic size survives NativeTick"), Slot && Slot->GetSize().Equals(FVector2D(480, 420)));
            Manager->ToggleGeographicPanel();
            Test->TestFalse(TEXT("same geographic entry can dismiss drawer"), GeographicWidgets[0]->IsVisible());
            Manager->TogglePathTools();
            Manager->ToggleGeographicPanel();
            Test->TestFalse(TEXT("geographic drawer hides sequence presentation"), Manager->ShowPathPanel()->IsVisible());
            Manager->ToggleGeographicPanel();
            VerifyState();
        }
        Capture(TEXT("2d"));
        Map->LogLayerState();
        for (TActorIterator<ACesium3DTileset> It(PC->GetWorld()); It; ++It)
            if (It->GetIonAssetID() == 2275207 || It->GetIonAssetID() == 96188)
                Test->TestTrue(TEXT("2D heavy layer disabled and network source detached"), It->SuspendUpdate && It->IsHidden() && It->GetTilesetSource() == ETilesetSource::FromEllipsoid);
        Test->TestTrue(TEXT("cancel existing path editor"), Map->CancelPathPlanning());
        Store->ClearAlert(AlertId);
        if (Pawn.IsValid()) Pawn->Destroy();
        Registry->UnregisterDrone(9011);
        Registry->SetPrimarySelectedDrone(OldPrimary);
        Registry->SetMultiSelectedDrones(OldSelection);
        Network->SetStrictLocalPreviewIsolation(WasIsolated);
        Test->AddInfo(TEXT("A2 regression finished; toolbar events are automation, physical mouse is separately verified."));
        return true;
    }
private:
    void ClickMode(const TCHAR* Name)
    {
        UButton* Button = Cast<UButton>(Manager->GetMapShell()->WidgetTree->FindWidget(FName(Name)));
        Test->TestNotNull(TEXT("map toolbar button"), Button);
        if (Button) Button->OnClicked.Broadcast();
    }
    void VerifyState()
    {
        Test->TestEqual(TEXT("same primary"), Registry->GetPrimarySelectedDrone(), 9011);
        Test->TestEqual(TEXT("same registry"), Manager->GetRegistry(), Registry);
        Test->TestEqual(TEXT("same coordinate service"), Registry->GetCoordinateService().GetObject(), CoordinateService);
        Test->TestEqual(TEXT("same drone count"), Registry->GetAllDroneDescriptors().Num(), Count);
        Test->TestEqual(TEXT("same drone actor"), Registry->GetSenderPawn(9011), Pawn.Get());
        Test->TestTrue(TEXT("same alert"), Store->GetAlerts().ContainsByPredicate([&](const FCommandAlert& A) { return A.Id == AlertId; }));
        FDroneTaskStateSnapshot Mission;
        Test->TestTrue(TEXT("same mission"), Registry->GetTaskState(9011, Mission) && Mission.ArrayId == TEXT("A2-REGRESSION-MISSION"));
        const auto Now = PC->BuildEditingPathsData();
        Test->TestEqual(TEXT("same path count"), Now.Num(), Paths.Num());
        for (const auto& Pair : Paths)
        {
            const auto* Current = Now.Find(Pair.Key);
            Test->TestTrue(TEXT("same route ID and waypoint count"), Current && Current->PathId == Pair.Value.PathId && Current->Waypoints.Num() == Pair.Value.Waypoints.Num());
            if (Current && Current->Waypoints.Num() == Pair.Value.Waypoints.Num())
                for (int32 I = 0; I < Current->Waypoints.Num(); ++I)
                    Test->TestTrue(TEXT("waypoint order position and speed unchanged"), Current->Waypoints[I].Location.Equals(Pair.Value.Waypoints[I].Location)
                        && Current->Waypoints[I].SegmentSpeed == Pair.Value.Waypoints[I].SegmentSpeed);
        }
        for (const auto& Path : PlaybackPaths)
        {
            Test->TestTrue(TEXT("same playback actor continues across ticks"), Path.IsValid() && Path->bIsMoving);
            if (!Path.IsValid()) continue;
            TArray<USplineMeshComponent*> Segments;
            Path->GetComponents(Segments);
            Test->TestFalse(TEXT("route still has render segments"), Segments.IsEmpty());
            for (const auto* Segment : Segments)
                Test->TestTrue(TEXT("GIS route width exceeds subpixel authored width"), Segment->GetStartScale().X > Path->PathSplineThickness / 50.f);
            const auto& Handles = Path->GetWaypointHandleActors();
            Test->TestFalse(TEXT("route still has waypoint handles"), Handles.IsEmpty());
            for (const ADroneWaypointActor* Handle : Handles)
                if (Handle && Handle->TextRenderComponent)
                    Test->TestTrue(TEXT("waypoint text faces camera including pitch"), FVector::DotProduct(
                        Handle->TextRenderComponent->GetForwardVector(), -PC->PlayerCameraManager->GetCameraRotation().Vector()) > .99);
        }
    }
    void Capture(const TCHAR* Name)
    {
        const auto Viewport = GEngine->GameViewport ? GEngine->GameViewport->GetGameViewportWidget() : nullptr;
        TArray<FColor> Pixels; FIntVector Size(0);
        if (!Viewport || !FSlateApplication::Get().TakeScreenshot(Viewport.ToSharedRef(), Pixels, Size)) { Test->AddError(TEXT("A2 capture unavailable")); return; }
        TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
        const FString Dir = FPaths::ProjectSavedDir() / TEXT("CommandQA/A2");
        IFileManager::Get().MakeDirectory(*Dir, true);
        Test->TestTrue(TEXT("A2 screenshot saved"), FFileHelper::SaveArrayToFile(Png, *(Dir / FString::Printf(TEXT("%s.png"), Name))));
        Test->AddInfo(FString::Printf(TEXT("A2 %s capture %dx%d; original Command.CesiumWorld retains exact 1080 height assertion"), Name, Size.X, Size.Y));
    }
    FAutomationTestBase* Test;
    ADroneOpsPlayerController* PC = nullptr;
    UCommandScreenManager* Manager = nullptr;
    UCommandMapInteractionService* Map = nullptr;
    UDroneRegistrySubsystem* Registry = nullptr;
    UDroneNetworkManager* Network = nullptr;
    UCommandAlertStore* Store = nullptr;
    UObject* CoordinateService = nullptr;
    TWeakObjectPtr<APawn> Pawn;
    TArray<TWeakObjectPtr<ADronePathActor>> PlaybackPaths;
    TMap<int32, FDronePathSaveData> Paths;
    TArray<int32> OldSelection;
    int32 OldPrimary = 0, Count = 0, AlertId = 0, Stage = 0;
    bool WasIsolated = false;
    double NextTime = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommandMapModeTest, "DroneOps.Command.A2MapMode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommandMapModeTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Level/CesiumWorld")));
    ADD_LATENT_AUTOMATION_COMMAND(FA2StartPIE());
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(12.f));
    ADD_LATENT_AUTOMATION_COMMAND(FA2SwitchCheck(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
