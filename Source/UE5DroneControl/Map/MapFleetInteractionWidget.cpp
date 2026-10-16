#include "Map/MapFleetInteractionWidget.h"
#include "Map/MapShellWidget.h"
#include "Map/MapPlanMoveWidget.h"
#include "Shared/ExecutionPresentation.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Core/ICoordinateService.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandMapInteractionService.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "GameFramework/Pawn.h"
using namespace PlanUI;

void UMapFleetInteractionWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);WidgetTree->RootWidget=Canvas;
    Panel=WidgetTree->ConstructWidget<UBorder>();CommandTheme::Panel(Panel);Panel->SetPadding(FMargin(12));
    auto* PanelSlot=Canvas->AddChildToCanvas(Panel);PanelSlot->SetAnchors(FAnchors(1,0));PanelSlot->SetAlignment(FVector2D(1,0));PanelSlot->SetPosition(FVector2D(-12,184));PanelSlot->SetAutoSize(true);
    auto* Content=WidgetTree->ConstructWidget<UVerticalBox>();Panel->SetContent(Content);
    auto Text=[&](int Size){auto* W=Label(WidgetTree,Content,FText::GetEmpty(),Size);W->SetWrapTextAt(300);W->SetMinDesiredWidth(300);return W;};
    Summary=Text(16);TargetText=Text(13);
    for(const auto& P:TArray<TPair<FString,FString>>{{TEXT("confirm"),TEXT("Fleet.Confirm")},{TEXT("pause"),TEXT("Execution.Pause")},{TEXT("resume"),TEXT("Execution.Resume")},{TEXT("abort"),TEXT("Execution.Abort")},{TEXT("cancel"),TEXT("Fleet.Browse")}}) {
        auto* B=Button(WidgetTree,Content,*P.Key,*P.Value);B->OnAction.AddDynamic(this,&UMapFleetInteractionWidget::Action);Buttons.Add(*P.Key,B);
    }
    Result=Text(13);SetVisibility(ESlateVisibility::SelfHitTestInvisible);Refresh();
}
bool UMapFleetInteractionWidget::IsAvailable() const {
    auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());
    auto* Manager=PC?PC->GetCommandScreenManager():nullptr;
    const auto* Move=Manager && Manager->GetMapShell()?Manager->GetMapShell()->GetMovePanel():nullptr;
    const auto* Map=Manager?Manager->GetMapService():nullptr;
    return Map && !Map->IsPlanning() && !Map->IsRouteEditCameraLocked() && !(Move && Move->IsMoving());
}
bool UMapFleetInteractionWidget::IsOverControls(const FVector2D& Screen) const {
    return Panel && Panel->IsVisible() && Panel->GetCachedGeometry().IsUnderLocation(Screen);
}
void UMapFleetInteractionWidget::SetMode(EMapFleetMode Value) {
    if(bPending || (Value!=EMapFleetMode::Browse && !IsAvailable()))return;
    CancelGesture();ClearTarget();Mode=Value;MessageKey.Empty();ErrorDetails=FText::GetEmpty();
    if(auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());PC && PC->GetCommandScreenManager())PC->GetCommandScreenManager()->StopCameraFollow();
    Refresh();
}
bool UMapFleetInteractionWidget::CancelMode() {
    if(bPending || (!OwnsPrimaryInput() && !bHasTarget))return false;
    SetMode(EMapFleetMode::Browse);return true;
}
TArray<int32> UMapFleetInteractionWidget::OrderedSelection() const {
    const auto* R=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();auto Selected=R->GetMultiSelectedDrones();Selected.Sort();
    const int32 Primary=R->GetPrimarySelectedDrone();if(Selected.Remove(Primary))Selected.Insert(Primary,0);return Selected;
}
bool UMapFleetInteractionWidget::Project(int32 Id,FVector2D& Point) const {
    const auto* R=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();const AActor* Display=R->GetReceiverActor(Id);
    if(!Display || Display->IsHidden())Display=R->GetSenderPawn(Id);
    if(!Display || Display->IsHidden())return false;
    return UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),Display->GetActorLocation(),Point,true)
        && FleetSelection::Contains(FVector2D::ZeroVector,GetCachedGeometry().GetLocalSize(),Point);
}
bool UMapFleetInteractionWidget::Telemetry(int32 Id,FVector& Geographic) const {
    const auto State=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans();
    // Mock positions are acknowledged runtime positions, or the configured Backend
    // home before the first execution. They need not have a live telemetry receiver.
    if(Field(State,TEXT("execution_adapter"))==TEXT("Mock")) {
        const FString Uav=FString::Printf(TEXT("UAV-%02d"),Id);
        const auto Position=ExecutionUI::MockPosition(State,Uav);
        double Lat,Lon,Alt;if(!Position || !Position->TryGetNumberField(TEXT("latitude"),Lat) || !Position->TryGetNumberField(TEXT("longitude"),Lon) || !Position->TryGetNumberField(TEXT("altitude"),Alt))return false;
        Geographic=FVector(Lon,Lat,Alt);return !Geographic.ContainsNaN();
    }
    FDroneTelemetrySnapshot T;if(!GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetTelemetry(Id,T) || !T.bGpsFix
        || T.Availability==EDroneAvailability::Offline)return false;
    Geographic=FVector(T.GpsLongitude,T.GpsLatitude,T.GpsAltitude);return !Geographic.ContainsNaN();
}
bool UMapFleetInteractionWidget::BeginPointer(const FVector2D& Screen,bool Shift,bool Control) {
    if(!IsAvailable() || bPending || (Mode==EMapFleetMode::Browse && !Shift))return false;
    if(!GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->IsReady())return false;
    CancelGesture();Start=Current=GetCachedGeometry().AbsoluteToLocal(Screen);bDragging=true;bPointerValid=true;
    SelectionOperation=Control?FleetSelection::EOperation::Toggle:Shift?FleetSelection::EOperation::Add:FleetSelection::EOperation::Replace;
    return true;
}
void UMapFleetInteractionWidget::MovePointer(const FVector2D& Screen,bool OverMap) {
    if(!bDragging)return;
    if(!OverMap || !IsAvailable()){CancelGesture();return;}
    Current=GetCachedGeometry().AbsoluteToLocal(Screen);InvalidateLayoutAndVolatility();
}
void UMapFleetInteractionWidget::EndPointer(const FVector2D& Screen,bool OverMap) {
    if(!bDragging)return;MovePointer(Screen,OverMap);if(!bDragging || !bPointerValid)return;
    bDragging=false;const bool Click=FVector2D::Distance(Start,Current)<=5.;
    if(Mode==EMapFleetMode::GroupMove){if(Click)PickTarget(Current);}
    else SelectInRectangle(Click);
    Refresh();
}
void UMapFleetInteractionWidget::CancelGesture() { bDragging=false;bPointerValid=false;InvalidateLayoutAndVolatility(); }
void UMapFleetInteractionWidget::SelectInRectangle(bool Click) {
    auto* R=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();TArray<int32> Hits;float Closest=24.f;int32 Nearest=0;
    for(const auto& D:R->GetFriendlyDroneDescriptors()) {FVector2D P;if(!Project(D.DroneId,P))continue;
        if(!Click && FleetSelection::Contains(Start,Current,P))Hits.Add(D.DroneId);
        if(Click && FVector2D::Distance(Current,P)<Closest){Closest=FVector2D::Distance(Current,P);Nearest=D.DroneId;}
    }
    if(Click && Nearest>0)Hits.Add(Nearest);
    const auto Selected=FleetSelection::Apply(R->GetMultiSelectedDrones(),Hits,SelectionOperation);
    const int32 Primary=Selected.Contains(R->GetPrimarySelectedDrone())?R->GetPrimarySelectedDrone():Selected.IsEmpty()?0:Selected[0];
    ClearTarget();MessageKey=TEXT("Fleet.SelectionPending");
    RequestedSelection=Selected;RequestedPrimary=Primary;
    bSelectionPending=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->SetLocalSelection(Primary,Selected);
    if(!bSelectionPending)MessageKey=TEXT("Errors.SYNC_OFFLINE");
}
void UMapFleetInteractionWidget::ClearTarget() { bHasTarget=false;TargetMembers.Empty();TargetPrimary=0;PreviewRoutes.Reset(); }
bool UMapFleetInteractionWidget::PickTarget(const FVector2D& Local) {
    ClearTarget();ErrorDetails=UnavailableReason();if(!ErrorDetails.IsEmpty())return false;
    const auto Members=OrderedSelection();FVector Anchor;if(Members.Num()<2 || !Telemetry(Members[0],Anchor))return false;
    auto* Coordinates=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();
    if(!Coordinates || !ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates)){MessageKey=TEXT("Errors.COORDINATES_NOT_READY");return false;}
    const FVector AnchorWorld=ICoordinateService::Execute_GeographicToWorld(Coordinates,Anchor.Y,Anchor.X,Anchor.Z);
    const FVector2D Pixel=Local*UWidgetLayoutLibrary::GetViewportScale(this);FVector Origin,Direction;
    if(!GetOwningPlayer()->DeprojectScreenPositionToWorld(Pixel.X,Pixel.Y,Origin,Direction) || FMath::Abs(Direction.Z)<1.e-8){MessageKey=TEXT("Map.InvalidPosition");return false;}
    const double Distance=(AnchorWorld.Z-Origin.Z)/Direction.Z;if(Distance<=0){MessageKey=TEXT("Map.InvalidPosition");return false;}
    const FVector Geo=ICoordinateService::Execute_WorldToGeographic(Coordinates,Origin+Direction*Distance);
    if(Geo.ContainsNaN()){MessageKey=TEXT("Map.InvalidPosition");return false;}
    TargetGeographic=FVector(Geo.X,Geo.Y,Anchor.Z);TargetMembers=Members;TargetPrimary=Members[0];bHasTarget=true;
    MessageKey=TEXT("Fleet.PreviewOnly");BuildPreview();return true;
}
void UMapFleetInteractionWidget::BuildPreview() {
    auto* Coordinates=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();FVector Anchor;
    if(!Coordinates || TargetMembers.IsEmpty() || !Telemetry(TargetMembers[0],Anchor))return;
    double Speed=0;const auto Config=Object(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),TEXT("mock_execution"));
    if(Config)Config->TryGetNumberField(TEXT("default_speed_mps"),Speed);
    TSet<FString> Live;
    for(int32 Id:TargetMembers){FVector From;if(!Telemetry(Id,From))continue;
        const FVector To(TargetGeographic.X+FMath::UnwindDegrees(From.X-Anchor.X),TargetGeographic.Y+From.Y-Anchor.Y,TargetGeographic.Z+From.Z-Anchor.Z);
        auto Point=[Speed](const FVector& P){auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("latitude"),P.Y);O->SetNumberField(TEXT("longitude"),FMath::UnwindDegrees(P.X));O->SetNumberField(TEXT("altitude"),P.Z);O->SetNumberField(TEXT("segmentSpeed"),Speed);O->SetNumberField(TEXT("waitTime"),0);return MakeShared<FJsonValueObject>(O);};
        auto Route=MakeShared<FJsonObject>();Route->SetBoolField(TEXT("bClosedLoop"),false);Route->SetArrayField(TEXT("waypoints"),{Point(From),Point(To)});
        const FString Key=FString::FromInt(Id);Live.Add(Key);PreviewRoutes.Put(GetWorld(),Coordinates,Key,Route,ERouteVisualState::Planning,true);
    }
    PreviewRoutes.Retain(Live);
}
FText UMapFleetInteractionWidget::UnavailableReason() const {
    if(bSelectionPending)return T(TEXT("Fleet.SelectionPending"));
    const auto Selected=OrderedSelection();if(Selected.Num()<2)return T(TEXT("Fleet.SelectTwo"));
    auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();if(!S->IsReady())return T(TEXT("Errors.SYNC_OFFLINE"));
    const auto All=Object(S->GetPlans(),TEXT("executions"));
    for(int32 Id:Selected){const FString Uav=FString::Printf(TEXT("UAV-%02d"),Id);
        if(All)for(const auto& Item:All->Values){const auto E=Item.Value->AsObject();if(Field(E,TEXT("uav_id"))==Uav && ExecutionUI::Active(E))
            return FText::Format(T(TEXT("Fleet.Occupied")),User(Uav),Field(E,TEXT("control_kind"))==TEXT("GROUP_MOVE")?T(TEXT("Group.Title")):User(Field(E,TEXT("plan_name"))));}
        FVector Geo;if(!Telemetry(Id,Geo))return FText::Format(T(TEXT("Fleet.TelemetryRequired")),User(Uav));
    }return FText::GetEmpty();
}
void UMapFleetInteractionWidget::Refresh() {
    if(!Summary)return;
    if(!IsAvailable() && Mode!=EMapFleetMode::Browse && !bPending){CancelGesture();ClearTarget();Mode=EMapFleetMode::Browse;}
    const auto Selected=OrderedSelection();
    auto Accepted=Selected;Accepted.Sort();
    if(bSelectionPending && Accepted==RequestedSelection && GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetPrimarySelectedDrone()==RequestedPrimary) {
        bSelectionPending=false;if(MessageKey==TEXT("Fleet.SelectionPending"))MessageKey=TEXT("Fleet.SelectionReady");
    }
    if(bHasTarget && (Selected!=TargetMembers || Selected.IsEmpty() || Selected[0]!=TargetPrimary)){ClearTarget();MessageKey=TEXT("Fleet.SelectionChanged");}
    auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();const auto All=Object(S->GetPlans(),TEXT("executions"));TSharedPtr<FJsonObject> Latest;double Time=-1;
    if(All)for(const auto& Item:All->Values){const auto E=Item.Value->AsObject();if(Field(E,TEXT("control_kind"))!=TEXT("GROUP_MOVE"))continue;
        TArray<int32> GroupMembers;for(const auto& Other:All->Values){const auto X=Other.Value->AsObject();if(Field(X,TEXT("group_id"))==Field(E,TEXT("group_id"))){int32 Id=0;LexTryParseString(Id,*Field(X,TEXT("uav_id")).Mid(4));GroupMembers.AddUnique(Id);}}
        auto Sorted=Selected;Sorted.Sort();GroupMembers.Sort();if(Sorted==GroupMembers && E->GetNumberField(TEXT("created_at"))>Time){Latest=E;Time=E->GetNumberField(TEXT("created_at"));}
    }
    ExecutionId=Field(Latest,TEXT("execution_id"));const FString State=Field(Latest,TEXT("state"));
    const bool Show=Mode!=EMapFleetMode::Browse || Latest.IsValid() || !MessageKey.IsEmpty();Panel->SetVisibility(Show?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    const FText ModeText=T(Mode==EMapFleetMode::BoxSelect?TEXT("Fleet.BoxHint"):Mode==EMapFleetMode::GroupMove?TEXT("Fleet.MoveHint"):TEXT("Fleet.BrowseHint"));
    Summary->SetText(FText::Format(T(TEXT("Fleet.Summary")),FText::AsNumber(Selected.Num()),ModeText));
    TargetText->SetText(bHasTarget?FText::Format(T(TEXT("Fleet.Target")),User(FString::Printf(TEXT("%.7f"),TargetGeographic.Y)),User(FString::Printf(TEXT("%.7f"),TargetGeographic.X)),FText::AsNumber(TargetGeographic.Z)):
        Latest?FText::Format(T(TEXT("Fleet.Execution")),T(*(TEXT("Execution.")+State))):FText::GetEmpty());
    const FText Unavailable=UnavailableReason();
    if(bHasTarget && !Unavailable.IsEmpty())ClearTarget();
    Buttons[TEXT("confirm")]->SetVisibility(Mode==EMapFleetMode::GroupMove?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    Buttons[TEXT("confirm")]->SetIsEnabled(bHasTarget && !bPending && Unavailable.IsEmpty() && IsAvailable());
    Buttons[TEXT("pause")]->SetVisibility(State==TEXT("EXECUTING")?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    Buttons[TEXT("resume")]->SetVisibility(State==TEXT("PAUSED")?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    Buttons[TEXT("abort")]->SetVisibility(State==TEXT("EXECUTING") || State==TEXT("PAUSED")?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    for(auto Name:{TEXT("pause"),TEXT("resume"),TEXT("abort")})Buttons[Name]->SetIsEnabled(!bPending && S->IsReady());
    Buttons[TEXT("cancel")]->SetIsEnabled(!bPending);
    Result->SetText(bPending?T(TEXT("Common.Waiting")):!ErrorDetails.IsEmpty()?ErrorDetails:Mode==EMapFleetMode::GroupMove && !Unavailable.IsEmpty()?Unavailable:MessageKey.IsEmpty()?FText::GetEmpty():T(*MessageKey));
    InvalidateLayoutAndVolatility();
}
void UMapFleetInteractionWidget::Action(FName Name,int32) {
    if(Name==TEXT("cancel")){SetMode(EMapFleetMode::Browse);return;}if(bPending)return;
    auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("request_id"),FGuid::NewGuid().ToString());
    if(Name==TEXT("confirm")) {
        ErrorDetails=UnavailableReason();if(!bHasTarget || !ErrorDetails.IsEmpty() || !IsAvailable()){Refresh();return;}
        if(OrderedSelection()!=TargetMembers){ClearTarget();MessageKey=TEXT("Fleet.SelectionChanged");Refresh();return;}
        R->SetStringField(TEXT("action"),TEXT("execution_group_move"));TArray<TSharedPtr<FJsonValue>> Members;
        for(int32 Id:TargetMembers)Members.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("UAV-%02d"),Id)));R->SetArrayField(TEXT("assigned_uav_ids"),Members);
        auto Target=MakeShared<FJsonObject>();Target->SetNumberField(TEXT("latitude"),TargetGeographic.Y);Target->SetNumberField(TEXT("longitude"),TargetGeographic.X);Target->SetNumberField(TEXT("altitude"),TargetGeographic.Z);R->SetObjectField(TEXT("target"),Target);
        // A large explicit envelope preserves the existing relative formation. No packing request.
        R->SetNumberField(TEXT("target_area_radius_m"),10000.);
    } else {
        const auto E=Find(S->GetPlans(),TEXT("executions"),ExecutionId);if(!E)return;
        R->SetStringField(TEXT("action"),TEXT("execution_")+Name.ToString());R->SetStringField(TEXT("execution_id"),ExecutionId);R->SetNumberField(TEXT("control_version"),E->GetNumberField(TEXT("control_version")));
    }
    bPending=true;ErrorDetails=FText::GetEmpty();const bool Moving=Name==TEXT("confirm");const auto Weak=TWeakObjectPtr<UMapFleetInteractionWidget>(this);
    if(!S->SubmitPlan(R,[Weak,Moving](TSharedPtr<FJsonObject> Reply){if(!Weak.IsValid())return;auto* Self=Weak.Get();Self->bPending=false;
        const FString Code=PlanUI::Error(Reply);Self->MessageKey=Code.IsEmpty()?TEXT("Group.Accepted"):TEXT("Errors.")+Code;Self->ErrorDetails=ReservationError(Reply);
        const auto Params=Object(Reply,TEXT("params"));if(Params && Params->HasField(TEXT("minimum_separation_m")))Self->ErrorDetails=FText::Format(T(TEXT("Group.Conflict")),User(Field(Params,TEXT("uav_a"))),User(Field(Params,TEXT("uav_b"))),FText::AsNumber(Params->GetNumberField(TEXT("minimum_separation_m"))),FText::AsNumber(Params->GetNumberField(TEXT("required_separation_m"))));
        if(Code.IsEmpty() && Moving)Self->ClearTarget();Self->Refresh();
    })){bPending=false;MessageKey=TEXT("Errors.SYNC_OFFLINE");}Refresh();
}
int32 UMapFleetInteractionWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const {
    const int32 Base=Super::NativePaint(Args,G,Clip,Out,Layer,Style,Enabled);auto* R=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    auto Rect=[&](const FVector2D& A,const FVector2D& B,const FLinearColor& Color){const FVector2D Min(FMath::Min(A.X,B.X),FMath::Min(A.Y,B.Y)),Max(FMath::Max(A.X,B.X),FMath::Max(A.Y,B.Y));FSlateDrawElement::MakeLines(Out,Base+1,G.ToPaintGeometry(),{Min,FVector2D(Max.X,Min.Y),Max,FVector2D(Min.X,Max.Y),Min},ESlateDrawEffect::None,Color,true,1.5f);};
    for(int32 Id:R->GetMultiSelectedDrones()){FVector2D P;if(Project(Id,P))Rect(P-FVector2D(13,13),P+FVector2D(13,13),Id==R->GetPrimarySelectedDrone()?CommandTheme::Warning:CommandTheme::Cyan);}
    if(bDragging && Mode!=EMapFleetMode::GroupMove)Rect(Start,Current,CommandTheme::Cyan);
    return Base+1;
}
void UMapFleetInteractionWidget::NativeDestruct(){CancelGesture();PreviewRoutes.Reset();Super::NativeDestruct();}
