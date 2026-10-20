#include "Map/MapExecutionWidget.h"
#include "Map/FigmaMapStyle.h"
#include "Map/MapMoveSupport.h"
#include "Map/MapShellWidget.h"
#include "Map/PlanRouteVisualSet.h"
#include "PathEditor/DronePathActor.h"
#include "Shared/ExecutionPresentation.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Core/ICoordinateService.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandMapInteractionService.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Rendering/DrawElements.h"
#include "Layout/Clipping.h"
#include "Styling/CoreStyle.h"
#include "RealTimeDroneReceiver.h"
#include "Map/MapPaintGeometry.h"
using namespace PlanUI;
void UMapExecutionWidget::NativeOnInitialized(){
    Super::NativeOnInitialized();auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Root;
    auto* Panel=WidgetTree->ConstructWidget<UBorder>();SummaryPanel=Panel;MapUI::Panel(Panel);Panel->SetPadding(FMargin(12));
    auto* Box=WidgetTree->ConstructWidget<UVerticalBox>();Panel->SetContent(Box);
    MapUI::Text(Label(WidgetTree,Box,T(TEXT("Execution.MonitorNeutral")),24),20,MapUI::PrimaryText,true);Summary=Label(WidgetTree,Box,FText::GetEmpty(),22);MapUI::Text(Summary,15);
    auto* Chrome=MapUI::Chrome(WidgetTree,Root);MapUI::Place(Chrome,Panel,FVector2D(45,230),FVector2D(380,270));
    SetVisibility(ESlateVisibility::HitTestInvisible);
}
void UMapExecutionWidget::Refresh(){
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    auto* Coordinates=Registry->GetCoordinateService().GetObject();
    // Hydrate initial and terminal Mock positions even when no execution panel is
    // visible. Aircraft, selection and previews must use the same accepted source.
    if(Field(Sync->GetPlans(),TEXT("execution_adapter"))==TEXT("Mock") && Coordinates && ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates)) {
        const auto Uavs=Object(Object(Sync->GetPlans(),TEXT("mock_execution")),TEXT("uavs"));
        if(Uavs)for(const auto& Item:Uavs->Values){const FString Uav(Item.Key.ToView());int32 Drone=0;LexTryParseString(Drone,*Uav.Mid(4));
            // Received provenance remains authoritative after GPS/liveness loss.
            // The map must not rewind its actor to a configured Mock home.
            FDroneTelemetrySnapshot Snapshot;
            if(Registry->GetTelemetry(Drone,Snapshot) && Snapshot.TelemetrySource==TEXT("received"))continue;
            const auto P=ExecutionUI::MockPosition(Sync->GetPlans(),Uav);
            if(auto* Mirror=Cast<ARealTimeDroneReceiver>(Registry->GetReceiverActor(Drone));Mirror && P)Mirror->ApplySimulationPosition(ICoordinateService::Execute_GeographicToWorld(Coordinates,P->GetNumberField(TEXT("latitude")),P->GetNumberField(TEXT("longitude")),P->GetNumberField(TEXT("altitude"))));}
    }
    const auto SelectedPlan=Field(Sync->GetContext(),TEXT("active_security_plan_id"));
    Execution=ExecutionUI::Latest(Sync->GetPlans(),SelectedPlan,true);
    if(!Execution)Execution=ExecutionUI::Latest(Sync->GetPlans(),TEXT(""),true);
    if(!Execution)Execution=ExecutionUI::Latest(Sync->GetPlans(),SelectedPlan);
    if(Execution && !ExecutionUI::Active(Execution)){const auto Plan=Find(Sync->GetPlans(),TEXT("plans"),Field(Execution,TEXT("plan_id")));if(Plan && Field(Plan,TEXT("deployment_id"))!=Field(Execution,TEXT("deployment_id")))Execution.Reset();}
    SetVisibility(Execution?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    RenderedExecutions=ExecutionUI::RouteOwners(Sync->GetPlans(),SelectedPlan);
    if(!Execution && !RenderedExecutions.IsEmpty())Execution=RenderedExecutions[0];
    const FString Group=Field(Execution,TEXT("group_id"));
    const FString Id=Group.IsEmpty()?Field(Execution,TEXT("execution_id")):TEXT("group/")+Group;
    if(Sync->IsHydrated() && !bInitialExecutionSnapshotObserved){
        bInitialExecutionSnapshotObserved=true;
        // A persisted execution is history, not a newly requested camera move.
        // Initial framing belongs to the current accepted fleet positions.
        FocusedExecution=Id;
    }
    if(!Execution){VisualRoutes.Reset();return;}
    Summary->SetText(FText::Format(User(TEXT("{0}\n{1}\n{2}")),ExecutionUI::Summary(Execution),T(TEXT("Execution.ReadOnly")),T(Sync->IsReady()?(Execution->GetBoolField(TEXT("simulation"))?TEXT("Execution.Simulation"):TEXT("Execution.Real")):TEXT("Stage1.RECONNECTING"))));
    auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());
    // Immutable acknowledged snapshots become depth-tested world route visuals.
    // No invented progress and no geometry updates from the presentation clock.
    if(Coordinates && ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates)){
        TSet<FString> Live;
        for(const auto& E:RenderedExecutions){if(IsRouteSuppressed(E))continue;const FString Key=Field(E,TEXT("execution_id"));Live.Add(Key);
            const auto R=Object(E,TEXT("route_snapshot"));if(!R)continue;
            const FString State=Field(E,TEXT("state"));
            const bool Running=ExecutionUI::Active(E) && State!=TEXT("CREATED") && State!=TEXT("SCHEDULED");
            const FString SelectedMission=Field(Sync->GetContext(),TEXT("active_mission_id"));
            const bool Selected=!SelectedMission.IsEmpty()
                ? Field(E,TEXT("plan_id"))==SelectedPlan && Field(E,TEXT("mission_id"))==SelectedMission
                : Field(E,TEXT("uav_id"))==FString::Printf(TEXT("UAV-%02d"),Registry->GetPrimarySelectedDrone());
            auto* Path=VisualRoutes.Put(GetWorld(),Coordinates,Key,R,State==TEXT("COMPLETED")?ERouteVisualState::Completed:Running?ERouteVisualState::Active:ERouteVisualState::Confirmed,Selected);
            if(Path)Path->SetActorHiddenInGame(false);
        }
        VisualRoutes.Retain(Live);
    }
    if(!ExecutionUI::Active(Execution))FocusedExecution=Id;
    // Consume automatic framing while editing; do not defer it until unlock.
    // Telemetry and execution presentation above keep refreshing normally.
    if(PC && PC->GetCommandScreenManager() && PC->GetCommandScreenManager()->GetMapService()->IsRouteEditCameraLocked())FocusedExecution=Id;
    if(bInitialExecutionSnapshotObserved && Id!=FocusedExecution && PC && Coordinates && ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates) && PC->GetCommandScreenManager()) {
        FBox Bounds(ForceInit);
        // One group is one camera action. Using the latest member alone can
        // frame out its peers, especially with a wider startup formation.
        const auto All=Object(Sync->GetPlans(),TEXT("executions"));
        if(All)for(const auto& Entry:All->Values){const auto E=Entry.Value->AsObject();
            if(E!=Execution && (Group.IsEmpty() || Field(E,TEXT("group_id"))!=Group))continue;
            const auto Route=Object(E,TEXT("route_snapshot"));
            if(Route)for(const auto& V:Route->GetArrayField(TEXT("waypoints"))){const auto P=V->AsObject();
                Bounds+=ICoordinateService::Execute_GeographicToWorld(Coordinates,P->GetNumberField(TEXT("latitude")),P->GetNumberField(TEXT("longitude")),EllipsoidAltitude(P));}
            if(const auto Home=Object(E,TEXT("home_position")))Bounds+=ICoordinateService::Execute_GeographicToWorld(Coordinates,Home->GetNumberField(TEXT("latitude")),Home->GetNumberField(TEXT("longitude")),Home->GetNumberField(TEXT("altitude")));
            if(const auto Position=Object(E,TEXT("position")))Bounds+=ICoordinateService::Execute_GeographicToWorld(Coordinates,Position->GetNumberField(TEXT("latitude")),Position->GetNumberField(TEXT("longitude")),Position->GetNumberField(TEXT("altitude")));
        }
        // Free movement keeps the remaining accepted fleet available for the
        // next selection, including after a single-aircraft move.
        if(Field(Execution,TEXT("control_kind"))==TEXT("GROUP_MOVE"))
            for(const auto& Drone:Registry->GetFriendlyDroneDescriptors()){FVector Geographic=FVector::ZeroVector;
                if(MapMove::Position(Registry,Sync->GetPlans(),Drone.DroneId,Geographic))
                    Bounds+=ICoordinateService::Execute_GeographicToWorld(Coordinates,Geographic.Y,Geographic.X,Geographic.Z);}
        if(PC->GetCommandScreenManager()->GetMapService()->FocusExecutionBounds(Bounds))FocusedExecution=Id;
    }
    InvalidateLayoutAndVolatility();
}
int32 UMapExecutionWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const {
    const int32 Base=Super::NativePaint(Args,G,Clip,Out,Layer,Style,Enabled);if(!Execution)return Base;
    const FGeometry Desktop=MapPaint::DesktopGeometry(G,Args);
    auto* C=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();if(!C || !ICoordinateService::Execute_IsCoordinateSystemReady(C))return Base;
    const auto* PaintPC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());const auto* FrameShell=PaintPC && PaintPC->GetCommandScreenManager()?PaintPC->GetCommandScreenManager()->GetMapShell():nullptr;
    if(FrameShell)Out.PushClip(FSlateClippingZone(MapPaint::ClipGeometry(G,Desktop,FrameShell->GetMapViewportGeometry())));
    auto Project=[&](const TSharedPtr<FJsonObject>& P,FVector2D& Screen){
        const auto World=ICoordinateService::Execute_GeographicToWorld(C,P->GetNumberField(TEXT("latitude")),P->GetNumberField(TEXT("longitude")),P->GetNumberField(TEXT("altitude")));
        FVector2D View;if(!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),World,View,false))return false;
        // ProjectWorldLocationToWidgetPosition already returns DPI-adjusted viewport
        // coordinates. This widget fills that viewport; desktop window origins must
        // not be applied a second time (especially with side-by-side windows).
        Screen=View;const auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());const auto* Shell=PC && PC->GetCommandScreenManager()?PC->GetCommandScreenManager()->GetMapShell():nullptr;
        return !Shell || Shell->IsPointOverMap(Desktop.LocalToAbsolute(Screen));
    };
    for(const auto& Current:RenderedExecutions){
    if(IsRouteSuppressed(Current))continue;
    // The fleet card and its decluttered UAV markers own this group's HUD.
    // Immutable Route V2 bodies above remain visible and authoritative.
    if(!ManagedFleetGroup.IsEmpty() && Field(Current,TEXT("group_id"))==ManagedFleetGroup)continue;
    const auto Route=Object(Current,TEXT("route_snapshot"));if(!Route)continue;
    const auto& Points=Route->GetArrayField(TEXT("waypoints"));
    const int Completed=Current->GetNumberField(TEXT("completed_waypoints"));
    for(int I=0;I<Points.Num();++I){FVector2D P;if(!Project(Points[I]->AsObject(),P))continue;
        const FLinearColor Color=I<Completed?FLinearColor(.2f,1,.55f):I==Completed?FLinearColor(1,.75f,.1f):FLinearColor(.5f,.7f,1);
        FSlateDrawElement::MakeBox(Out,Base+2,G.ToPaintGeometry(FVector2D(12,12),FSlateLayoutTransform(P-FVector2D(6,6))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color);
        const FString Caption=FString::Printf(TEXT("WP%d %s"),I+1,I<Completed?TEXT("✓"):I==Completed?TEXT("●"):TEXT("○"));
        FSlateDrawElement::MakeText(Out,Base+3,G.ToPaintGeometry(FVector2D(130,24),FSlateLayoutTransform(P+FVector2D(10,-12))),Caption,FCoreStyle::GetDefaultFontStyle("Bold",14),ESlateDrawEffect::None,Color);
    }
    // Route geometry is rendered by depth-tested Route V2 actors, not Slate lines.
    const FString UAVId=Field(Current,TEXT("uav_id"));int32 RegisteredId=0;if(UAVId.StartsWith(TEXT("UAV-")))LexTryParseString(RegisteredId,*UAVId.Mid(4));
    auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();FVector RegisteredPosition=FVector::ZeroVector;
    const bool FleetOwnsLabel=Registry->IsDroneRegistered(RegisteredId) && MapMove::Position(Registry,GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),RegisteredId,RegisteredPosition);
    // Fleet owns registered UAV symbols and shows selection plus accepted execution
    // state. Keep the task route/waypoint colors above and unknown-UAV fallback.
    FVector2D UAV;if(!FleetOwnsLabel && Project(Object(Current,TEXT("position")),UAV)){
        const TArray<FVector2D> Diamond{UAV+FVector2D(0,-13),UAV+FVector2D(13,0),UAV+FVector2D(0,13),UAV+FVector2D(-13,0),UAV+FVector2D(0,-13)};
        FSlateDrawElement::MakeLines(Out,Base+4,G.ToPaintGeometry(),Diamond,ESlateDrawEffect::None,FLinearColor::Yellow,true,4);
        FSlateDrawElement::MakeText(Out,Base+4,G.ToPaintGeometry(FVector2D(140,24),FSlateLayoutTransform(UAV+FVector2D(16,12))),Field(Current,TEXT("uav_id")),FCoreStyle::GetDefaultFontStyle("Bold",16),ESlateDrawEffect::None,FLinearColor::Yellow);
    }
    }
    if(FrameShell)Out.PopClip();return Base+4;
}

void UMapExecutionWidget::NativeDestruct(){
    VisualRoutes.Reset();
    Super::NativeDestruct();
}
void UMapExecutionWidget::SetManagedFleetGroup(const FString& Group,const TArray<int32>& PreviewMembers){
    FleetPreviewUavs.Reset();for(int Id:PreviewMembers)FleetPreviewUavs.Add(FString::Printf(TEXT("UAV-%02d"),Id));
    ManagedFleetGroup=Group;if(SummaryPanel)SummaryPanel->SetVisibility(Execution && !Group.IsEmpty() && Field(Execution,TEXT("group_id"))==Group?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
    for(const auto& E:RenderedExecutions)if(auto* Path=VisualRoutes.Find(Field(E,TEXT("execution_id"))))Path->SetActorHiddenInGame(IsRouteSuppressed(E));
}

bool UMapExecutionWidget::IsRouteSuppressed(const TSharedPtr<FJsonObject>& E) const {
    if(Field(E,TEXT("control_kind"))==TEXT("GROUP_MOVE") && !ExecutionUI::Active(E) && FleetPreviewUavs.Contains(Field(E,TEXT("uav_id"))))return true;
    return !SuppressedPlan.IsEmpty() && Field(E,TEXT("plan_id"))==SuppressedPlan
        && (bPlanMoving || Field(E,TEXT("mission_id"))==SuppressedMission);
}
bool UMapExecutionWidget::OwnsMission(const FString& Plan,const FString& Mission) const {
    for(const auto& E:RenderedExecutions)if(Field(E,TEXT("plan_id"))==Plan && Field(E,TEXT("mission_id"))==Mission){
        const auto* P=VisualRoutes.Find(Field(E,TEXT("execution_id")));
        if(IsValid(P) && !P->IsHidden())return true;
    }return false;
}

bool UMapExecutionWidget::IsOverControls(const FVector2D& Screen) const {return IsVisible() && SummaryPanel && SummaryPanel->IsVisible() && SummaryPanel->GetCachedGeometry().IsUnderLocation(Screen);}
