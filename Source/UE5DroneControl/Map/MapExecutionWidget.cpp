#include "Map/MapExecutionWidget.h"
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
    Label(WidgetTree,Box,T(TEXT("Execution.Monitor")),24);Summary=Label(WidgetTree,Box,FText::GetEmpty(),22);
    auto* PanelSlot=Root->AddChildToCanvas(Panel);PanelSlot->SetPosition(FVector2D(12,190));PanelSlot->SetSize(FVector2D(520,270));
    SetVisibility(ESlateVisibility::HitTestInvisible);
}
void UMapExecutionWidget::Refresh(){
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    Execution=ExecutionUI::Latest(Sync->GetPlans(),TEXT(""),true);
    if(!Execution)Execution=ExecutionUI::Latest(Sync->GetPlans());
    if(Execution && !ExecutionUI::Active(Execution)){const auto Plan=Find(Sync->GetPlans(),TEXT("plans"),Field(Execution,TEXT("plan_id")));if(Plan && Field(Plan,TEXT("deployment_id"))!=Field(Execution,TEXT("deployment_id")))Execution.Reset();}
    SetVisibility(Execution?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    if(!Execution)return;
    Summary->SetText(FText::Format(User(TEXT("{0}\n{1}\n{2}")),ExecutionUI::Summary(Execution),T(TEXT("Execution.ReadOnly")),T(Sync->IsReady()?TEXT("Execution.Simulation"):TEXT("Stage1.RECONNECTING"))));
    auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    auto* Coordinates=Registry->GetCoordinateService().GetObject();
    if(Coordinates && ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates)){
        int32 Drone=0;LexTryParseString(Drone,*Field(Execution,TEXT("uav_id")).Mid(4));
        if(auto* Mirror=Cast<ARealTimeDroneReceiver>(Registry->GetReceiverActor(Drone))){const auto P=Object(Execution,TEXT("position"));Mirror->ApplySimulationPosition(ICoordinateService::Execute_GeographicToWorld(Coordinates,P->GetNumberField(TEXT("latitude")),P->GetNumberField(TEXT("longitude")),P->GetNumberField(TEXT("altitude"))));}
    }
    const auto Id=Field(Execution,TEXT("execution_id"));
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
    const auto Route=Object(Execution,TEXT("route_snapshot"));if(!Route)return Base;
    const auto& Points=Route->GetArrayField(TEXT("waypoints"));TArray<FVector2D> Line;
    const int Completed=Execution->GetNumberField(TEXT("completed_waypoints"));
    for(int I=0;I<Points.Num();++I){FVector2D P;if(!Project(Points[I]->AsObject(),P))continue;Line.Add(P);
        const FLinearColor Color=I<Completed?FLinearColor(.2f,1,.55f):I==Completed?FLinearColor(1,.75f,.1f):FLinearColor(.5f,.7f,1);
        FSlateDrawElement::MakeBox(Out,Base+2,G.ToPaintGeometry(FVector2D(12,12),FSlateLayoutTransform(P-FVector2D(6,6))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color);
        const FString Caption=FString::Printf(TEXT("WP%d %s"),I+1,I<Completed?TEXT("✓"):I==Completed?TEXT("●"):TEXT("○"));
        FSlateDrawElement::MakeText(Out,Base+3,G.ToPaintGeometry(FVector2D(130,24),FSlateLayoutTransform(P+FVector2D(10,-12))),Caption,FCoreStyle::GetDefaultFontStyle("Bold",14),ESlateDrawEffect::None,Color);
    }
    bool Closed=false;Route->TryGetBoolField(TEXT("bClosedLoop"),Closed);if(Closed && Line.Num()>2)Line.Add(FVector2D(Line[0]));
    if(Line.Num()>1)FSlateDrawElement::MakeLines(Out,Base+1,G.ToPaintGeometry(),Line,ESlateDrawEffect::None,FLinearColor(.1f,.8f,1),true,3);
    FVector2D UAV;if(Project(Object(Execution,TEXT("position")),UAV)){
        const TArray<FVector2D> Diamond{UAV+FVector2D(0,-13),UAV+FVector2D(13,0),UAV+FVector2D(0,13),UAV+FVector2D(-13,0),UAV+FVector2D(0,-13)};
        FSlateDrawElement::MakeLines(Out,Base+4,G.ToPaintGeometry(),Diamond,ESlateDrawEffect::None,FLinearColor::Yellow,true,4);
        FSlateDrawElement::MakeText(Out,Base+4,G.ToPaintGeometry(FVector2D(140,24),FSlateLayoutTransform(UAV+FVector2D(16,12))),Field(Execution,TEXT("uav_id")),FCoreStyle::GetDefaultFontStyle("Bold",16),ESlateDrawEffect::None,FLinearColor::Yellow);
    }
    return Base+4;
}
