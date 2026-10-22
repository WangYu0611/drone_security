#include "Map/MapShellWidget.h"
#include "Shared/PlanWidgetSupport.h"
#include "Shared/PlanConfiguration.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "PathEditor/DronePathActor.h"
#include "EngineUtils.h"
#include "Map/MapExecutionWidget.h"
#include "Shared/FigmaRoleHeaderWidget.h"
#include "Map/FigmaMapStyle.h"
#include "Components/SizeBox.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Shared/ProductText.h"
#include "Map/MapMissionRouteWidget.h"
#include "Map/MapPlanMoveWidget.h"
#include "Map/MapFleetInteractionWidget.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Engine/GameInstance.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandMapInteractionService.h"
#include "Command/CommandActionButton.h"
#include "Command/CommandTheme.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "UI/SequenceDispatchPanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/Pawn.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"

static void RefreshMapLegend(UWorld* World,UBorder* RouteLegend,UTextBlock* RouteLegendText) {
    if(RouteLegend){
        ADronePathActor* SelectedRoute=nullptr;
        for(TActorIterator<ADronePathActor> It(World);It;++It)if(!It->IsHidden() && !It->GetSegmentVisuals().IsEmpty()){
            if(!SelectedRoute || It->IsVisualSelected())SelectedRoute=*It;if(It->IsVisualSelected())break;
        }
        RouteLegend->SetVisibility(SelectedRoute?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
        if(SelectedRoute){double Min=DBL_MAX,Max=-DBL_MAX;float Slow=FLT_MAX,Fast=0;
            for(const auto& V:SelectedRoute->GetSegmentVisuals()){Min=FMath::Min3(Min,V.StartAltitude,V.EndAltitude);Max=FMath::Max3(Max,V.StartAltitude,V.EndAltitude);Slow=FMath::Min(Slow,V.EffectiveSpeed);Fast=FMath::Max(Fast,V.EffectiveSpeed);}
            RouteLegendText->SetText(FText::Format(ProductText::Get(TEXT("RouteVisual.Legend")),
                FText::AsNumber(FMath::RoundToInt(Min)),FText::AsNumber(FMath::RoundToInt(Max)),FText::AsNumber(Slow),FText::AsNumber(Fast),
                ProductText::Get(SelectedRoute->HasResolvedGeographicAltitude()?TEXT("RouteVisual.Ellipsoid"):TEXT("RouteVisual.WorldHeight"))));
        }
    }
}

void UMapShellWidget::InitializeManager(UCommandScreenManager* InManager) { Manager = InManager; Refresh(); }
void UMapShellWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>();Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);WidgetTree->RootWidget=Root;
    // Backgrounds are children at the lowest Z. UUserWidget::NativePaint runs
    // after Slate children, so an opaque shell paint would cover the chrome.
    for(int I=0;I<4;++I){auto* Mask=WidgetTree->ConstructWidget<UBorder>();Mask->SetBrushColor(MapUI::Background);Mask->SetVisibility(ESlateVisibility::HitTestInvisible);BackgroundMasks.Add(Mask);MapUI::Place(Root,Mask,FVector2D::ZeroVector,FVector2D::ZeroVector,-10);}
    MapTint=WidgetTree->ConstructWidget<UBorder>();MapTint->SetBrushColor(FLinearColor(.018f,.038f,.065f,.13f));MapTint->SetVisibility(ESlateVisibility::HitTestInvisible);MapUI::Place(Root,MapTint,FVector2D::ZeroVector,FVector2D::ZeroVector,-11);
    FigmaChrome=MapUI::Chrome(WidgetTree,Root,20);
    auto* Header=CreateWidget<UFigmaRoleHeaderWidget>(GetOwningPlayer());MapUI::Place(FigmaChrome,Header,FVector2D::ZeroVector,FVector2D(1440,90),10);
    auto Label=[&](const FText& Value,int Size,bool Bold=false){auto* W=WidgetTree->ConstructWidget<UTextBlock>();MapUI::Text(W,Size,FigmaUI::PrimaryText,Bold);W->SetText(Value);return W;};
    MapUI::Place(FigmaChrome,Label(ProductText::Get(TEXT("MapUI.Operations")),37,true),FVector2D(30,98),FVector2D(480,50));
    Status=Label(FText::GetEmpty(),12);Status->SetVisibility(ESlateVisibility::HitTestInvisible);MapUI::Place(FigmaChrome,Status,FVector2D(720,115),FVector2D(690,24));Status->SetJustification(ETextJustify::Right);
    MapFrame=WidgetTree->ConstructWidget<UBorder>();MapUI::Panel(MapFrame,FLinearColor::Transparent,FigmaUI::Outline,9,FMargin(0));MapFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
    MapUI::Place(FigmaChrome,MapFrame,FVector2D(30,216),FVector2D(1380,574));
    CenterHost=WidgetTree->ConstructWidget<UOverlay>();CenterHost->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto* CenterSlot=Root->AddChildToCanvas(CenterHost);CenterSlot->SetAnchors(FAnchors(0,0,1,1));CenterSlot->SetOffsets(FMargin(0));CenterSlot->SetZOrder(15);
    // Compatibility handles are retained; the fleet layer owns the visible marker.
    SelectionHighlight=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("MapSelectionHighlight"));SelectionHighlight->SetVisibility(ESlateVisibility::Collapsed);Root->AddChildToCanvas(SelectionHighlight);
    MissionHighlight=WidgetTree->ConstructWidget<UBorder>();MissionHighlight->SetVisibility(ESlateVisibility::Collapsed);Root->AddChildToCanvas(MissionHighlight);
    Toolbar=WidgetTree->ConstructWidget<UBorder>();Toolbar->SetPadding(FMargin(0));Toolbar->SetBrushColor(FLinearColor::Transparent);MapUI::Place(FigmaChrome,Toolbar,FVector2D(30,157),FVector2D(1010,44),2);
    auto* Tools=WidgetTree->ConstructWidget<UHorizontalBox>();Toolbar->SetContent(Tools);
    auto MakeButton=[&](const TCHAR* Name,const FText& Caption,const TCHAR* Icon,float Width){
        auto* B=WidgetTree->ConstructWidget<UCommandActionButton>(UCommandActionButton::StaticClass(),FName(Name));B->Configure(Name);B->OnAction.AddDynamic(this,&UMapShellWidget::ActionRequested);MapUI::Button(B,false,true);
        auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();B->SetContent(Row);
        if(Icon && *Icon){auto* I=MapUI::Icon(WidgetTree,Icon,FVector2D(22));auto* IS=Row->AddChildToHorizontalBox(I);IS->SetVerticalAlignment(VAlign_Center);IS->SetPadding(FMargin(0,0,8,0));if(FString(Name)==TEXT("security_plan"))RouteButtonIcon=I;}
        auto* L=Label(Caption,14,true);L->SetJustification(ETextJustify::Center);auto* LS=Row->AddChildToHorizontalBox(L);LS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));LS->SetVerticalAlignment(VAlign_Center);
        if(!Icon || !*Icon){L->RemoveFromParent();B->SetContent(L);}
        auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(Width);Size->SetHeightOverride(40);Size->SetContent(B);Tools->AddChildToHorizontalBox(Size)->SetPadding(FMargin(0,0,10,0));return B;
    };
    Map2DButton=MakeButton(TEXT("map2d"),PlanUI::User(TEXT("2D")),TEXT(""),58);Map3DButton=MakeButton(TEXT("map3d"),PlanUI::User(TEXT("3D")),TEXT(""),58);
    auto* Divider=WidgetTree->ConstructWidget<UBorder>();Divider->SetBrushColor(FigmaUI::Outline);auto* DS=WidgetTree->ConstructWidget<USizeBox>();DS->SetWidthOverride(1);DS->SetHeightOverride(26);DS->SetContent(Divider);auto* DSlot=Tools->AddChildToHorizontalBox(DS);DSlot->SetPadding(FMargin(8,0,18,0));DSlot->SetVerticalAlignment(VAlign_Center);
    MakeButton(TEXT("focus"),ProductText::Source(TEXT("Locate")),TEXT("locate"),96);
    RouteButton=MakeButton(TEXT("security_plan"),ProductText::Get(TEXT("MapUI.MissionRoute")),TEXT("route"),122);
    MakeButton(TEXT("open_video"),ProductText::Get(TEXT("MapUI.ViewVideo")),TEXT("video"),122);
    BoxSelectButton=MakeButton(TEXT("box_select"),ProductText::Source(TEXT("Box select UAVs")),TEXT(""),126);
    GroupMoveButton=MakeButton(TEXT("group_move"),ProductText::Source(TEXT("Fleet move")),TEXT(""),112);
    // A hidden option model preserves the old selection contract. The visible
    // dropdown is the design's icon/radio list, populated from live Registry IDs.
    Aircraft=WidgetTree->ConstructWidget<UComboBoxString>();Aircraft->OnSelectionChanged.AddDynamic(this,&UMapShellWidget::AircraftSelected);
    auto* DeviceLabel=Label(ProductText::Get(TEXT("MapUI.CurrentDevice")),13);MapUI::Place(FigmaChrome,DeviceLabel,FVector2D(1047,169),FVector2D(95,24));DeviceLabel->SetJustification(ETextJustify::Right);
    DeviceButton=WidgetTree->ConstructWidget<UCommandActionButton>();DeviceButton->Configure(TEXT("devices"));DeviceButton->OnAction.AddDynamic(this,&UMapShellWidget::ActionRequested);MapUI::Button(DeviceButton,false,true);
    auto* DeviceRow=WidgetTree->ConstructWidget<UHorizontalBox>();DeviceButton->SetContent(DeviceRow);DeviceCaption=Label(FText::GetEmpty(),14,true);auto* DC=DeviceRow->AddChildToHorizontalBox(DeviceCaption);DC->SetSize(FSlateChildSize(ESlateSizeRule::Fill));DC->SetVerticalAlignment(VAlign_Center);
    auto* Arrow=MapUI::Icon(WidgetTree,TEXT("chevron"),FVector2D(10,7));DeviceRow->AddChildToHorizontalBox(Arrow)->SetVerticalAlignment(VAlign_Center);
    MapUI::Place(FigmaChrome,DeviceButton,FVector2D(1148,157),FVector2D(250,44),12);
    DroneDropdown=WidgetTree->ConstructWidget<UBorder>();MapUI::Panel(DroneDropdown,FigmaUI::Surface,FigmaUI::Outline,8,FMargin(6));DroneDropdown->SetVisibility(ESlateVisibility::Collapsed);
    auto* DropScroll=WidgetTree->ConstructWidget<UScrollBox>();DroneDropdown->SetContent(DropScroll);DroneOptions=WidgetTree->ConstructWidget<UVerticalBox>();DropScroll->AddChild(DroneOptions);
    MapUI::Place(FigmaChrome,DroneDropdown,FVector2D(1148,204),FVector2D(250,224),30);
    ZoomControls=WidgetTree->ConstructWidget<UVerticalBox>();MapUI::Place(FigmaChrome,ZoomControls,FVector2D(45,707),FVector2D(32,70),4);
    for(const auto& Entry:TArray<TPair<FString,FString>>{{TEXT("zoom_in"),TEXT("plus")},{TEXT("zoom_out"),TEXT("minus")}}){
        auto* B=WidgetTree->ConstructWidget<UCommandActionButton>();B->Configure(FName(*Entry.Key));B->OnAction.AddDynamic(this,&UMapShellWidget::ActionRequested);MapUI::Button(B,false,true);
        auto ZoomStyle=B->GetStyle();ZoomStyle.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));B->SetStyle(ZoomStyle);
        auto* I=MapUI::Icon(WidgetTree,Entry.Value,FVector2D(12));B->SetContent(I);if(auto* IconSlot=Cast<UButtonSlot>(I->Slot)){IconSlot->SetPadding(FMargin(8));IconSlot->SetHorizontalAlignment(HAlign_Center);IconSlot->SetVerticalAlignment(VAlign_Center);}auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetHeightOverride(30);Size->SetContent(B);ZoomControls->AddChildToVerticalBox(Size)->SetPadding(FMargin(0,0,0,8));
    }
    CameraHint=WidgetTree->ConstructWidget<UBorder>();MapUI::Panel(CameraHint,MapUI::Surface,FigmaUI::Outline,6,FMargin(8,5));MapUI::Place(FigmaChrome,CameraHint,FVector2D(1035,748),FVector2D(360,30),4);
    auto* HintRow=WidgetTree->ConstructWidget<UHorizontalBox>();CameraHint->SetContent(HintRow);auto* Info=MapUI::Icon(WidgetTree,TEXT("hint"),FVector2D(16));HintRow->AddChildToHorizontalBox(Info)->SetVerticalAlignment(VAlign_Center);
    ActionStatus=Label(FText::GetEmpty(),11);MapUI::Text(ActionStatus,11,FigmaUI::Muted);ActionStatus->SetAutoWrapText(true);auto* HS=HintRow->AddChildToHorizontalBox(ActionStatus);HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));HS->SetPadding(FMargin(8,0,0,0));HS->SetVerticalAlignment(VAlign_Center);
    RouteLegend=WidgetTree->ConstructWidget<UBorder>();MapUI::Panel(RouteLegend,MapUI::Surface,FigmaUI::Outline,6,FMargin(8));RouteLegend->SetVisibility(ESlateVisibility::HitTestInvisible);MapUI::Place(FigmaChrome,RouteLegend,FVector2D(90,728),FVector2D(330,50),4);
    auto* LegendBox=WidgetTree->ConstructWidget<UVerticalBox>();RouteLegend->SetContent(LegendBox);auto* Colors=WidgetTree->ConstructWidget<UHorizontalBox>();LegendBox->AddChild(Colors);
    for(int I=0;I<5;++I){auto* Swatch=WidgetTree->ConstructWidget<UBorder>();Swatch->SetBrushColor(RouteVisual::AltitudeColor(I*30.,0,120));Swatch->SetPadding(FMargin(22,2));Colors->AddChild(Swatch);}
    RouteLegendText=Label(FText::GetEmpty(),10);RouteLegendText->SetAutoWrapText(true);LegendBox->AddChild(RouteLegendText);
    PlanPanel=CreateWidget<UMapMissionRouteWidget>(GetOwningPlayer());MapUI::Place(FigmaChrome,PlanPanel,FVector2D(30,216),FVector2D(419,574),5);PlanPanel->SetEditorEnabled(false);
    MovePanel=CreateWidget<UMapPlanMoveWidget>(GetOwningPlayer());auto* MoveSlot=Root->AddChildToCanvas(MovePanel);MoveSlot->SetAnchors(FAnchors(0,0,1,1));MoveSlot->SetOffsets(FMargin(0));MoveSlot->SetZOrder(25);
    ExecutionMonitor=CreateWidget<UMapExecutionWidget>(GetOwningPlayer());auto* ExecSlot=Root->AddChildToCanvas(ExecutionMonitor);ExecSlot->SetAnchors(FAnchors(0,0,1,1));ExecSlot->SetOffsets(FMargin(0));ExecSlot->SetZOrder(2);
    FleetInteraction=CreateWidget<UMapFleetInteractionWidget>(GetOwningPlayer());auto* FleetSlot=Root->AddChildToCanvas(FleetInteraction);FleetSlot->SetAnchors(FAnchors(0,0,1,1));FleetSlot->SetOffsets(FMargin(0));FleetSlot->SetZOrder(6);
}
void UMapShellWidget::Refresh()
{
    const auto PlanState=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans();
    const bool Configuring=PlanConfiguration::Active(PlanState);
    if(MovePanel)MovePanel->Refresh();const bool Moving=MovePanel && MovePanel->IsMoving();
    if(Configuring && MovePanel)MovePanel->SetVisibility(ESlateVisibility::Collapsed);
    if(PlanPanel && !Moving){if(bWasMoving)PlanPanel->ReloadGeometry();PlanPanel->Refresh();}
    if(Moving && PlanPanel)PlanPanel->SetVisibility(ESlateVisibility::Collapsed);bWasMoving=Moving;
    if(ExecutionMonitor){
        ExecutionMonitor->SetRouteSuppression(Moving?MovePanel->GetVisualPlanId():PlanPanel && PlanPanel->OwnsRouteVisual()?PlanPanel->GetVisualPlanId():FString(),
            PlanPanel && PlanPanel->OwnsRouteVisual()?PlanPanel->GetVisualMissionId():FString(),Moving);
        ExecutionMonitor->Refresh();if(Moving || (PlanPanel && PlanPanel->IsEditorEnabled()))ExecutionMonitor->SetVisibility(ESlateVisibility::Collapsed);
    }
    RefreshSavedRoutes(Moving);
    if(FleetInteraction) {
        FleetInteraction->Refresh();const bool Available=FleetInteraction->IsAvailable();
        if(ExecutionMonitor)ExecutionMonitor->SetManagedFleetGroup(FleetInteraction->GetManagedGroup(),FleetInteraction->GetPreviewMembers());
        BoxSelectButton->SetIsEnabled(Available);GroupMoveButton->SetIsEnabled(Available);
        BoxSelectButton->SetVisibility(Configuring?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
        GroupMoveButton->SetVisibility(Configuring?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
        MapUI::Button(BoxSelectButton,FleetInteraction->GetMode()==EMapFleetMode::BoxSelect);
        MapUI::Button(GroupMoveButton,FleetInteraction->GetMode()==EMapFleetMode::GroupMove);
    }
    RefreshMapLegend(GetWorld(),RouteLegend,RouteLegendText);
    if (!Manager.IsValid() || !Manager->GetRegistry() || !Status) return;
    auto* Registry = Manager->GetRegistry(); auto* Map = Manager->GetMapService();
    TArray<FString> Options; AircraftIds.Reset();
    auto Drones = Registry->GetFriendlyDroneDescriptors();
    Drones.Sort([](const FDroneDescriptor& A, const FDroneDescriptor& B) { return A.DroneId < B.DroneId; });
    FString Selected;
    for (const auto& D : Drones)
    {
        FDroneLabelSettings Settings;Registry->GetDroneLabelSettings(D.DroneId,Settings);
        const FString Label = FString::Printf(TEXT("UAV-%02d · %s"), D.DroneId, *(Settings.DisplayName.IsEmpty()?D.Name:Settings.DisplayName));
        Options.Add(Label); AircraftIds.Add(Label, D.DroneId);
        if (Registry->GetPrimarySelectedDrone() == D.DroneId) Selected = Label;
    }
    bRefreshing = true;
    if (Options != AircraftOptions)
    {
        Aircraft->ClearOptions(); for (const auto& Option : Options) Aircraft->AddOption(Option);
        AircraftOptions = Options;
    }
    if (Selected.IsEmpty()) Aircraft->ClearSelection();
    else if (Aircraft->GetSelectedOption() != Selected) Aircraft->SetSelectedOption(Selected);
    bRefreshing = false;
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    if(bDeviceSelectionPending && Registry->GetPrimarySelectedDrone()==PendingDevice && Registry->GetMultiSelectedDrones()==TArray<int32>{PendingDevice}){bDeviceSelectionPending=false;DeviceSelectionError=FText::GetEmpty();}
    if(bDeviceSelectionPending && (!Sync->IsReady() || FPlatformTime::Seconds()-DeviceSelectionRequestedAt>10.)){
        bDeviceSelectionPending=false;DeviceSelectionError=ProductText::Get(TEXT("Errors.SYNC_FAILED"));}
    DeviceCaption->SetText(bDeviceSelectionPending?ProductText::Get(TEXT("Fleet.SelectionPending")):Selected.IsEmpty()?ProductText::Get(TEXT("MapUI.SelectDevice")):FText::AsCultureInvariant(Selected));
    DeviceButton->SetIsEnabled(!Configuring && !bDeviceSelectionPending);
    if(Configuring)bDropdownOpen=false;
    DroneDropdown->SetVisibility(bDropdownOpen?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    RefreshDeviceMenu();
    const bool RouteOpen=PlanPanel && PlanPanel->IsVisible();
    auto* FrameSlot=CastChecked<UCanvasPanelSlot>(MapFrame->Slot);FrameSlot->SetPosition(FVector2D(RouteOpen?461:30,216));FrameSlot->SetSize(FVector2D(RouteOpen?949:1380,574));
    CastChecked<UCanvasPanelSlot>(ZoomControls->Slot)->SetPosition(FVector2D(RouteOpen?476:45,707));
    CastChecked<UCanvasPanelSlot>(RouteLegend->Slot)->SetPosition(FVector2D(RouteOpen?520:90,728));
    MapUI::Button(RouteButton,RouteOpen,true);
    if(RouteButtonIcon && bRouteIconActive!=RouteOpen){bRouteIconActive=RouteOpen;auto* Icon=MapUI::Icon(WidgetTree,RouteOpen?TEXT("route-active"):TEXT("route"),FVector2D(22));RouteButtonIcon->SetBrush(Icon->GetBrush());}
    const bool b2D = !Map || Map->GetMapMode() == ECommandMapMode::Map2D;
    const bool Locked=Map && Map->IsRouteEditCameraLocked();
    Map2DButton->SetIsEnabled(!Locked);Map3DButton->SetIsEnabled(!Locked);
    const FText ModeHint=Locked?ProductText::Get(TEXT("Map.FinishBeforeMode")):FText::GetEmpty();
    Map2DButton->SetToolTipText(ModeHint);Map3DButton->SetToolTipText(ModeHint);
    const FText MapHint=ProductText::Get(Map && Map->HasInvalidMapPosition()?TEXT("Map.InvalidPosition"):Locked?TEXT("MapMove.EditCameraHint"):TEXT("Map.AdjustBeforeEdit"));

    const bool FleetInput=FleetInteraction && FleetInteraction->IsAvailable();
    ActionStatus->SetText(ProductText::Get(FleetInput?TEXT("MapUI.FleetPointerHint"):TEXT("MapUI.CameraHint")));
    ActionStatus->SetToolTipText(FleetInput?ProductText::Get(TEXT("MapUI.FleetPointerHelp")):FText::Format(PlanUI::User(TEXT("{0}\n{1}")),ActionStatus->GetText(),MapHint));
    Aircraft->SetIsEnabled(!Configuring);Aircraft->SetToolTipText(Configuring?ActionStatus->GetText():FText::GetEmpty());
    MapUI::Button(Map2DButton, b2D); MapUI::Button(Map3DButton, !b2D);
    Status->SetText(ProductText::Source(FString::Printf(TEXT("UAV %02d  |  %s  |  %s"), Registry->GetPrimarySelectedDrone(),
        b2D ? TEXT("2D") : TEXT("3D"), Map && Map->OwnsCamera() ? TEXT("Camera ready") : TEXT("Initializing"))));
    if (Sync && Sync->IsEnabled()){
        Status->SetText(ProductText::Get(!Sync->IsReady()?TEXT("FleetUX.SyncWaiting"):PlanUI::Field(Sync->GetPlans(),TEXT("execution_adapter"))==TEXT("Mock")?TEXT("FleetUX.SyncMock"):TEXT("FleetUX.SyncReady")));
        Status->SetColorAndOpacity(Sync->IsReady()?MapUI::Online:MapUI::Warning);
    }
    if(!DeviceSelectionError.IsEmpty()){Status->SetText(DeviceSelectionError);Status->SetColorAndOpacity(MapUI::Warning);}
}
void UMapShellWidget::AircraftSelected(FString Item, ESelectInfo::Type Type)
{
    if(PlanConfiguration::Active(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans()))return;
    if (bRefreshing || !Manager.IsValid() || !Manager->GetMapService()) return;
    if (const int32* Id = AircraftIds.Find(Item)) ActionRequested(TEXT("choose_aircraft"),*Id);
}
void UMapShellWidget::AttachCenterWidget(UUserWidget* Widget)
{
    if (!Widget || !CenterHost || Widget->GetParent() == CenterHost) return;
    auto* HostSlot = CenterHost->AddChildToOverlay(Widget);
    HostSlot->SetHorizontalAlignment(HAlign_Fill); HostSlot->SetVerticalAlignment(VAlign_Fill);
}
bool UMapShellWidget::IsCursorOverMap() const
{
    return FSlateApplication::IsInitialized() && IsPointOverMap(FSlateApplication::Get().GetCursorPos());
}
bool UMapShellWidget::IsPointOverMap(const FVector2D& CursorPosition) const
{
    if(!IsVisible() || !MapFrame || !MapFrame->GetCachedGeometry().IsUnderLocation(CursorPosition))return false;
    if(FleetInteraction && FleetInteraction->IsOverControls(CursorPosition))return false;
    if(ExecutionMonitor && ExecutionMonitor->IsOverControls(CursorPosition))return false;
    if(MovePanel && MovePanel->IsOverControls(CursorPosition))return false;
    for(const UWidget* W:TArray<const UWidget*>{PlanPanel.Get(),DroneDropdown.Get(),CameraHint.Get(),ZoomControls.Get(),RouteLegend.Get()})
        if(W && W->IsVisible() && W->GetCachedGeometry().IsUnderLocation(CursorPosition))return false;
    bool bBlocked = false;
    for (UWidget* Child : CenterHost->GetAllChildren())
        if (auto* Panel = Cast<UUserWidget>(Child); Panel && Panel->IsVisible() && Panel->WidgetTree)
            Panel->WidgetTree->ForEachWidget([&](UWidget* Widget)
            {
                for (UWidget* Parent = Widget; Parent; Parent = Parent->GetParent()) if (!Parent->IsVisible()) return;
                if ((Widget->IsA<UBorder>() || Widget->IsA<UButton>() || Widget->IsA<UScrollBox>()
                    || Widget->IsA<UEditableTextBox>() || Widget->IsA<UComboBoxString>())
                    && Widget->GetCachedGeometry().IsUnderLocation(CursorPosition)) bBlocked = true;
            });
    return !bBlocked;
}
void UMapShellWidget::ActionRequested(FName Action, int32 ItemId)
{
    if(Action==TEXT("en") || Action==TEXT("zh-Hans")){GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->SetLanguage(Action.ToString());return;}
    if(!Manager.IsValid() || !Manager->GetRegistry())return;
    if(Action==TEXT("devices")){bDropdownOpen=!bDropdownOpen;Refresh();return;}
    if(Action==TEXT("choose_aircraft")){
        auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
        if(bDeviceSelectionPending || PlanConfiguration::Active(Sync->GetPlans()) || !Manager->GetRegistry()->IsDroneRegistered(ItemId))return;
        const bool Accepted=Sync->IsEnabled()?Sync->SetLocalSelection(ItemId,{ItemId}):Manager->GetMapService()->SelectDrone(ItemId);
        if(Accepted){DeviceSelectionError=FText::GetEmpty();PendingDevice=ItemId;bDeviceSelectionPending=Sync->IsEnabled();DeviceSelectionRequestedAt=FPlatformTime::Seconds();bDropdownOpen=false;}
        else DeviceSelectionError=ProductText::Get(TEXT("Errors.SYNC_FAILED"));Refresh();return;
    }
    if(Action==TEXT("open_video")){const int Id=Manager->GetRegistry()->GetPrimarySelectedDrone();if(Id>0)GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->OpenVideo(FString::Printf(TEXT("UAV-%02d"),Id));return;}
    if(Action==TEXT("box_select") || Action==TEXT("group_move")){
        const auto Mode=Action==TEXT("box_select")?EMapFleetMode::BoxSelect:EMapFleetMode::GroupMove;
        FleetInteraction->SetMode(FleetInteraction->GetMode()==Mode?EMapFleetMode::Browse:Mode);Refresh();return;
    }
    if(Action==TEXT("security_plan")){PlanPanel->SetEditorEnabled(!PlanPanel->IsEditorEnabled());Refresh();return;}
    if (!Manager.IsValid()) return;
    auto* Map = Manager->GetMapService(); if (!Map) return;
    if(Action==TEXT("zoom_in") || Action==TEXT("zoom_out")){Map->ZoomView(Action==TEXT("zoom_in")?1.f:-1.f);return;}
    if (Action == TEXT("map2d")) { Map->Enter2DMode(); Refresh(); return; }
    if (Action == TEXT("map3d")) { Map->Enter3DMode(); Refresh(); return; }
    if(PlanPanel && PlanPanel->IsEditorEnabled() && Action!=TEXT("focus")) {
        ActionStatus->SetText(ProductText::Source(TEXT("Use Security Plan Editor controls for this mission route.")));return;
    }
    bool bOk = false;
    if (Action == TEXT("focus")) {
        bOk = Map->FocusDrone(Manager->GetRegistry()->GetPrimarySelectedDrone(),true);
        if(bOk)Manager->CancelStartupFraming();
    }
    if (Action == TEXT("plan")) { Manager->ShowPathPanel(); bOk = Map->BeginPathPlanning(); }
    if (Action == TEXT("delete")) bOk = Map->DeleteSelectedWaypoint();
    if (Action == TEXT("confirm")) { Manager->ShowPathPanel(); bOk = Map->ConfirmPath(); }
    if (Action == TEXT("cancel")) bOk = Map->CancelPathPlanning();
    if (Action == TEXT("add")) { Manager->ToggleGeographicPanel(); bOk = true; }
    if (Action == TEXT("sequence")) { Manager->TogglePathTools(); bOk = true; }
    if (Action == TEXT("speed")) { Manager->ToggleSpeedPanel(); bOk = true; }
    if (Action == TEXT("save")) { if (auto* Panel = Manager->ShowPathPanel()) { Panel->SaveCommandPath(); bOk = true; } }
    if (Action == TEXT("stop")) { if (auto* Panel = Manager->ShowPathPanel()) { Panel->StopCommandPlayback(); bOk = true; } }
    ActionStatus->SetText(ProductText::Source(bOk ? TEXT("Operation submitted.")
        : TEXT("Unavailable: check selection, route, map actors or connection.")));
    Refresh();
}

void UMapShellWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry, DeltaSeconds);
    if(MapFrame && BackgroundMasks.Num()==4 && MapFrame->GetCachedGeometry().GetLocalSize().X>0){
        const auto& MG=MapFrame->GetCachedGeometry();const FVector2D Size=Geometry.GetLocalSize();
        const FVector2D A=Geometry.AbsoluteToLocal(MG.GetAbsolutePosition()),B=Geometry.AbsoluteToLocal(MG.LocalToAbsolute(MG.GetLocalSize()));
        auto Layout=[](UWidget* W,FVector2D P,FVector2D S){auto* CS=CastChecked<UCanvasPanelSlot>(W->Slot);CS->SetPosition(P);CS->SetSize(FVector2D(FMath::Max(0.,S.X),FMath::Max(0.,S.Y)));};
        Layout(BackgroundMasks[0],FVector2D::ZeroVector,FVector2D(Size.X,A.Y));Layout(BackgroundMasks[1],FVector2D(0,B.Y),FVector2D(Size.X,Size.Y-B.Y));
        Layout(BackgroundMasks[2],FVector2D(0,A.Y),FVector2D(A.X,B.Y-A.Y));Layout(BackgroundMasks[3],FVector2D(B.X,A.Y),FVector2D(Size.X-B.X,B.Y-A.Y));Layout(MapTint,A,B-A);
    }
    if(ExecutionMonitor && FleetInteraction)ExecutionMonitor->SetManagedFleetGroup(FleetInteraction->GetManagedGroup(),FleetInteraction->GetPreviewMembers());
    RefreshMapLegend(GetWorld(),RouteLegend,RouteLegendText);
    if(SelectionHighlight)SelectionHighlight->SetVisibility(ESlateVisibility::Collapsed);
    if(MissionHighlight)MissionHighlight->SetVisibility(ESlateVisibility::Collapsed);
}

void UMapShellWidget::RefreshSavedRoutes(bool Moving){
    using namespace PlanUI;
    auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());
    const bool Editing=!Moving && PlanPanel && PlanPanel->OwnsRouteVisual();
    if(PC)PC->SetMissionRouteVisualVisible(Editing);
    if(Moving || !PlanPanel){SavedRouteVisuals.Reset();return;}
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    // Use the panel's accepted selection: a dirty draft can reject remote switching.
    const FString PlanId=PlanPanel->GetVisualPlanId(),Selected=PlanPanel->GetVisualMissionId();
    const auto Plan=Find(Sync->GetPlans(),TEXT("plans"),PlanId);
    auto* C=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();
    TSet<FString> Live;const TArray<TSharedPtr<FJsonValue>>* Missions=nullptr;
    if(Plan && Plan->TryGetArrayField(TEXT("mission_ids"),Missions))for(const auto& V:*Missions){
        const FString Id=V->AsString();if((Editing && Id==Selected) || (ExecutionMonitor && ExecutionMonitor->OwnsMission(PlanId,Id)))continue;
        const auto Mission=Find(Sync->GetPlans(),TEXT("missions"),Id);
        const auto Route=Find(Sync->GetPlans(),TEXT("paths"),Field(Mission,TEXT("route_id")));if(!Route)continue;
        const FString Key=PlanId+TEXT("/")+Id;Live.Add(Key);
        SavedRouteVisuals.Put(GetWorld(),C,Key,Route,ERouteVisualState::Confirmed,Id==Selected);
    }
    SavedRouteVisuals.Retain(Live);SavedRouteVisuals.CheckConflicts(Editing && PC?PC->GetMissionRouteVisual():nullptr);
}
void UMapShellWidget::NativeDestruct(){
    SavedRouteVisuals.Reset();
    if(auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer()))PC->SetMissionRouteVisualVisible(false);
    Super::NativeDestruct();
}
int32 UMapShellWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const {
    return Super::NativePaint(Args,G,Clip,Out,Layer,Style,Enabled);
}
FGeometry UMapShellWidget::GetMapViewportGeometry() const {return MapFrame?MapFrame->GetCachedGeometry():FGeometry();}

void UMapShellWidget::RefreshDeviceMenu(){
    if(!Manager.IsValid() || !Manager->GetRegistry() || !DroneOptions)return;
    auto* Registry=Manager->GetRegistry();FString Signature=FString::FromInt(Registry->GetPrimarySelectedDrone())+TEXT("|")+ProductText::Get(TEXT("MapUI.CurrentDevice")).ToString();
    for(const auto& O:AircraftOptions)Signature+=TEXT("|")+O;
    if(Signature==MenuSignature)return;MenuSignature=Signature;DroneOptions->ClearChildren();
    for(const auto& O:AircraftOptions){const int32 Id=AircraftIds[O];const bool Selected=Registry->GetPrimarySelectedDrone()==Id;
        auto* B=WidgetTree->ConstructWidget<UCommandActionButton>();B->Configure(TEXT("choose_aircraft"),Id);B->OnAction.AddDynamic(this,&UMapShellWidget::ActionRequested);MapUI::Button(B,false,true);
        auto Style=B->GetStyle();Style.SetNormal(FSlateRoundedBoxBrush(Selected?FigmaUI::Accent.CopyWithNewOpacity(.35f):FigmaUI::Surface,5.f));Style.SetNormalPadding(FMargin(10,9));B->SetStyle(Style);
        auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();B->SetContent(Row);auto* Icon=MapUI::Icon(WidgetTree,TEXT("drone-option"),FVector2D(24,16));auto* IS=Row->AddChildToHorizontalBox(Icon);IS->SetVerticalAlignment(VAlign_Center);IS->SetPadding(FMargin(0,0,10,0));
        auto* Caption=WidgetTree->ConstructWidget<UTextBlock>();MapUI::Text(Caption,13);Caption->SetText(FText::AsCultureInvariant(O));Caption->SetClipping(EWidgetClipping::ClipToBounds);auto* CS=Row->AddChildToHorizontalBox(Caption);CS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));CS->SetVerticalAlignment(VAlign_Center);
        auto* Radio=WidgetTree->ConstructWidget<UOverlay>();auto* Outer=MapUI::Icon(WidgetTree,Selected?TEXT("radio-active"):TEXT("radio"),FVector2D(17));Radio->AddChildToOverlay(Outer)->SetHorizontalAlignment(HAlign_Center);
        if(Selected){auto* Dot=MapUI::Icon(WidgetTree,TEXT("radio-dot"),FVector2D(9));auto* OS=Radio->AddChildToOverlay(Dot);OS->SetHorizontalAlignment(HAlign_Center);OS->SetVerticalAlignment(VAlign_Center);}
        auto* RS=Row->AddChildToHorizontalBox(Radio);RS->SetVerticalAlignment(VAlign_Center);RS->SetPadding(FMargin(8,0,0,0));
        auto* Height=WidgetTree->ConstructWidget<USizeBox>();Height->SetHeightOverride(49);Height->SetContent(B);DroneOptions->AddChildToVerticalBox(Height)->SetPadding(FMargin(0,0,0,3));
    }
}

TArray<FBox2D> UMapShellWidget::GetMapControlBounds() const {
    TArray<FBox2D> Bounds;
    auto Add=[&](const UWidget* W){
        if(!W || !W->IsVisible())return;for(const UWidget* Parent=W->GetParent();Parent;Parent=Parent->GetParent())if(!Parent->IsVisible())return;
        const auto& G=W->GetCachedGeometry();if(G.GetLocalSize().X<=0 || G.GetLocalSize().Y<=0)return;
        Bounds.Add(FBox2D(G.GetAbsolutePosition(),G.LocalToAbsolute(G.GetLocalSize())));
    };
    for(const UWidget* W:TArray<const UWidget*>{PlanPanel.Get(),DroneDropdown.Get(),CameraHint.Get(),ZoomControls.Get(),RouteLegend.Get()})Add(W);
    auto Inspect=[&](const UUserWidget* Host){if(!Host || !Host->IsVisible() || !Host->WidgetTree)return;
        Host->WidgetTree->ForEachWidget([&](UWidget* W){if(W->IsA<UBorder>() || W->IsA<UButton>() || W->IsA<UScrollBox>() || W->IsA<UEditableTextBox>() || W->IsA<UComboBoxString>())Add(W);});};
    Inspect(FleetInteraction);Inspect(MovePanel);Inspect(ExecutionMonitor);
    if(CenterHost)for(UWidget* Child:CenterHost->GetAllChildren())Inspect(Cast<UUserWidget>(Child));
    return Bounds;
}
