#include "Map/MapShellWidget.h"
#include "Map/MapExecutionWidget.h"
#include "Shared/Stage1HeaderWidget.h"
#include "Shared/ProductText.h"
#include "Map/MapMissionRouteWidget.h"
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

void UMapShellWidget::InitializeManager(UCommandScreenManager* InManager) { Manager = InManager; Refresh(); }
void UMapShellWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    WidgetTree->RootWidget = Root;
    auto* ProductHeader=CreateWidget<UStage1HeaderWidget>(GetOwningPlayer());
    auto* ProductSlot=Root->AddChildToCanvas(ProductHeader);ProductSlot->SetAnchors(FAnchors(0,0,1,0));ProductSlot->SetOffsets(FMargin(0,0,0,72));ProductSlot->SetZOrder(5);
    CenterHost = WidgetTree->ConstructWidget<UOverlay>();
    CenterHost->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto* CenterSlot = Root->AddChildToCanvas(CenterHost);
    CenterSlot->SetZOrder(1);
    CenterSlot->SetAnchors(FAnchors(0, 0, 1, 1)); CenterSlot->SetOffsets(FMargin(0, 178, 0, 0));
    SelectionHighlight = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MapSelectionHighlight"));
    CommandTheme::Panel(SelectionHighlight, FLinearColor(0, 0.8f, 1, 0.06f), CommandTheme::Cyan);
    SelectionHighlight->SetVisibility(ESlateVisibility::Collapsed);
    auto* HighlightSlot = Root->AddChildToCanvas(SelectionHighlight);
    HighlightSlot->SetSize(FVector2D(48, 48)); HighlightSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    HighlightSlot->SetZOrder(0);
    MissionHighlight=WidgetTree->ConstructWidget<UBorder>();CommandTheme::Panel(MissionHighlight,FLinearColor(1,.6f,0,.08f),CommandTheme::Warning);
    MissionHighlight->SetVisibility(ESlateVisibility::Collapsed);
    auto* MissionSlot=Root->AddChildToCanvas(MissionHighlight);MissionSlot->SetSize(FVector2D(64,64));MissionSlot->SetAlignment(FVector2D(.5f,.5f));MissionSlot->SetZOrder(0);
    Toolbar = WidgetTree->ConstructWidget<UBorder>();
    CommandTheme::Panel(Toolbar, CommandTheme::Background);
    Toolbar->SetPadding(FMargin(12, 6));
    auto* TopSlot = Root->AddChildToCanvas(Toolbar);
    TopSlot->SetZOrder(2);
    TopSlot->SetAnchors(FAnchors(0, 0, 1, 0)); TopSlot->SetOffsets(FMargin(0, 72, 0, 104));
    auto* Rows = WidgetTree->ConstructWidget<UVerticalBox>(); Toolbar->SetContent(Rows);
    auto Text = [this](const FString& Value, int32 Size)
    {
        auto* W = WidgetTree->ConstructWidget<UTextBlock>(); W->SetText(ProductText::Source(Value));
        CommandTheme::Text(W, Size); return W;
    };
    auto* Header = WidgetTree->ConstructWidget<UHorizontalBox>(); Rows->AddChild(Header);
    Header->AddChildToHorizontalBox(Text(TEXT("MAP OPERATIONS"), 21))->SetPadding(FMargin(0, 0, 24, 0));
    Aircraft = WidgetTree->ConstructWidget<UComboBoxString>();
    Aircraft->OnSelectionChanged.AddDynamic(this, &UMapShellWidget::AircraftSelected);
    Header->AddChildToHorizontalBox(Aircraft)->SetPadding(FMargin(0, 0, 24, 0));
    Status = Text(TEXT("Map initializing"), 14); Header->AddChild(Status);
    auto* Tools = WidgetTree->ConstructWidget<UHorizontalBox>(); Rows->AddChild(Tools);
    auto Button = [&](const TCHAR* Name, const TCHAR* Label)
    {
        auto* W = WidgetTree->ConstructWidget<UCommandActionButton>(UCommandActionButton::StaticClass(), FName(Name));
        W->Configure(FName(Name)); W->OnAction.AddDynamic(this, &UMapShellWidget::ActionRequested);
        W->SetContent(Text(Label, 13)); Tools->AddChildToHorizontalBox(W)->SetPadding(FMargin(3, 5));
        return W;
    };
    Map2DButton = Button(TEXT("map2d"), TEXT("2D")); Map3DButton = Button(TEXT("map3d"), TEXT("3D"));
    Button(TEXT("focus"), TEXT("Locate"));
    Button(TEXT("security_plan"), TEXT("MISSION ROUTE"));
    Button(TEXT("open_video"), TEXT("OPEN VIDEO"));
    Button(TEXT("zh-Hans"), TEXT("中文"));Button(TEXT("en"),TEXT("English"));
    ActionStatus = Text(TEXT("Select a UAV here or on the map."), 12);
    Rows->AddChild(ActionStatus);
    PlanPanel=CreateWidget<UMapMissionRouteWidget>(GetOwningPlayer(),UMapMissionRouteWidget::StaticClass());
    auto* PlanSlot=Root->AddChildToCanvas(PlanPanel);PlanSlot->SetAnchors(FAnchors(0,0,0,1));PlanSlot->SetOffsets(FMargin(8,184,600,8));PlanSlot->SetZOrder(3);
    PlanPanel->SetEditorEnabled(false);
    ExecutionMonitor=CreateWidget<UMapExecutionWidget>(GetOwningPlayer());
    auto* ExecSlot=Root->AddChildToCanvas(ExecutionMonitor);ExecSlot->SetAnchors(FAnchors(0,0,1,1));ExecSlot->SetOffsets(FMargin(0));ExecSlot->SetZOrder(2);
}
void UMapShellWidget::Refresh()
{
    if(PlanPanel)PlanPanel->Refresh();
    if(ExecutionMonitor){ExecutionMonitor->Refresh();if(PlanPanel && PlanPanel->IsEditorEnabled())ExecutionMonitor->SetVisibility(ESlateVisibility::Collapsed);}
    if (!Manager.IsValid() || !Manager->GetRegistry() || !Status) return;
    auto* Registry = Manager->GetRegistry(); auto* Map = Manager->GetMapService();
    TArray<FString> Options; AircraftIds.Reset();
    auto Drones = Registry->GetFriendlyDroneDescriptors();
    Drones.Sort([](const FDroneDescriptor& A, const FDroneDescriptor& B) { return A.DroneId < B.DroneId; });
    FString Selected;
    for (const auto& D : Drones)
    {
        const FString Label = FString::Printf(TEXT("UAV %02d / %s"), D.DroneId, *D.Name);
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
    const bool b2D = !Map || Map->GetMapMode() == ECommandMapMode::Map2D;
    CommandTheme::Button(Map2DButton, b2D); CommandTheme::Button(Map3DButton, !b2D);
    Status->SetText(ProductText::Source(FString::Printf(TEXT("UAV %02d  |  %s  |  %s"), Registry->GetPrimarySelectedDrone(),
        b2D ? TEXT("2D") : TEXT("3D"), Map && Map->OwnsCamera() ? TEXT("Camera ready") : TEXT("Initializing"))));
    if (auto* Sync = GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>(); Sync && Sync->IsEnabled())
        Status->SetText(ProductText::Source(Sync->GetStatusText()));
}
void UMapShellWidget::AircraftSelected(FString Item, ESelectInfo::Type Type)
{
    if (bRefreshing || !Manager.IsValid() || !Manager->GetMapService()) return;
    if (const int32* Id = AircraftIds.Find(Item)) Manager->GetMapService()->SelectDrone(*Id);
}
void UMapShellWidget::AttachCenterWidget(UUserWidget* Widget)
{
    if (!Widget || !CenterHost || Widget->GetParent() == CenterHost) return;
    auto* HostSlot = CenterHost->AddChildToOverlay(Widget);
    HostSlot->SetHorizontalAlignment(HAlign_Fill); HostSlot->SetVerticalAlignment(VAlign_Fill);
}
bool UMapShellWidget::IsCursorOverMap() const
{
    if (!IsVisible() || !CenterHost || !FSlateApplication::IsInitialized()) return false;
    const FVector2D CursorPosition = FSlateApplication::Get().GetCursorPos();
    if(PlanPanel && PlanPanel->IsVisible() && PlanPanel->GetCachedGeometry().IsUnderLocation(CursorPosition))return false;
    if (!CenterHost->GetCachedGeometry().IsUnderLocation(CursorPosition) || Toolbar->GetCachedGeometry().IsUnderLocation(CursorPosition)) return false;
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
    if(Action==TEXT("open_video")){const int Id=Manager->GetRegistry()->GetPrimarySelectedDrone();if(Id>0)GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->OpenVideo(FString::Printf(TEXT("UAV-%02d"),Id));return;}
    if(Action==TEXT("security_plan")){PlanPanel->SetEditorEnabled(!PlanPanel->IsEditorEnabled());return;}
    if (!Manager.IsValid()) return;
    auto* Map = Manager->GetMapService(); if (!Map) return;
    if (Action == TEXT("map2d")) { Map->Enter2DMode(); Refresh(); return; }
    if (Action == TEXT("map3d")) { Map->Enter3DMode(); Refresh(); return; }
    if(PlanPanel && PlanPanel->IsEditorEnabled() && Action!=TEXT("focus")) {
        ActionStatus->SetText(ProductText::Source(TEXT("Use Security Plan Editor controls for this mission route.")));return;
    }
    bool bOk = false;
    if (Action == TEXT("focus")) bOk = Map->FocusDrone(Manager->GetRegistry()->GetPrimarySelectedDrone());
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
    if (!SelectionHighlight) return;
    auto* Registry = Manager.IsValid() ? Manager->GetRegistry() : nullptr;
    const APawn* Pawn = Registry ? Registry->GetSenderPawn(Registry->GetPrimarySelectedDrone()) : nullptr;
    FVector2D Position = FVector2D::ZeroVector;
    const bool bProjected = Pawn && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
        GetOwningPlayer(), Pawn->GetActorLocation(), Position, true);
    const FVector2D Size = Geometry.GetLocalSize();
    const bool bOnMap = bProjected && Position.X >= 24 && Position.X <= Size.X - 24
        && Position.Y >= 130 && Position.Y <= Size.Y - 24;
    SelectionHighlight->SetVisibility(bOnMap ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (bOnMap) CastChecked<UCanvasPanelSlot>(SelectionHighlight->Slot)->SetPosition(Position);
    const APawn* Assigned=Registry && PlanPanel?Registry->GetSenderPawn(PlanPanel->GetAssignedUAV()):nullptr;
    const bool MissionOnMap=Assigned && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),Assigned->GetActorLocation(),Position,true)
        && Position.X>=24 && Position.X<=Size.X-24 && Position.Y>=130 && Position.Y<=Size.Y-24;
    MissionHighlight->SetVisibility(MissionOnMap?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    if(MissionOnMap)CastChecked<UCanvasPanelSlot>(MissionHighlight->Slot)->SetPosition(Position);
}
