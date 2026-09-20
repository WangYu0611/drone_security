#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandShellWidget.h"
#include "Command/CommandActionButton.h"
#include "Command/CommandAlertStore.h"
#include "Map/MapShellWidget.h"
#include "Shared/DroneMissionService.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Network/DroneNetworkManager.h"
#include "UI/UIManagerBlueprintLibrary.h"
#include "UI/DroneListWidget.h"
#include "UI/DroneListItemWidget.h"
#include "UI/SequenceDispatchPanelWidget.h"
#include "UI/GeographicTargetPanelWidget.h"
#include "UI/DefaultSegmentSpeedWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FM1CommandCheck, FAutomationTestBase*, Test);
bool FM1CommandCheck::Update()
{
    UWorld* World = nullptr;
    for (const auto& C : GEngine->GetWorldContexts()) if (C.WorldType == EWorldType::PIE) World = C.World();
    auto* PC = World ? Cast<ADroneOpsPlayerController>(World->GetFirstPlayerController()) : nullptr;
    auto* M = PC ? PC->GetCommandScreenManager() : nullptr;
    if (!M || M->GetClientRole() != EDroneClientRole::Command || !M->GetShell())
    { Test->AddError(TEXT("Run M1.CommandRegression with -ClientRole=Command")); return true; }
    auto Count = [&](UClass* Class)
    {
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, Class, false); return Widgets.Num();
    };
    Test->TestNull(TEXT("no map interaction service"), M->GetMapService());
    Test->TestNull(TEXT("no MapShell"), M->GetMapShell());
    Test->TestNull(TEXT("no VideoShell"), M->GetVideoShell());
    auto* Shell = M->GetShell(); M->Initialize(PC); M->Show();
    Test->TestTrue(TEXT("idempotent CommandShell"), Shell == M->GetShell());
    Test->TestEqual(TEXT("one CommandShell"), Count(UCommandShellWidget::StaticClass()), 1);
    Test->TestTrue(TEXT("Command does not render scene"), World->GetGameViewport()->bDisableWorldRendering);
    int32 Sources = 0;
    for (TActorIterator<ACesium3DTileset> It(World); It; ++It) ++Sources;
    for (TActorIterator<ACesiumGeoreference> It(World); It; ++It) ++Sources;
    Test->TestEqual(TEXT("zero Cesium sources/georeferences in Command world"), Sources, 0);
    UUIManagerBlueprintLibrary::ShowSequenceDispatchPanel(PC);
    UUIManagerBlueprintLibrary::ShowGeographicTargetPanel(PC);
    M->TogglePathTools(); M->ToggleSpeedPanel(); M->ShowGeographicPanel(true);
    Test->TestEqual(TEXT("no path editor panel even via legacy facade"), Count(USequenceDispatchPanelWidget::StaticClass()), 0);
    Test->TestEqual(TEXT("no Geographic drawer"), Count(UGeographicTargetPanelWidget::StaticClass()), 0);
    Test->TestEqual(TEXT("no Speed drawer"), Count(UDefaultSegmentSpeedWidget::StaticClass()), 0);
    Test->TestFalse(TEXT("no map input surface"), M->IsCursorOverMap());
    for (const TCHAR* Name : {TEXT("map2d"), TEXT("map3d"), TEXT("plan"), TEXT("confirm"), TEXT("sequence"), TEXT("speed")})
        Test->TestNull(TEXT("map control never constructed"), Shell->WidgetTree->FindWidget(FName(Name)));
    auto* R = M->GetRegistry(); auto* N = PC->GetGameInstance()->GetSubsystem<UDroneNetworkManager>();
    auto* Store = PC->GetGameInstance()->GetSubsystem<UCommandAlertStore>();
    const bool WasIsolated = N->IsStrictLocalPreviewIsolation(); N->SetStrictLocalPreviewIsolation(true);
    const auto OldMulti = R->GetMultiSelectedDrones(); const int32 OldPrimary = R->GetPrimarySelectedDrone();
    FDroneDescriptor D; D.DroneId = 9021; D.Name = TEXT("M1 QA UAV"); R->RegisterDrone(D);
    FDroneTelemetrySnapshot T; T.DroneId = D.DroneId; T.BatteryPercent = 64; R->UpdateTelemetry(D.DroneId, T);
    FDroneTaskStateSnapshot Mission; Mission.ArrayId = TEXT("M1 QA MISSION"); R->UpdateTaskState(D.DroneId, Mission);
    Test->TestNull(TEXT("Command descriptor has no spawned drone pawn"), R->GetSenderPawn(D.DroneId));
    UDroneListWidget* List = nullptr;
    Shell->WidgetTree->ForEachWidget([&](UWidget* W) { if (auto* L = Cast<UDroneListWidget>(W)) List = L; });
    Test->TestNotNull(TEXT("Fleet list"), List);
    if (List)
    {
        List->RefreshFromRegistry();
        for (auto* W : List->DroneScrollBox->GetAllChildren())
            if (auto* Row = Cast<UDroneListItemWidget>(W); Row && Row->GetDroneId() == D.DroneId)
                if (auto* B = Cast<UButton>(Row->WidgetTree->FindWidget(TEXT("SelectButton")))) B->OnClicked.Broadcast();
    }
    Test->TestEqual(TEXT("Command list selects independently"), R->GetPrimarySelectedDrone(), D.DroneId);
    Shell->Refresh(); FString Text;
    Shell->WidgetTree->ForEachWidget([&](UWidget* W) { if (auto* B = Cast<UTextBlock>(W)) Text += B->GetText().ToString(); });
    Test->TestTrue(TEXT("Tactical selected data"), Text.Contains(D.Name) && Text.Contains(TEXT("64%")));
    Test->TestTrue(TEXT("Mission data"), Text.Contains(Mission.ArrayId));
    const int32 Before = Store->GetAlerts().Num(); N->OnDroneWsAlert.Broadcast(D.DroneId, TEXT("low_battery"), 19);
    Test->TestEqual(TEXT("Alert event projected"), Store->GetAlerts().Num(), FMath::Min(Before + 1, UCommandAlertStore::MaxAlerts));
    Test->TestNotNull(TEXT("pause mission control"), Shell->WidgetTree->FindWidget(TEXT("pause")));
    Test->TestNotNull(TEXT("resume mission control"), Shell->WidgetTree->FindWidget(TEXT("resume")));
    auto* MissionService = PC->GetGameInstance()->GetSubsystem<UDroneMissionService>();
    Test->TestFalse(TEXT("isolated mission request remains gated without a map"), MissionService->SetSelectedPaused(true));
    Store->ClearAlert(Store->GetAlerts().Last().Id); R->UnregisterDrone(D.DroneId);
    R->SetMultiSelectedDrones(OldMulti); if (OldPrimary > 0) R->SetPrimarySelectedDrone(OldPrimary); else R->ClearSelection();
    N->SetStrictLocalPreviewIsolation(WasIsolated);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM1CommandRegression, "DroneOps.M1.CommandRegression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FM1CommandRegression::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Level/CesiumWorld")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FM1CommandCheck(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
