#include "Map/MapPlanMoveWidget.h"
#include "Shared/PlanWidgetSupport.h"
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
using namespace PlanUI;
void UMapPlanMoveWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Root;
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto* Panel=WidgetTree->ConstructWidget<UBorder>();CommandTheme::Panel(Panel,CommandTheme::Elevated,CommandTheme::Warning);Panel->SetPadding(FMargin(16));
    auto* Box=WidgetTree->ConstructWidget<UVerticalBox>();Panel->SetContent(Box);
    Summary=Label(WidgetTree,Box,FText::GetEmpty(),22);
    Confirm=Button(WidgetTree,Box,TEXT("confirm"),TEXT("Geometry.Confirm"));Cancel=Button(WidgetTree,Box,TEXT("cancel"),TEXT("Common.Cancel"));
    Confirm->OnAction.AddDynamic(this,&UMapPlanMoveWidget::Action);Cancel->OnAction.AddDynamic(this,&UMapPlanMoveWidget::Action);
    auto* PanelSlot=Root->AddChildToCanvas(Panel);PanelSlot->SetPosition(FVector2D(12,190));PanelSlot->SetSize(FVector2D(510,250));
    Handle=WidgetTree->ConstructWidget<UButton>();Handle->SetBackgroundColor(FLinearColor(1,.65f,.1f,.6f));
    auto* HandleText=WidgetTree->ConstructWidget<UTextBlock>();HandleText->SetText(FText::AsCultureInvariant(TEXT("+")));HandleText->SetJustification(ETextJustify::Center);Handle->SetContent(HandleText);
    Handle->SetToolTipText(T(TEXT("Geometry.Drag")));
    auto* HandleSlot=Root->AddChildToCanvas(Handle);HandleSlot->SetSize(FVector2D(60));HandleSlot->SetAlignment(FVector2D(.5f));
    GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->OnMapRouteEditRequested.AddUObject(this,&UMapPlanMoveWidget::Requested);
    SetVisibility(ESlateVisibility::Collapsed);
}
void UMapPlanMoveWidget::Requested(const TSharedPtr<FJsonObject>& Request) {
    if(Field(Request,TEXT("mode"))!=TEXT("MOVE") || bPending || IsMoving())return;
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    auto* C=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();
    PlanId=Field(Request,TEXT("plan_id"));MissionId=Field(Request,TEXT("mission_id"));
    const auto Plan=Find(Sync->GetPlans(),TEXT("plans"),PlanId);if(!Plan)return;PlanName=Field(Plan,TEXT("name"));
    ErrorCode.Empty();Routes.Empty();Delta=FVector::ZeroVector;
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if(!C || !ICoordinateService::Execute_IsCoordinateSystemReady(C)){ErrorCode=TEXT("COORDINATES_NOT_READY");Refresh();return;}
    FBox Bounds(ForceInit);
    for(const auto& Mid:Plan->GetArrayField(TEXT("mission_ids"))) {
        const auto Mission=Find(Sync->GetPlans(),TEXT("missions"),Mid->AsString());const auto Rid=Field(Mission,TEXT("route_id"));
        const auto Route=Find(Sync->GetPlans(),TEXT("paths"),Rid);if(!Route)continue;
        FRoutePreview Item;Item.Id=Rid;Item.Saved=Route;Route->TryGetBoolField(TEXT("bClosedLoop"),Item.Closed);
        for(const auto& V:Route->GetArrayField(TEXT("waypoints"))) {const auto P=V->AsObject();const auto W=ICoordinateService::Execute_GeographicToWorld(C,P->GetNumberField(TEXT("latitude")),P->GetNumberField(TEXT("longitude")),P->GetNumberField(TEXT("altitude")));Item.Original.Add(W);Bounds+=W;}
        Routes.Add(Item);
    }
    if(!Bounds.IsValid){ErrorCode=TEXT("ROUTE_TOO_SHORT");Refresh();return;}
    Anchor=Bounds.GetCenter();auto Body=MakeShared<FJsonObject>();Body->SetStringField(TEXT("action"),TEXT("begin_plan_move"));Body->SetStringField(TEXT("plan_id"),PlanId);Body->SetStringField(TEXT("mission_id"),MissionId);
    bPending=true;const auto Weak=TWeakObjectPtr<UMapPlanMoveWidget>(this);
    if(!Sync->SubmitPlan(Body,[Weak,Bounds](TSharedPtr<FJsonObject> Reply){if(!Weak.IsValid())return;auto* Self=Weak.Get();Self->bPending=false;Self->ErrorCode=PlanUI::Error(Reply);if(Self->ErrorCode.IsEmpty()){Self->SessionId=Field(Reply,TEXT("edit_session_id"));if(auto* PC=Cast<ADroneOpsPlayerController>(Self->GetOwningPlayer())){PC->ClearEditingPaths();if(PC->GetCommandScreenManager())PC->GetCommandScreenManager()->GetMapService()->FocusExecutionBounds(Bounds);}}Self->Refresh();})){bPending=false;ErrorCode=TEXT("SYNC_OFFLINE");}
    Refresh();
}
void UMapPlanMoveWidget::Refresh() {
    if(!Summary)return;auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    const auto Sessions=Object(S->GetPlans(),TEXT("edit_sessions"));
    if(IsMoving() && !bPending && S->IsReady() && Sessions && !Sessions->HasField(SessionId)){ErrorCode=TEXT("EDIT_SESSION_EXPIRED");}
    Summary->SetText(FText::Format(User(TEXT("{0}\n{1}\n{2}")),FText::Format(T(TEXT("Geometry.Moving")),User(PlanName)),T(TEXT("Geometry.Drag")),ErrorCode.IsEmpty()?(bPending?T(TEXT("Common.Waiting")):T(TEXT("Geometry.Preview"))):ProductText::Get(TEXT("Errors.")+ErrorCode)));
    if(Handle){Handle->SetToolTipText(T(TEXT("Geometry.Drag")));Handle->SetIsEnabled(!bPending);}
    Confirm->SetIsEnabled(IsMoving() && !bPending && S->IsReady() && ErrorCode!=TEXT("EDIT_SESSION_EXPIRED"));Cancel->SetIsEnabled(!bPending);
    InvalidateLayoutAndVolatility();
}
void UMapPlanMoveWidget::Leave(){SessionId.Empty();Routes.Empty();Delta=FVector::ZeroVector;bDragging=false;SetVisibility(ESlateVisibility::Collapsed);}
void UMapPlanMoveWidget::Action(FName Name,int32) {
    if(bPending)return;if(SessionId.IsEmpty()){Leave();return;}
    auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();auto R=MakeShared<FJsonObject>();
    R->SetStringField(TEXT("action"),Name==TEXT("confirm")?TEXT("move_plan"):TEXT("cancel_plan_move"));R->SetStringField(TEXT("plan_id"),PlanId);R->SetStringField(TEXT("mission_id"),MissionId);R->SetStringField(TEXT("edit_session_id"),SessionId);
    if(Name==TEXT("confirm")) {
        auto* C=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();if(!C || !ICoordinateService::Execute_IsCoordinateSystemReady(C)){ErrorCode=TEXT("COORDINATES_NOT_READY");Refresh();return;}
        auto Paths=MakeShared<FJsonObject>();
        for(const auto& Route:Routes){auto New=MakeShared<FJsonObject>();New->Values=Route.Saved->Values;TArray<TSharedPtr<FJsonValue>> Points;
            const auto& Saved=Route.Saved->GetArrayField(TEXT("waypoints"));
            for(int I=0;I<Route.Original.Num();++I){auto P=MakeShared<FJsonObject>();P->Values=Saved[I]->AsObject()->Values;
                const FVector W=Route.Original[I]+Delta,G=ICoordinateService::Execute_WorldToGeographic(C,W);
                P->SetNumberField(TEXT("longitude"),G.X);P->SetNumberField(TEXT("latitude"),G.Y);P->SetNumberField(TEXT("altitude"),G.Z);
                auto L=MakeShared<FJsonObject>();L->SetNumberField(TEXT("x"),W.X);L->SetNumberField(TEXT("y"),W.Y);L->SetNumberField(TEXT("z"),W.Z);P->SetObjectField(TEXT("location"),L);Points.Add(MakeShared<FJsonValueObject>(P));}
            New->SetArrayField(TEXT("waypoints"),Points);Paths->SetObjectField(Route.Id,New);
        }R->SetObjectField(TEXT("paths"),Paths);
    }
    bPending=true;bDragging=false;const auto Weak=TWeakObjectPtr<UMapPlanMoveWidget>(this);const bool Cancelling=Name==TEXT("cancel");
    if(!S->SubmitPlan(R,[Weak,Cancelling](TSharedPtr<FJsonObject> Reply){if(!Weak.IsValid())return;Weak->bPending=false;Weak->ErrorCode=PlanUI::Error(Reply);if(Weak->ErrorCode.IsEmpty() || (Cancelling && Weak->ErrorCode==TEXT("EDIT_SESSION_EXPIRED")))Weak->Leave();else Weak->Refresh();})){bPending=false;ErrorCode=TEXT("SYNC_OFFLINE");}Refresh();
}
bool UMapPlanMoveWidget::Project(const FVector& W,FVector2D& P) const {return UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),W,P,false);}
bool UMapPlanMoveWidget::CursorOnPlane(const FVector2D& Absolute,FVector& Point) const {
    const FVector2D Screen=GetCachedGeometry().AbsoluteToLocal(Absolute)*UWidgetLayoutLibrary::GetViewportScale(this);
    FVector O,D;if(!GetOwningPlayer()->DeprojectScreenPositionToWorld(Screen.X,Screen.Y,O,D) || FMath::Abs(D.Z)<1e-8)return false;
    const double T=(Anchor.Z-O.Z)/D.Z;if(T<0)return false;Point=O+D*T;return true;
}
void UMapPlanMoveWidget::NativeTick(const FGeometry& G,float Seconds){
    Super::NativeTick(G,Seconds);FVector2D P;
    if(Handle && IsMoving() && Project(Anchor+Delta,P))Cast<UCanvasPanelSlot>(Handle->Slot)->SetPosition(P);
}
FReply UMapPlanMoveWidget::NativeOnPreviewMouseButtonDown(const FGeometry&,const FPointerEvent& E){
    if(IsMoving() && !bPending && E.GetEffectingButton()==EKeys::LeftMouseButton && Handle->GetCachedGeometry().IsUnderLocation(E.GetScreenSpacePosition()) && CursorOnPlane(E.GetScreenSpacePosition(),DragStart)){
        DragDelta=Delta;bDragging=true;UE_LOG(LogTemp,Log,TEXT("[PlanMove] Handle drag started: %s"),*PlanId);return FReply::Handled().CaptureMouse(TakeWidget());
    }return FReply::Unhandled();
}
FReply UMapPlanMoveWidget::NativeOnMouseMove(const FGeometry&,const FPointerEvent& E){
    FVector P;if(bDragging && CursorOnPlane(E.GetScreenSpacePosition(),P)){Delta=DragDelta+P-DragStart;InvalidateLayoutAndVolatility();return FReply::Handled();}return FReply::Unhandled();
}
FReply UMapPlanMoveWidget::NativeOnMouseButtonUp(const FGeometry& G,const FPointerEvent& E){
    if(!bDragging)return FReply::Unhandled();NativeOnMouseMove(G,E);bDragging=false;
    UE_LOG(LogTemp,Log,TEXT("[PlanMove] Preview delta cm: %s"),*Delta.ToString());return FReply::Handled().ReleaseMouseCapture();
}
int32 UMapPlanMoveWidget::NativePaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& C,FSlateWindowElementList& Out,int32 L,const FWidgetStyle& S,bool E) const {
    const int Base=Super::NativePaint(A,G,C,Out,L,S,E);if(!IsMoving())return Base;
    const auto Color=FLinearColor(1,.65f,.1f);FBox2D Bounds(ForceInit);
    auto Line=[&](const TArray<FVector2D>& P,float Width){if(P.Num()>1)FSlateDrawElement::MakeLines(Out,Base+1,G.ToPaintGeometry(),P,ESlateDrawEffect::None,Color,true,Width);};
    for(const auto& Route:Routes){TArray<FVector2D> Points;for(int I=0;I<Route.Original.Num();++I){FVector2D P;if(!Project(Route.Original[I]+Delta,P))continue;Points.Add(P);Bounds+=P;
        FSlateDrawElement::MakeBox(Out,Base+2,G.ToPaintGeometry(FVector2D(12),FSlateLayoutTransform(P-FVector2D(6))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Color);
        FSlateDrawElement::MakeText(Out,Base+3,G.ToPaintGeometry(FVector2D(60,24),FSlateLayoutTransform(P+FVector2D(9,-12))),FText::AsNumber(I+1),FCoreStyle::GetDefaultFontStyle("Bold",16),ESlateDrawEffect::None,Color);}
        if(Route.Closed && Points.Num()>2)Points.Add(FVector2D(Points[0]));Line(Points,4);
        for(int I=1;I<Points.Num();++I){const auto D=(Points[I]-Points[I-1]).GetSafeNormal(),N=FVector2D(-D.Y,D.X),M=(Points[I]+Points[I-1])*.5;Line({M-D*9+N*5,M,M-D*9-N*5},3);}
    }
    if(Bounds.bIsValid){const auto Min=Bounds.Min-FVector2D(18),Max=Bounds.Max+FVector2D(18);Line({Min,FVector2D(Max.X,Min.Y),Max,FVector2D(Min.X,Max.Y),Min},1);}
    FVector2D P;if(Project(Anchor+Delta,P)){Line({P+FVector2D(-30,0),P+FVector2D(30,0)},5);Line({P+FVector2D(0,-30),P+FVector2D(0,30)},5);
        FSlateDrawElement::MakeText(Out,Base+3,G.ToPaintGeometry(FVector2D(180,28),FSlateLayoutTransform(P+FVector2D(32,0))),T(TEXT("Geometry.MovePlan")),FCoreStyle::GetDefaultFontStyle("Bold",18),ESlateDrawEffect::None,Color);}
    return Base+3;
}
