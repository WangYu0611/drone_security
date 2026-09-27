#include "Map/MapExecutionWidget.h"
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
#include "Styling/CoreStyle.h"
#include "RealTimeDroneReceiver.h"
using namespace PlanUI;
void UMapExecutionWidget::NativeOnInitialized(){
    Super::NativeOnInitialized();auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Root;
    auto* Panel=WidgetTree->ConstructWidget<UBorder>();CommandTheme::Panel(Panel);Panel->SetPadding(FMargin(12));
    auto* Box=WidgetTree->ConstructWidget<UVerticalBox>();Panel->SetContent(Box);
    Label(WidgetTree,Box,T(TEXT("Execution.MonitorNeutral")),24);Summary=Label(WidgetTree,Box,FText::GetEmpty(),22);
    auto* PanelSlot=Root->AddChildToCanvas(Panel);PanelSlot->SetPosition(FVector2D(12,190));PanelSlot->SetSize(FVector2D(520,270));
    SetVisibility(ESlateVisibility::HitTestInvisible);
}
void UMapExecutionWidget::Refresh(){
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    const auto SelectedPlan=Field(Sync->GetContext(),TEXT("active_security_plan_id"));
    Execution=ExecutionUI::Latest(Sync->GetPlans(),SelectedPlan,true);
    if(!Execution)Execution=ExecutionUI::Latest(Sync->GetPlans(),TEXT(""),true);
    if(!Execution)Execution=ExecutionUI::Latest(Sync->GetPlans(),SelectedPlan);
    if(Execution && !ExecutionUI::Active(Execution)){const auto Plan=Find(Sync->GetPlans(),TEXT("plans"),Field(Execution,TEXT("plan_id")));if(Plan && Field(Plan,TEXT("deployment_id"))!=Field(Execution,TEXT("deployment_id")))Execution.Reset();}
    SetVisibility(Execution?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    RenderedExecutions.Empty();if(!Execution){for(auto& Item:VisualRoutes)if(Item.Value)Item.Value->Destroy();VisualRoutes.Empty();VisualSnapshots.Empty();return;}
    const auto All=Object(Sync->GetPlans(),TEXT("executions"));
    if(All)for(const auto& Item:All->Values){const auto E=Item.Value->AsObject();if(ExecutionUI::Active(E) || (!ExecutionUI::Active(Execution) && Field(E,TEXT("group_id"))==Field(Execution,TEXT("group_id"))))RenderedExecutions.Add(E);}
    if(RenderedExecutions.IsEmpty())RenderedExecutions.Add(Execution);
    Summary->SetText(FText::Format(User(TEXT("{0}\n{1}\n{2}")),ExecutionUI::Summary(Execution),T(TEXT("Execution.ReadOnly")),T(Sync->IsReady()?(Execution->GetBoolField(TEXT("simulation"))?TEXT("Execution.Simulation"):TEXT("Execution.Real")):TEXT("Stage1.RECONNECTING"))));
    auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    auto* Coordinates=Registry->GetCoordinateService().GetObject();
    if(Coordinates && ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates)){
        for(const auto& E:RenderedExecutions){if(!E->GetBoolField(TEXT("simulation")))continue;int32 Drone=0;LexTryParseString(Drone,*Field(E,TEXT("uav_id")).Mid(4));
        if(auto* Mirror=Cast<ARealTimeDroneReceiver>(Registry->GetReceiverActor(Drone))){const auto P=Object(E,TEXT("position"));Mirror->ApplySimulationPosition(ICoordinateService::Execute_GeographicToWorld(Coordinates,P->GetNumberField(TEXT("latitude")),P->GetNumberField(TEXT("longitude")),P->GetNumberField(TEXT("altitude"))));}}
    }
    // Immutable acknowledged snapshots become depth-tested world route visuals.
    // No invented progress and no geometry updates from the presentation clock.
    if(Coordinates && ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates)){
        TSet<FString> Live;
        for(const auto& E:RenderedExecutions){const FString Key=Field(E,TEXT("execution_id"));Live.Add(Key);
            const auto R=Object(E,TEXT("route_snapshot"));if(!R)continue;
            FString Snapshot;FJsonSerializer::Serialize(R.ToSharedRef(),TJsonWriterFactory<>::Create(&Snapshot));
            auto& Path=VisualRoutes.FindOrAdd(Key);
            if(!IsValid(Path)){
                Path=GetWorld()->SpawnActor<ADronePathActor>();Path->bParticipatesInConflictChecks=false;
                Path->bSpawnWaypointHandlesAtRuntime=false;Path->bSpawnWaypointHandlesInEditor=false;
            }
            if(VisualSnapshots.FindRef(Key)!=Snapshot){
                Path->Waypoints.Empty();R->TryGetBoolField(TEXT("bClosedLoop"),Path->bClosedLoop);
                for(const auto& Point:R->GetArrayField(TEXT("waypoints"))){const auto P=Point->AsObject();FDroneWaypoint W;
                    W.Location=ICoordinateService::Execute_GeographicToWorld(Coordinates,P->GetNumberField(TEXT("latitude")),P->GetNumberField(TEXT("longitude")),P->GetNumberField(TEXT("altitude")));
                    double Speed=0;P->TryGetNumberField(TEXT("segmentSpeed"),Speed);W.SegmentSpeed=Speed;Path->Waypoints.Add(W);
                }Path->RefreshPath();VisualSnapshots.Add(Key,Snapshot);
            }
            Path->SetRoutePresentation(Field(E,TEXT("state"))==TEXT("COMPLETED")?ERouteVisualState::Completed:ExecutionUI::Active(E)?ERouteVisualState::Active:ERouteVisualState::Confirmed,E==Execution);
            Path->SetActorHiddenInGame(PC && PC->IsPathEditMode());
        }
        for(auto It=VisualRoutes.CreateIterator();It;++It)if(!Live.Contains(It.Key())){if(It.Value())It.Value()->Destroy();VisualSnapshots.Remove(It.Key());It.RemoveCurrent();}
    }
    const auto Id=Field(Execution,TEXT("execution_id"));
    // Consume automatic framing while editing; do not defer it until unlock.
    // Telemetry and execution presentation above keep refreshing normally.
    if(PC && PC->GetCommandScreenManager() && PC->GetCommandScreenManager()->GetMapService()->IsRouteEditCameraLocked())FocusedExecution=Id;
    if(Id!=FocusedExecution && PC && Coordinates && ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates) && PC->GetCommandScreenManager()) {
        FBox Bounds(ForceInit);const auto R=Object(Execution,TEXT("route_snapshot"));
        for(const auto& V:R->GetArrayField(TEXT("waypoints"))){const auto P=V->AsObject();Bounds+=ICoordinateService::Execute_GeographicToWorld(Coordinates,P->GetNumberField(TEXT("latitude")),P->GetNumberField(TEXT("longitude")),P->GetNumberField(TEXT("altitude")));}
        const auto Home=Object(Execution,TEXT("home_position"));Bounds+=ICoordinateService::Execute_GeographicToWorld(Coordinates,Home->GetNumberField(TEXT("latitude")),Home->GetNumberField(TEXT("longitude")),Home->GetNumberField(TEXT("altitude")));
        if(PC->GetCommandScreenManager()->GetMapService()->FocusExecutionBounds(Bounds))FocusedExecution=Id;
    }
    InvalidateLayoutAndVolatility();
}
int32 UMapExecutionWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const {
    const int32 Base=Super::NativePaint(Args,G,Clip,Out,Layer,Style,Enabled);if(!Execution)return Base;
    auto* C=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();if(!C || !ICoordinateService::Execute_IsCoordinateSystemReady(C))return Base;
    auto Project=[&](const TSharedPtr<FJsonObject>& P,FVector2D& Screen){
        const auto World=ICoordinateService::Execute_GeographicToWorld(C,P->GetNumberField(TEXT("latitude")),P->GetNumberField(TEXT("longitude")),P->GetNumberField(TEXT("altitude")));
        FVector2D View;if(!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),World,View,false))return false;
        // ProjectWorldLocationToWidgetPosition already returns DPI-adjusted viewport
        // coordinates. This widget fills that viewport; desktop window origins must
        // not be applied a second time (especially with side-by-side windows).
        Screen=View;return true;
    };
    for(const auto& Current:RenderedExecutions){
    const auto Route=Object(Current,TEXT("route_snapshot"));if(!Route)continue;
    const auto& Points=Route->GetArrayField(TEXT("waypoints"));TArray<FVector2D> Line;
    const int Completed=Current->GetNumberField(TEXT("completed_waypoints"));
    for(int I=0;I<Points.Num();++I){FVector2D P;if(!Project(Points[I]->AsObject(),P))continue;Line.Add(P);
        const FLinearColor Color=I<Completed?FLinearColor(.2f,1,.55f):I==Completed?FLinearColor(1,.75f,.1f):FLinearColor(.5f,.7f,1);
        FSlateDrawElement::MakeBox(Out,Base+2,G.ToPaintGeometry(FVector2D(12,12),FSlateLayoutTransform(P-FVector2D(6,6))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color);
        const FString Caption=FString::Printf(TEXT("WP%d %s"),I+1,I<Completed?TEXT("✓"):I==Completed?TEXT("●"):TEXT("○"));
        FSlateDrawElement::MakeText(Out,Base+3,G.ToPaintGeometry(FVector2D(130,24),FSlateLayoutTransform(P+FVector2D(10,-12))),Caption,FCoreStyle::GetDefaultFontStyle("Bold",14),ESlateDrawEffect::None,Color);
    }
    bool Closed=false;Route->TryGetBoolField(TEXT("bClosedLoop"),Closed);if(Closed && Line.Num()>2)Line.Add(FVector2D(Line[0]));
    // Route geometry is rendered by depth-tested Route V2 actors, not Slate lines.
    FVector2D UAV;if(Project(Object(Current,TEXT("position")),UAV)){
        const TArray<FVector2D> Diamond{UAV+FVector2D(0,-13),UAV+FVector2D(13,0),UAV+FVector2D(0,13),UAV+FVector2D(-13,0),UAV+FVector2D(0,-13)};
        FSlateDrawElement::MakeLines(Out,Base+4,G.ToPaintGeometry(),Diamond,ESlateDrawEffect::None,FLinearColor::Yellow,true,4);
        FSlateDrawElement::MakeText(Out,Base+4,G.ToPaintGeometry(FVector2D(140,24),FSlateLayoutTransform(UAV+FVector2D(16,12))),Field(Current,TEXT("uav_id")),FCoreStyle::GetDefaultFontStyle("Bold",16),ESlateDrawEffect::None,FLinearColor::Yellow);
    }
    }
    return Base+4;
}

void UMapExecutionWidget::NativeDestruct(){
    for(auto& Item:VisualRoutes)if(IsValid(Item.Value))Item.Value->Destroy();VisualRoutes.Empty();VisualSnapshots.Empty();
    Super::NativeDestruct();
}
