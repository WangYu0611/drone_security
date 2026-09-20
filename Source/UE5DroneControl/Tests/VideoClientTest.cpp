#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandShellWidget.h"
#include "Video/VideoShellWidget.h"
#include "Map/MapShellWidget.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Network/DroneNetworkManager.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "WebBrowser.h"
#include "UI/UIManagerBlueprintLibrary.h"
#include "UI/DroneListWidget.h"
#include "UI/DroneVideoWindowWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVideoRoleCheck, FAutomationTestBase*, Test);
bool FVideoRoleCheck::Update()
{
    UWorld* World = nullptr;
    for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::PIE) World = Context.World();
    if (!World) { Test->AddError(TEXT("No PIE world")); return true; }
    auto* PC = Cast<ADroneOpsPlayerController>(World->GetFirstPlayerController());
    if (!PC || !PC->GetCommandScreenManager()) { Test->AddError(TEXT("Role manager not started")); return true; }
    auto* Manager = PC->GetCommandScreenManager();
    const auto Role = UCommandScreenManager::ResolveClientRole();
    Test->TestEqual(TEXT("startup role"), Manager->GetClientRole(), Role);
    auto Count = [&](UClass* Class)
    {
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, Class, true);
        return Widgets.Num();
    };
    if (Role == EDroneClientRole::Map)
    {
        auto* Shell = Manager->GetMapShell();
        Test->TestNotNull(TEXT("MapShell exists"), Shell);
        if (Shell) Test->TestNotNull(TEXT("Map selection overlay exists"), Shell->WidgetTree->FindWidget(TEXT("MapSelectionHighlight")));
        Manager->Initialize(PC); Manager->Show(); Manager->Show();
        Test->TestTrue(TEXT("same MapShell after repeated Initialize/Show"), Shell == Manager->GetMapShell());
        Test->TestEqual(TEXT("unique MapShell"), Count(UMapShellWidget::StaticClass()), 1);
        Test->TestEqual(TEXT("no CommandShell in Map"), Count(UCommandShellWidget::StaticClass()), 0);
        Test->TestEqual(TEXT("no VideoShell in Map"), Count(UVideoShellWidget::StaticClass()), 0);
        return true;
    }
    Test->TestEqual(TEXT("no MapShell in other roles"), Count(UMapShellWidget::StaticClass()), 0);
    if (Role != EDroneClientRole::Video)
    {
        Test->TestNotNull(TEXT("Command/Standalone existing Shell"), Manager->GetShell());
        Test->TestEqual(TEXT("no Video Shell in Command/Standalone"), Count(UVideoShellWidget::StaticClass()), 0);
        Test->AddInfo(TEXT("Command/Standalone physical PIE startup checked"));
        return true;
    }
    auto* Shell = Manager->GetVideoShell();
    if (!Shell) { Test->AddError(TEXT("Video Shell missing")); return true; }
    Test->TestNull(TEXT("no Command Shell"), Manager->GetShell());
    Test->TestNull(TEXT("no Command map service"), Manager->GetMapService());
    Manager->Initialize(PC); Manager->Show(); Manager->Show();
    Test->TestTrue(TEXT("initialize/show retain same Shell"), Shell == Manager->GetVideoShell());
    Test->TestEqual(TEXT("unique Video Shell"), Count(UVideoShellWidget::StaticClass()), 1);
    int32 Browsers = 0;
    Shell->WidgetTree->ForEachWidget([&](UWidget* W) { if (Cast<UWebBrowser>(W)) ++Browsers; });
    Test->TestEqual(TEXT("one browser in shell"), Browsers, 1);
    UUIManagerBlueprintLibrary::ShowDroneList(PC);
    Test->TestEqual(TEXT("legacy facade cannot create DroneList"), Count(UDroneListWidget::StaticClass()), 0);
    Test->TestEqual(TEXT("no legacy video windows"), Count(UDroneVideoWindowWidget::StaticClass()), 0);
    auto* Registry = PC->GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    auto* Network = PC->GetGameInstance()->GetSubsystem<UDroneNetworkManager>();
    const bool WasIsolated = Network->IsStrictLocalPreviewIsolation();
    Network->SetStrictLocalPreviewIsolation(true);
    const int32 OldPrimary = Registry->GetPrimarySelectedDrone();
    const auto OldMulti = Registry->GetMultiSelectedDrones();
    const auto SavedDescriptors = Registry->GetFriendlyDroneDescriptors();
    for (const auto& D : SavedDescriptors) Registry->UnregisterDrone(D.DroneId);
    Shell->Refresh();
    Test->TestEqual(TEXT("empty registry has no source"), Shell->GetSourceUrl(), FString());
    Test->TestEqual(TEXT("empty registry has no primary"), Shell->GetDisplayedDroneId(), 0);
    for (int32 Id : {9001, 9002, 9003, 9004, 9005, 9006})
    {
        FDroneDescriptor D; D.DroneId = Id; D.Name = TEXT("Video test fixture");
        D.VideoUrl = Id == 9001 ? TEXT("about:blank#one") : TEXT("about:blank#two");
        if (Id == 9006) D.VideoUrl.Empty();
        Registry->RegisterDrone(D);
    }
    Registry->SetPrimarySelectedDrone(9001); Shell->Refresh();
    Test->TestEqual(TEXT("registry selects source one"), Shell->GetSourceUrl(), FString(TEXT("about:blank#one")));
    auto* AircraftButton = Shell->GetAircraftButton(9002);
    if (!AircraftButton) { Test->AddError(TEXT("Aircraft row missing")); return true; }
    Test->TestTrue(TEXT("no invented aircraft model"), AircraftButton->Label->GetText().ToString().Contains(TEXT("MODEL N/A")));
    AircraftButton->OnClicked.Broadcast();
    Test->TestEqual(TEXT("real selector writes shared selection"), Registry->GetPrimarySelectedDrone(), 9002);
    Test->TestEqual(TEXT("selector changes video source"), Shell->GetSourceUrl(), FString(TEXT("about:blank#two")));
    Test->TestEqual(TEXT("six positions by default"), Shell->GetViewMode(), 6);
    const int32 SecondaryId = Shell->GetSecondaryDroneId(0);
    Shell->GetSecondaryButton(0)->OnClicked.Broadcast();
    Test->TestEqual(TEXT("secondary click becomes primary"), Shell->GetDisplayedDroneId(), SecondaryId);
    Test->TestEqual(TEXT("old primary returns to clicked slot"), Shell->GetSecondaryDroneId(0), 9002);
    Shell->SetViewMode(4);
    Test->TestEqual(TEXT("four view hides fifth position"), Shell->GetSecondaryButton(3)->GetVisibility(), ESlateVisibility::Collapsed);
    Shell->SetViewMode(1);
    Test->TestEqual(TEXT("focus hides secondary"), Shell->GetSecondaryButton(0)->GetVisibility(), ESlateVisibility::Collapsed);
    Shell->SetViewMode(6);
    Test->TestEqual(TEXT("six view restores all slots"), Shell->GetSecondaryButton(4)->GetVisibility(), ESlateVisibility::Visible);
    Test->TestTrue(TEXT("layout changes reuse browser"), Shell->GetBrowser() != nullptr);
    Shell->SelectDrone(9006);
    Test->TestEqual(TEXT("no source is explicit"), Shell->GetMediaState(), FString(TEXT("OFFLINE / NO SOURCE")));
    Shell->SelectDrone(9002);
    FDroneDescriptor Updated; Registry->GetDroneDescriptor(9002, Updated); Updated.VideoUrl = TEXT("about:blank#updated");
    Registry->RegisterDrone(Updated); Shell->Refresh();
    Test->TestEqual(TEXT("same UAV updated source"), Shell->GetSourceUrl(), Updated.VideoUrl);
    auto* Browser = Shell->GetBrowser();
    Manager->Hide();
    Test->TestEqual(TEXT("hide stops media"), Shell->GetMediaState(), FString(TEXT("OFFLINE / CLOSED")));
    Manager->Show();
    Test->TestTrue(TEXT("show reuses browser"), Browser == Shell->GetBrowser());
    PC->OpenDroneVideoWindow(9001);
    Test->TestEqual(TEXT("legacy open routes to shell selection"), Registry->GetPrimarySelectedDrone(), 9001);
    Test->TestEqual(TEXT("still one shell"), Count(UVideoShellWidget::StaticClass()), 1);
    Registry->UnregisterDrone(9001); Shell->Refresh();
    Test->TestTrue(TEXT("removed UAV source stops"), Shell->GetSourceUrl() != TEXT("about:blank#one"));
    Manager->Destroy(); Manager->Destroy();
    Test->TestNull(TEXT("destroy releases Video Shell"), Manager->GetVideoShell());
    Test->TestEqual(TEXT("destroy stops browser"), Shell->GetMediaState(), FString(TEXT("OFFLINE / CLOSED")));
    Manager->Initialize(PC);
    Test->TestNotNull(TEXT("shell reopens"), Manager->GetVideoShell());
    Test->TestEqual(TEXT("one reopened shell"), Count(UVideoShellWidget::StaticClass()), 1);
    for (int32 Id : {9001,9002,9003,9004,9005,9006}) Registry->UnregisterDrone(Id);
    for (const auto& D : SavedDescriptors) Registry->RegisterDrone(D);
    Registry->SetMultiSelectedDrones(OldMulti); Registry->SetPrimarySelectedDrone(OldPrimary);
    Network->SetStrictLocalPreviewIsolation(WasIsolated);
    Test->AddInfo(TEXT("Video lifecycle/selection checks complete; about:blank is NOT playback validation"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVideoRoleLifecycleTest, "DroneOps.Video.RoleLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVideoRoleLifecycleTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Level/CesiumWorld")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FVideoRoleCheck(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
