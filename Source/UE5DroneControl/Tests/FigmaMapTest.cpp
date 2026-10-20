#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Map/MapShellWidget.h"
#include "Map/MapMissionRouteWidget.h"
#include "Map/MapFleetInteractionWidget.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandMapInteractionService.h"
#include "Command/CommandActionButton.h"
#include "Shared/FigmaRoleHeaderWidget.h"
#include "Shared/OperationalContextSubsystem.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Editor.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Settings/LevelEditorPlaySettings.h"

DEFINE_LATENT_AUTOMATION_COMMAND(FFigmaMapStartPIE);
bool FFigmaMapStartPIE::Update()
{
    FRequestPlaySessionParams Params;
    Params.EditorPlaySettings=DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(),GetTransientPackage());
    Params.EditorPlaySettings->NewWindowWidth=1920;
    Params.EditorPlaySettings->NewWindowHeight=1080;
    Params.EditorPlaySettings->CenterNewWindow=true;
    GUnrealEd->RequestPlaySession(Params);
    return true;
}

// Backend-acknowledged selection and rendered widget geometry, not native mouse
// or physical-flight evidence. The isolated harness owns the Backend and registry.
class FFigmaMapChecks : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    int32 Step=0,SelectedId=0;
    TArray<int32> GroupSelection;
    double Started=FPlatformTime::Seconds(),Next=0;
    FString Out,VideoBefore;
    FVector2D InitialFramePixels=FVector2D::ZeroVector,OriginalWindowSize=FVector2D::ZeroVector;
    TWeakPtr<SWindow> Window;

    static FVector2D Center(const UWidget* Widget)
    {
        const auto& Geometry=Widget->GetCachedGeometry();
        return Geometry.LocalToAbsolute(Geometry.GetLocalSize()*.5);
    }
    static FVector2D PixelSize(const UWidget* Widget)
    {
        const auto& Geometry=Widget->GetCachedGeometry();
        return Geometry.LocalToAbsolute(Geometry.GetLocalSize())-Geometry.GetAbsolutePosition();
    }
    static FString VideoJson(UOperationalContextSubsystem* Sync)
    {
        FString Json;
        if(Sync->GetVideoView())FJsonSerializer::Serialize(Sync->GetVideoView().ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
        return Json;
    }
    void Boundaries(UMapShellWidget* Shell,const TCHAR* Phase)
    {
        const auto& Geometry=Shell->MapFrame->GetCachedGeometry();
        const FVector2D Size=Geometry.GetLocalSize();
        Test->TestTrue(*FString::Printf(TEXT("%s: blank map accepts input"),Phase),
            Shell->IsPointOverMap(Geometry.LocalToAbsolute(Size*.5)));
        // Test actual transformed screen points, not fixed 1440x810 coordinates.
        Test->TestTrue(*FString::Printf(TEXT("%s: point just inside map edge accepts input"),Phase),
            Shell->IsPointOverMap(Geometry.LocalToAbsolute(FVector2D(4,Size.Y*.3))));
        Test->TestFalse(*FString::Printf(TEXT("%s: point beyond map edge is excluded"),Phase),
            Shell->IsPointOverMap(Geometry.LocalToAbsolute(FVector2D(-4,Size.Y*.3))));
        Test->TestFalse(*FString::Printf(TEXT("%s: point above map is excluded"),Phase),
            Shell->IsPointOverMap(Geometry.LocalToAbsolute(FVector2D(Size.X*.5,-4))));
        Test->TestFalse(*FString::Printf(TEXT("%s: point below map is excluded"),Phase),
            Shell->IsPointOverMap(Geometry.LocalToAbsolute(FVector2D(Size.X*.5,Size.Y+4))));
        Test->TestFalse(*FString::Printf(TEXT("%s: device selector cannot click through"),Phase),
            Shell->IsPointOverMap(Center(Shell->DeviceButton)));
        Test->TestFalse(*FString::Printf(TEXT("%s: camera hint cannot click through"),Phase),
            Shell->IsPointOverMap(Center(Shell->CameraHint)));
        Test->TestFalse(*FString::Printf(TEXT("%s: zoom controls cannot click through"),Phase),
            Shell->IsPointOverMap(Center(Shell->ZoomControls)));
        Test->TestFalse(*FString::Printf(TEXT("%s: header language area cannot click through"),Phase),
            Shell->IsPointOverMap(Shell->FigmaChrome->GetCachedGeometry().LocalToAbsolute(FVector2D(1160,35))));
    }
    void Capture(const TCHAR* Name)
    {
        if(FParse::Param(FCommandLine::Get(),TEXT("NullRHI")))return;
        FSlateApplication::Get().InvalidateAllWidgets(false);
        TArray<FColor> Pixels;FIntVector Size=FIntVector::ZeroValue;
        const auto View=GEngine->GameViewport?GEngine->GameViewport->GetGameViewportWidget():nullptr;
        if(!View || !FSlateApplication::Get().TakeScreenshot(View.ToSharedRef(),Pixels,Size))
        {Test->AddError(TEXT("Figma Map rendered screenshot unavailable"));return;}
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
        Test->TestTrue(TEXT("Map screenshot saved"),FFileHelper::SaveArrayToFile(Png,*(Out/(FString(Name)+TEXT(".png")))));
        Test->AddInfo(FString::Printf(TEXT("Map screenshot %s %dx%d"),Name,Size.X,Size.Y));
    }
public:
    explicit FFigmaMapChecks(FAutomationTestBase* InTest):Test(InTest)
    {
        Out=FPaths::ProjectSavedDir()/TEXT("FigmaMapQA")/FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"));
        IFileManager::Get().MakeDirectory(*Out,true);Test->AddInfo(TEXT("Map evidence: ")+Out);
    }
    ~FFigmaMapChecks()
    {
        if(auto OwnedWindow=Window.Pin();OwnedWindow && OriginalWindowSize.X>0)OwnedWindow->Resize(OriginalWindowSize);
    }
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Started>90){Test->AddError(FString::Printf(TEXT("Figma Map workflow timeout at step %d"),Step));return true;}
        if(Now<Next)return false;
        UWorld* World=nullptr;
        for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::PIE)World=Context.World();
        if(!World)return false;
        auto* PC=Cast<ADroneOpsPlayerController>(World->GetFirstPlayerController());
        auto* Manager=PC?PC->GetCommandScreenManager():nullptr;
        auto* Shell=Manager?Manager->GetMapShell():nullptr;
        if(!Shell || !Manager->GetMapService())return false;
        auto* Sync=World->GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
        auto* Registry=Manager->GetRegistry();
        if(!Sync->IsHydrated() || Registry->GetFriendlyDroneDescriptors().Num()<2)return false;
        Shell->Refresh();
        if(!Shell->MapFrame || Shell->MapFrame->GetCachedGeometry().GetLocalSize().X<100)return false;
        if(Step==0)
        {
            Test->TestEqual(TEXT("Map role owns this shell"),Manager->GetClientRole(),EDroneClientRole::Map);
            Test->TestNull(TEXT("Map creates no Command shell"),Manager->GetShell());
            Test->TestNull(TEXT("Map creates no Video shell"),Manager->GetVideoShell());
            Manager->Show();Manager->Show();
            Test->TestTrue(TEXT("Repeated Show keeps the same Map shell"),Manager->GetMapShell()==Shell);
            TArray<UUserWidget*> Widgets;
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World,Widgets,UMapShellWidget::StaticClass(),true);
            Test->TestEqual(TEXT("One visible Map shell"),Widgets.Num(),1);
            Test->TestNotNull(TEXT("Map 2D control retains stable name"),Shell->WidgetTree->FindWidget(TEXT("map2d")));
            Test->TestNotNull(TEXT("Map 3D control retains stable name"),Shell->WidgetTree->FindWidget(TEXT("map3d")));
            VideoBefore=VideoJson(Sync);
            Shell->Map2DButton->OnClicked.Broadcast();
            Sync->SetLanguage(TEXT("zh-Hans"));
        }
        else if(Step==1)
        {
            FString Language;Sync->GetUIPreferences()->TryGetStringField(TEXT("language"),Language);
            if(Language!=TEXT("zh-Hans"))return false;
            Test->TestEqual(TEXT("2D button reaches existing map service"),Manager->GetMapService()->GetMapMode(),ECommandMapMode::Map2D);
            Boundaries(Shell,TEXT("Initial layout"));InitialFramePixels=PixelSize(Shell->MapFrame);
            Capture(TEXT("01-main"));Shell->DeviceButton->OnClicked.Broadcast();
        }
        else if(Step==2)
        {
            Test->TestTrue(TEXT("Device action opens the dropdown"),Shell->DroneDropdown->IsVisible());
            Test->TestFalse(TEXT("Dropdown over map blocks map input"),Shell->IsPointOverMap(Center(Shell->DroneDropdown)));
            Capture(TEXT("02-dropdown"));
            const int32 Before=Registry->GetPrimarySelectedDrone();
            for(const auto& Descriptor:Registry->GetFriendlyDroneDescriptors())if(Descriptor.DroneId!=Before){SelectedId=Descriptor.DroneId;break;}
            Shell->ActionRequested(TEXT("choose_aircraft"),SelectedId);
            Test->TestEqual(TEXT("Device choice does not optimistically mutate selection"),Registry->GetPrimarySelectedDrone(),Before);
        }
        else if(Step==3)
        {
            if(Registry->GetPrimarySelectedDrone()!=SelectedId || Shell->IsDeviceSelectionPending())return false;
            Test->TestFalse(TEXT("ACK closes device dropdown"),Shell->DroneDropdown->IsVisible());
            Test->TestEqual(TEXT("Map selection preserves exact VideoView"),VideoJson(Sync),VideoBefore);
            for(const auto& Descriptor:Registry->GetFriendlyDroneDescriptors())GroupSelection.Add(Descriptor.DroneId);
            GroupSelection.Sort();
            if(!Test->TestTrue(TEXT("Same-primary device regression has UAV 1 and multiple registered aircraft"),GroupSelection.Contains(1) && GroupSelection.Num()>1))return true;
            if(!Test->TestTrue(TEXT("Group selection is submitted to Backend before same-primary device choice"),Sync->SetLocalSelection(1,GroupSelection)))return true;
        }
        else if(Step==4)
        {
            if(Registry->GetPrimarySelectedDrone()!=1 || Registry->GetMultiSelectedDrones()!=GroupSelection)return false;
            auto* Fleet=Shell->GetFleetInteraction();
            if(!Test->TestNotNull(TEXT("Device ACK guard uses the actual fleet preview widget"),Fleet))return true;
            Test->TestTrue(TEXT("Backend group ACK preserves independent VideoView"),VideoJson(Sync)==VideoBefore);
            Shell->DeviceButton->OnClicked.Broadcast();
            Test->TestTrue(TEXT("Same-primary choice uses the visible device menu"),Shell->DroneDropdown->IsVisible());
            const int32 OptionIndex=Shell->AircraftOptions.IndexOfByPredicate([Shell](const FString& Option){
                const int32* Id=Shell->AircraftIds.Find(Option);return Id && *Id==1;
            });
            auto* OptionRow=OptionIndex!=INDEX_NONE?Cast<USizeBox>(Shell->DroneOptions->GetChildAt(OptionIndex)):nullptr;
            auto* OptionButton=OptionRow?Cast<UCommandActionButton>(OptionRow->GetContent()):nullptr;
            if(!Test->TestNotNull(TEXT("Current primary UAV has an actual device option button"),OptionButton))return true;
            OptionButton->OnClicked.Broadcast();
            // The HTTP callback cannot run within this synchronous click. The
            // unchanged primary alone must not stand in for the singleton ACK.
            Test->TestTrue(TEXT("Choosing current primary from a group stays pending before singleton ACK"),Shell->IsDeviceSelectionPending());
            Test->TestTrue(TEXT("Device click preserves acknowledged group until Backend replies"),Registry->GetMultiSelectedDrones()==GroupSelection);
            Test->TestFalse(TEXT("Right-click destination is rejected while same-primary singleton selection awaits ACK"),Fleet->PickDestinationAtScreen(Center(Shell->MapFrame)));
            Test->TestTrue(TEXT("Pending device selection creates no fleet preview members"),Fleet->GetPreviewMembers().IsEmpty());
            Test->TestEqual(TEXT("Pending same-primary device selection preserves VideoView"),VideoJson(Sync),VideoBefore);
        }
        else if(Step==5)
        {
            if(Shell->IsDeviceSelectionPending() || Registry->GetPrimarySelectedDrone()!=1 || Registry->GetMultiSelectedDrones()!=TArray<int32>{1})return false;
            Test->TestFalse(TEXT("Singleton ACK releases the device selection guard"),Shell->IsDeviceSelectionPending());
            Test->TestTrue(TEXT("Singleton ACK contains only the explicitly chosen primary"),Registry->GetMultiSelectedDrones()==TArray<int32>{1});
            Test->TestTrue(TEXT("Singleton ACK does not silently generate a move preview"),Shell->GetFleetInteraction()->GetPreviewMembers().IsEmpty());
            Test->TestEqual(TEXT("Same-primary singleton ACK preserves exact VideoView"),VideoJson(Sync),VideoBefore);
            Shell->Map3DButton->OnClicked.Broadcast();
        }
        else if(Step==6)
        {
            Test->TestEqual(TEXT("3D button reaches existing map service"),Manager->GetMapService()->GetMapMode(),ECommandMapMode::Map3D);
            Boundaries(Shell,TEXT("3D layout"));Capture(TEXT("03-3d"));
            Shell->Map2DButton->OnClicked.Broadcast();Shell->RouteButton->OnClicked.Broadcast();
        }
        else if(Step==7)
        {
            Test->TestTrue(TEXT("Mission route action opens existing route panel"),Shell->PlanPanel->IsEditorEnabled() && Shell->PlanPanel->IsVisible());
            Test->TestFalse(TEXT("Route sidebar cannot click through"),Shell->IsPointOverMap(Center(Shell->PlanPanel)));
            Test->TestTrue(TEXT("Opening route panel reserves map width"),PixelSize(Shell->MapFrame).X<InitialFramePixels.X*.8);
            Boundaries(Shell,TEXT("Route layout"));Capture(TEXT("04-route"));
            Shell->RouteButton->OnClicked.Broadcast();Shell->Map2DButton->OnClicked.Broadcast();
            const auto OwnedWindow=World->GetGameViewport()->GetWindow();
            if(!OwnedWindow){Test->AddError(TEXT("Owned PIE window unavailable for resize regression"));return true;}
            Window=OwnedWindow;OriginalWindowSize=OwnedWindow->GetClientSizeInScreen();
            OwnedWindow->Resize(FVector2D(1120.,840.));
        }
        else if(Step==8)
        {
            const FVector2D Resized=PixelSize(Shell->MapFrame);
            if(Resized.Equals(InitialFramePixels,2.))return false;
            Test->TestFalse(TEXT("Closed route panel has no stale hit region after resize"),Shell->PlanPanel->IsVisible());
            Test->TestFalse(TEXT("PIE resize changes rendered frame dimensions"),Resized.Equals(InitialFramePixels,2.));
            Test->TestTrue(TEXT("Scaled chrome retains frame aspect ratio"),FMath::IsNearlyEqual(Resized.X/Resized.Y,1380./574.,.01));
            Boundaries(Shell,TEXT("Resized 4:3 window"));
            Test->TestEqual(TEXT("Layout and map mode changes preserve VideoView"),VideoJson(Sync),VideoBefore);
            Test->AddInfo(FString::Printf(TEXT("Resize geometry %.1fx%.1f -> %.1fx%.1f; widget actions/RHI only, no native mouse or flight claim."),
                InitialFramePixels.X,InitialFramePixels.Y,Resized.X,Resized.Y));
            return true;
        }
        ++Step;Next=Now+1.5;return false;
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFigmaMapPresentation,"DroneOps.Figma.MapPresentation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFigmaMapPresentation::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Tests/P55OfflineMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FFigmaMapStartPIE());
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(8));
    ADD_LATENT_AUTOMATION_COMMAND(FFigmaMapChecks(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
