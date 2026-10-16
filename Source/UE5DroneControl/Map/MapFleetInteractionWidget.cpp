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
#include "Map/FleetTargeting.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/ProgressBar.h"
#include "Components/WidgetComponent.h"
#include "Map/FleetPanelStyle.h"
#include "RealTimeDroneReceiver.h"
using namespace PlanUI;

void UMapFleetInteractionWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);WidgetTree->RootWidget=Canvas;
    Panel=WidgetTree->ConstructWidget<UBorder>();FleetPanelStyle::Card(Panel);
    auto* PanelSlot=Canvas->AddChildToCanvas(Panel);PanelSlot->SetAnchors(FAnchors(1,0));PanelSlot->SetAlignment(FVector2D(1,0));PanelSlot->SetPosition(FVector2D(-20,196));PanelSlot->SetSize(FVector2D(420,700));
    auto* Content=WidgetTree->ConstructWidget<UVerticalBox>();Panel->SetContent(Content);
    auto Text=[&](UPanelWidget* Parent,const FText& Value,int Size,const FLinearColor& Color=CommandTheme::PrimaryText){auto* W=Label(WidgetTree,Parent,Value,Size);W->SetColorAndOpacity(Color);return W;};
    auto Space=[&](UPanelWidget* Parent,float Height){auto* S=WidgetTree->ConstructWidget<USizeBox>();S->SetHeightOverride(Height);Parent->AddChild(S);};
    auto MakeButton=[&](UPanelWidget* Parent,const TCHAR* Name,const TCHAR* Key,bool Primary=false,bool Danger=false){
        auto* B=Button(WidgetTree,Parent,Name,Key);B->OnAction.AddDynamic(this,&UMapFleetInteractionWidget::Action);Buttons.Add(Name,B);FleetPanelStyle::Button(B,Primary,Danger);return B;
    };
    CommandTheme::Text(Text(Content,T(TEXT("FleetUX.Title")),24),24,CommandTheme::PrimaryText,true);Space(Content,12);
    auto* StepRow=WidgetTree->ConstructWidget<UHorizontalBox>();Content->AddChild(StepRow);
    for(int I=0;I<3;++I){auto* Card=WidgetTree->ConstructWidget<UBorder>();Card->SetPadding(FMargin(8));
        auto* LayoutSlot=StepRow->AddChildToHorizontalBox(Card);LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));LayoutSlot->SetPadding(FMargin(I?4:0,0,0,0));
        auto* L=WidgetTree->ConstructWidget<UTextBlock>();CommandTheme::Text(L,14);L->SetJustification(ETextJustify::Center);Card->SetContent(L);Steps.Add(Card);StepLabels.Add(L);
    }Space(Content,14);
    auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();auto* ScrollSlot=Content->AddChildToVerticalBox(Scroll);ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* Body=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Body);
    Summary=Text(Body,FText::GetEmpty(),18);Space(Body,12);
    SelectionText=Text(Body,FText::GetEmpty(),17,CommandTheme::Cyan);Space(Body,18);
    HeightControls=WidgetTree->ConstructWidget<UVerticalBox>();Body->AddChild(HeightControls);
    Text(HeightControls,T(TEXT("Fleet3D.Height")),16,CommandTheme::SecondaryText);Space(HeightControls,6);
    TargetHeightInput=WidgetTree->ConstructWidget<UEditableTextBox>();CommandTheme::Input(TargetHeightInput);HeightControls->AddChild(TargetHeightInput);
    auto InputStyle=TargetHeightInput->GetWidgetStyle();InputStyle.TextStyle.SetFont(FCoreStyle::GetDefaultFontStyle("Bold",26));InputStyle.SetPadding(FMargin(12,8));
    TargetHeightInput->WidgetStyle=InputStyle;TargetHeightInput->SetWidgetStyle(TargetHeightInput->WidgetStyle);
    TargetHeightInput->OnTextChanged.AddDynamic(this,&UMapFleetInteractionWidget::TargetHeightChanged);
    Space(HeightControls,8);HeightDeltaText=Text(HeightControls,FText::GetEmpty(),16,CommandTheme::Cyan);
    Space(HeightControls,6);auto* SliderHeight=WidgetTree->ConstructWidget<USizeBox>();SliderHeight->SetHeightOverride(36);HeightControls->AddChild(SliderHeight);
    HeightSlider=WidgetTree->ConstructWidget<USlider>();HeightSlider->SetMinValue(-100);HeightSlider->SetMaxValue(100);HeightSlider->SetStepSize(1);HeightSlider->SetSliderBarColor(CommandTheme::Border);HeightSlider->SetSliderHandleColor(CommandTheme::Cyan);
    auto SliderStyle=HeightSlider->GetWidgetStyle();SliderStyle.SetBarThickness(4.f);
    auto Thumb=SliderStyle.NormalThumbImage;Thumb.ImageSize=FVector2D(16,24);SliderStyle.SetNormalThumbImage(Thumb).SetHoveredThumbImage(Thumb).SetDisabledThumbImage(Thumb);HeightSlider->SetWidgetStyle(SliderStyle);
    HeightSlider->SetToolTipText(T(TEXT("FleetUX.SliderHint")));HeightSlider->OnValueChanged.AddDynamic(this,&UMapFleetInteractionWidget::HeightSliderChanged);SliderHeight->SetContent(HeightSlider);
    auto* HeightButtons=WidgetTree->ConstructWidget<UHorizontalBox>();HeightControls->AddChild(HeightButtons);
    for(const auto& P:TArray<TPair<FString,FString>>{{TEXT("height_down"),TEXT("Fleet3D.Down")},{TEXT("height_up"),TEXT("Fleet3D.Up")},{TEXT("height_current"),TEXT("Fleet3D.Current")}}){
        auto* B=MakeButton(HeightButtons,*P.Key,*P.Value);auto* LayoutSlot=CastChecked<UHorizontalBoxSlot>(B->Slot);LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));LayoutSlot->SetPadding(FMargin(2,4));
    }
    TargetHeightInput->SetToolTipText(T(TEXT("FleetUX.HeightHint")));Space(HeightControls,12);
    TargetCard=WidgetTree->ConstructWidget<UBorder>();CommandTheme::Panel(TargetCard,CommandTheme::Background);TargetCard->SetPadding(FMargin(12));Body->AddChild(TargetCard);
    auto* TargetBody=WidgetTree->ConstructWidget<UVerticalBox>();TargetCard->SetContent(TargetBody);
    Text(TargetBody,T(TEXT("FleetUX.Preview")),15,CommandTheme::Cyan);Space(TargetBody,8);TravelText=Text(TargetBody,FText::GetEmpty(),20);
    Text(TargetBody,T(TEXT("FleetUX.Formation")),14,CommandTheme::SecondaryText);Space(TargetBody,8);
    MakeButton(TargetBody,TEXT("details"),TEXT("FleetUX.Coordinates"));
    TargetText=Text(TargetBody,FText::GetEmpty(),13,CommandTheme::SecondaryText);
    ProgressBox=WidgetTree->ConstructWidget<UVerticalBox>();Content->AddChild(ProgressBox);StateText=Text(ProgressBox,FText::GetEmpty(),19);
    Space(ProgressBox,10);auto* ProgressHeight=WidgetTree->ConstructWidget<USizeBox>();ProgressHeight->SetHeightOverride(8);ProgressBox->AddChild(ProgressHeight);
    Progress=WidgetTree->ConstructWidget<UProgressBar>();Progress->SetFillColorAndOpacity(CommandTheme::Cyan);ProgressHeight->SetContent(Progress);Space(ProgressBox,14);
    Space(Content,10);Result=Text(Content,FText::GetEmpty(),15,CommandTheme::SecondaryText);Space(Content,10);
    for(const auto& P:TArray<TPair<FString,FString>>{{TEXT("next"),TEXT("FleetUX.Next")},{TEXT("again"),TEXT("FleetUX.Again")},{TEXT("confirm"),TEXT("Fleet.Confirm")},{TEXT("pause"),TEXT("Execution.Pause")},{TEXT("resume"),TEXT("Execution.Resume")}}){
        auto* B=MakeButton(Content,*P.Key,*P.Value,true);CastChecked<UVerticalBoxSlot>(B->Slot)->SetPadding(FMargin(0,0,0,6));
    }
    auto* Secondary=WidgetTree->ConstructWidget<UHorizontalBox>();Content->AddChild(Secondary);
    for(const auto& P:TArray<TPair<FString,FString>>{{TEXT("fit_preview"),TEXT("FleetUX.Fit")},{TEXT("reselect"),TEXT("FleetUX.Reselect")},{TEXT("abort"),TEXT("Execution.Abort")},{TEXT("cancel"),TEXT("FleetUX.Exit")}}){
        auto* B=MakeButton(Secondary,*P.Key,*P.Value,false,P.Key==TEXT("abort"));auto* LayoutSlot=CastChecked<UHorizontalBoxSlot>(B->Slot);LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));LayoutSlot->SetPadding(FMargin(2,4));
        auto ButtonStyle=B->GetStyle();ButtonStyle.SetNormalPadding(FMargin(8,8)).SetPressedPadding(FMargin(8,8));B->SetStyle(ButtonStyle);
        auto Font=CastChecked<UTextBlock>(B->GetContent())->GetFont();Font.Size=14;CastChecked<UTextBlock>(B->GetContent())->SetFont(Font);
    }
    AbortPrompt=WidgetTree->ConstructWidget<UVerticalBox>();Content->AddChild(AbortPrompt);Space(AbortPrompt,8);Text(AbortPrompt,T(TEXT("FleetUX.AbortHint")),15,CommandTheme::Warning);Space(AbortPrompt,8);
    auto* AbortRow=WidgetTree->ConstructWidget<UHorizontalBox>();AbortPrompt->AddChild(AbortRow);
    for(const auto& P:TArray<TPair<FString,FString>>{{TEXT("abort_cancel"),TEXT("FleetUX.Keep")},{TEXT("abort_confirm"),TEXT("FleetUX.AbortConfirm")}}){auto* B=MakeButton(AbortRow,*P.Key,*P.Value,P.Key==TEXT("abort_confirm"),P.Key==TEXT("abort_confirm"));auto* LayoutSlot=CastChecked<UHorizontalBoxSlot>(B->Slot);LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));LayoutSlot->SetPadding(FMargin(2));}
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);Refresh();
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
    CancelGesture();ClearTarget();Mode=Value;HeightPrimary=0;MessageKey.Empty();ErrorDetails=FText::GetEmpty();bAbortConfirm=false;bShowCoordinates=false;DismissedExecution.Empty();
    if(auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());PC && PC->GetCommandScreenManager())PC->GetCommandScreenManager()->StopCameraFollow();
    Refresh();
}
bool UMapFleetInteractionWidget::CancelMode() {
    if(bPending || (!OwnsPrimaryInput() && !bHasTarget))return false;
    const FString Dismiss=ExecutionId;SetMode(EMapFleetMode::Browse);DismissedExecution=Dismiss;Refresh();return true;
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
bool UMapFleetInteractionWidget::Is3DView() const {
    const auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());const auto* Manager=PC?PC->GetCommandScreenManager():nullptr;
    const auto* Map=Manager?Manager->GetMapService():nullptr;return Map && Map->GetMapMode()==ECommandMapMode::Map3D;
}
bool UMapFleetInteractionWidget::TargetHeight(double& Height) const {
    const auto Members=OrderedSelection();FVector Anchor;if(Members.IsEmpty() || !Telemetry(Members[0],Anchor))return false;
    if(Is3DView()) {
        double Parsed=0;
        // Bound coordinates before building world/spline previews, using the
        // existing Backend 10 km local-frame domain rather than accepting overflow.
        if(!TargetHeightInput || !FleetTargeting::ParseHeight(TargetHeightInput->GetText().ToString(),Parsed) || FMath::Abs(Parsed-Anchor.Z)>10000.)return false;
        Height=Parsed;return true;
    }
    Height=Anchor.Z;return true;
}
void UMapFleetInteractionWidget::ResetHeight() {
    const auto Members=OrderedSelection();FVector Anchor;
    if(Members.IsEmpty() || !Telemetry(Members[0],Anchor))return;
    HeightPrimary=Members[0];TGuardValue<bool> Guard(bUpdatingHeight,true);
    TargetHeightInput->SetText(User(FString::SanitizeFloat(Anchor.Z)));
}
void UMapFleetInteractionWidget::TargetHeightChanged(const FText&) {
    if(bUpdatingHeight || bPending || Mode!=EMapFleetMode::GroupMove || !Is3DView())return;
    double Height=0;ErrorDetails=FText::GetEmpty();
    if(TargetHeight(Height) && bHasTarget){TargetGeographic.Z=Height;BuildPreview();MessageKey=TEXT("FleetUX.PreviewOnly");}
    Refresh();
}
void UMapFleetInteractionWidget::HeightSliderChanged(float Value) {
    if(bUpdatingHeight || bPending || Mode!=EMapFleetMode::GroupMove || !Is3DView() || !FMath::IsFinite(Value))return;
    const auto Members=OrderedSelection();FVector Anchor;if(Members.IsEmpty() || !Telemetry(Members[0],Anchor))return;
    {TGuardValue<bool> Guard(bUpdatingHeight,true);TargetHeightInput->SetText(User(FString::SanitizeFloat(Anchor.Z+FMath::RoundToInt(FMath::Clamp(Value,-100.f,100.f)))));}
    TargetHeightChanged(TargetHeightInput->GetText());
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
    double Height=0;if(!TargetHeight(Height)){MessageKey=TEXT("Fleet3D.InvalidHeight");return false;}
    auto* Coordinates=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();
    if(!Coordinates || !ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates)){MessageKey=TEXT("Errors.COORDINATES_NOT_READY");return false;}
    const FVector AnchorWorld=ICoordinateService::Execute_GeographicToWorld(Coordinates,Anchor.Y,Anchor.X,Anchor.Z);
    const FVector2D Pixel=Local*UWidgetLayoutLibrary::GetViewportScale(this);FVector Origin,Direction;
    FVector Point;if(!GetOwningPlayer()->DeprojectScreenPositionToWorld(Pixel.X,Pixel.Y,Origin,Direction) || !FleetTargeting::IntersectFlightPlane(Origin,Direction,AnchorWorld,Point)){MessageKey=TEXT("Map.InvalidPosition");return false;}
    const FVector Geo=ICoordinateService::Execute_WorldToGeographic(Coordinates,Point);
    if(Geo.ContainsNaN()){MessageKey=TEXT("Map.InvalidPosition");return false;}
    TargetGeographic=FVector(Geo.X,Geo.Y,Height);TargetMembers=Members;TargetPrimary=Members[0];bHasTarget=true;
    MessageKey=TEXT("FleetUX.PreviewOnly");BuildPreview();return true;
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
    const bool View3D=Is3DView();
    if(bHadMapMode && View3D!=bWas3D){CancelGesture();ClearTarget();HeightPrimary=0;MessageKey=Mode!=EMapFleetMode::Browse?TEXT("Fleet3D.ViewChanged"):TEXT("");ErrorDetails=FText::GetEmpty();}
    bHadMapMode=true;bWas3D=View3D;
    if(!IsAvailable() && Mode!=EMapFleetMode::Browse && !bPending){CancelGesture();ClearTarget();Mode=EMapFleetMode::Browse;bAbortConfirm=false;}
    const auto Selected=OrderedSelection();
    if(Mode==EMapFleetMode::GroupMove && Selected.Num()<2 && !bSelectionPending){CancelGesture();ClearTarget();Mode=EMapFleetMode::BoxSelect;HeightPrimary=0;}
    if(Mode==EMapFleetMode::GroupMove && View3D && !Selected.IsEmpty() && HeightPrimary!=Selected[0])ResetHeight();
    auto Accepted=Selected;Accepted.Sort();
    if(bSelectionPending && Accepted==RequestedSelection && GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetPrimarySelectedDrone()==RequestedPrimary) {
        bSelectionPending=false;if(MessageKey==TEXT("Fleet.SelectionPending"))MessageKey=TEXT("Fleet.SelectionReady");
    }
    if(bHasTarget && (Selected!=TargetMembers || Selected.IsEmpty() || Selected[0]!=TargetPrimary)){ClearTarget();MessageKey=TEXT("Fleet.SelectionChanged");}
    auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();const auto All=Object(S->GetPlans(),TEXT("executions"));TSharedPtr<FJsonObject> Latest;double Time=-1;
    TMap<FString,TArray<int32>> MembersByGroup;
    if(All)for(const auto& Item:All->Values){const auto E=Item.Value->AsObject();if(Field(E,TEXT("control_kind"))!=TEXT("GROUP_MOVE"))continue;
        int32 Id=0;LexTryParseString(Id,*Field(E,TEXT("uav_id")).Mid(4));MembersByGroup.FindOrAdd(Field(E,TEXT("group_id"))).AddUnique(Id);}
    for(auto& Item:MembersByGroup)Item.Value.Sort();
    auto Sorted=Selected;Sorted.Sort();
    if(All)for(const auto& Item:All->Values){const auto E=Item.Value->AsObject();const auto* Members=MembersByGroup.Find(Field(E,TEXT("group_id")));
        if(Field(E,TEXT("control_kind"))==TEXT("GROUP_MOVE") && Members && Sorted==*Members && E->GetNumberField(TEXT("created_at"))>Time){Latest=E;Time=E->GetNumberField(TEXT("created_at"));}}
    ExecutionId=Field(Latest,TEXT("execution_id"));ManagedGroup=Field(Latest,TEXT("group_id"));const FString State=Field(Latest,TEXT("state"));
    const bool Active=Latest && ExecutionUI::Active(Latest);if(!Active || (bAbortConfirm && ManagedGroup!=AbortGroup))bAbortConfirm=false;
    const bool Show=Mode!=EMapFleetMode::Browse || (Latest && ExecutionId!=DismissedExecution) || !MessageKey.IsEmpty() || !ErrorDetails.IsEmpty();
    Panel->SetVisibility(Show?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    const int Step=Mode==EMapFleetMode::BoxSelect || Selected.Num()<2?0:bHasTarget?2:1;
    Steps[0]->GetParent()->SetVisibility(Active || Mode==EMapFleetMode::Browse?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
    for(int I=0;I<Steps.Num();++I){CommandTheme::Panel(Steps[I],I==Step?CommandTheme::Selected:CommandTheme::Background,I<=Step?CommandTheme::Cyan:CommandTheme::Border);
        StepLabels[I]->SetText(T(*FString::Printf(TEXT("FleetUX.Step%d"),I+1)));StepLabels[I]->SetColorAndOpacity(I<=Step?CommandTheme::Cyan:CommandTheme::SecondaryText);}
    Summary->SetText(T(Active?TEXT("FleetUX.RunningHint"):Mode==EMapFleetMode::BoxSelect?(Selected.Num()>=2?TEXT("FleetUX.SelectionReady"):TEXT("FleetUX.SelectHint")):
        Mode==EMapFleetMode::GroupMove?(bHasTarget?TEXT("FleetUX.ReviewHint"):TEXT("FleetUX.PickHint")):TEXT("FleetUX.FinishedHint")));
    SelectionText->SetText(Selected.IsEmpty()?T(TEXT("FleetUX.EmptySelection")):FText::Format(T(TEXT("FleetUX.Selected")),FText::AsNumber(Selected.Num()),User(FString::Printf(TEXT("UAV-%02d"),Selected[0]))));
    SelectionText->SetVisibility(Mode!=EMapFleetMode::Browse || Active?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    TArray<FString> Ids;for(int Id:Selected)Ids.Add(FString::Printf(TEXT("UAV-%02d"),Id));SelectionText->SetToolTipText(User(FString::Join(Ids,TEXT(", "))));
    const FText Unavailable=UnavailableReason();if(bHasTarget && !Unavailable.IsEmpty())ClearTarget();
    double Height=0;const bool HeightValid=Active || !View3D || Mode!=EMapFleetMode::GroupMove || Selected.Num()<2 || TargetHeight(Height);
    HeightControls->SetVisibility(Mode==EMapFleetMode::GroupMove && View3D && Selected.Num()>=2 && !Active?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    FVector Anchor=FVector::ZeroVector;const bool HasAnchor=!Selected.IsEmpty() && Telemetry(Selected[0],Anchor);
    TargetHeightInput->SetIsEnabled(!bPending && !Active && HasAnchor);HeightSlider->SetIsEnabled(!bPending && !Active && Selected.Num()>=2 && HasAnchor);
    if(HasAnchor && HeightValid){const double Change=Height-Anchor.Z;FNumberFormattingOptions Format;Format.SetMaximumFractionalDigits(1);
        HeightDeltaText->SetText(FText::Format(T(Change>.01?TEXT("FleetUX.Rise"):Change<-.01?TEXT("FleetUX.Descend"):TEXT("FleetUX.Level")),FText::AsNumber(Anchor.Z,&Format),FText::AsNumber(FMath::Abs(Change),&Format)));
        {TGuardValue<bool> Guard(bUpdatingHeight,true);HeightSlider->SetValue(FMath::Clamp(float(Change),-100.f,100.f));}}
    TargetCard->SetVisibility(bHasTarget && !Active?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    TargetText->SetVisibility(bShowCoordinates?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(bHasTarget && HasAnchor){
        const double North=FMath::DegreesToRadians(TargetGeographic.Y-Anchor.Y)*6371000.;
        const double East=FMath::DegreesToRadians(FMath::UnwindDegrees(TargetGeographic.X-Anchor.X))*6371000.*FMath::Cos(FMath::DegreesToRadians((TargetGeographic.Y+Anchor.Y)*.5));
        const double Distance=FVector(North,East,TargetGeographic.Z-Anchor.Z).Size();double Speed=0;
        if(Field(S->GetPlans(),TEXT("execution_adapter"))==TEXT("Mock")){const auto C=Object(S->GetPlans(),TEXT("mock_execution"));if(C)C->TryGetNumberField(TEXT("default_speed_mps"),Speed);}
        TravelText->SetText(Speed>0?FText::Format(T(TEXT("FleetUX.Travel")),FText::AsNumber(FMath::RoundToInt(Distance)),FText::AsNumber(FMath::CeilToInt(Distance/Speed))):FText::Format(T(TEXT("FleetUX.Distance")),FText::AsNumber(FMath::RoundToInt(Distance))));
        TargetText->SetText(FText::Format(T(TEXT("FleetUX.TargetCoordinates")),User(FString::Printf(TEXT("%.6f"),TargetGeographic.Y)),User(FString::Printf(TEXT("%.6f"),TargetGeographic.X)),FText::AsNumber(TargetGeographic.Z)));
    }
    double ProgressSum=0;int Count=0;
    if(All && Latest)for(const auto& Item:All->Values){const auto E=Item.Value->AsObject();if(Field(E,TEXT("group_id"))==ManagedGroup){double P=0;E->TryGetNumberField(TEXT("progress"),P);ProgressSum+=P;++Count;}}
    ProgressBox->SetVisibility(Latest && (Mode==EMapFleetMode::Browse || Active)?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(Latest){const float P=Count?FMath::Clamp(float(ProgressSum/Count),0.f,1.f):0.f;Progress->SetPercent(P);
        const auto Color=State==TEXT("COMPLETED")?CommandTheme::Online:State==TEXT("PAUSED")?CommandTheme::Warning:State==TEXT("FAILED")?CommandTheme::Critical:CommandTheme::Cyan;
        Progress->SetFillColorAndOpacity(Color);StateText->SetColorAndOpacity(Color);StateText->SetText(FText::Format(T(TEXT("FleetUX.State")),T(*(TEXT("Execution.")+State)),FText::AsPercent(P)));}
    auto ShowButton=[&](const TCHAR* Name,bool Visible,bool Enabled=true){Buttons[Name]->SetVisibility(Visible?ESlateVisibility::Visible:ESlateVisibility::Collapsed);Buttons[Name]->SetIsEnabled(Enabled && !bPending);};
    ShowButton(TEXT("next"),Mode==EMapFleetMode::BoxSelect,Selected.Num()>=2 && Unavailable.IsEmpty());
    ShowButton(TEXT("again"),Mode==EMapFleetMode::Browse && Latest && !Active,Unavailable.IsEmpty() && IsAvailable());
    ShowButton(TEXT("confirm"),Mode==EMapFleetMode::GroupMove && !Active,bHasTarget && HeightValid && Unavailable.IsEmpty() && IsAvailable());
    ShowButton(TEXT("pause"),State==TEXT("EXECUTING") && !bAbortConfirm,S->IsReady());ShowButton(TEXT("resume"),State==TEXT("PAUSED") && !bAbortConfirm,S->IsReady());
    ShowButton(TEXT("abort"),Active && !bAbortConfirm,S->IsReady());ShowButton(TEXT("reselect"),Mode==EMapFleetMode::GroupMove && bHasTarget && !Active);
    ShowButton(TEXT("fit_preview"),Mode==EMapFleetMode::GroupMove && bHasTarget && !Active,IsAvailable());
    ShowButton(TEXT("cancel"),!bAbortConfirm);AbortPrompt->SetVisibility(bAbortConfirm?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    ShowButton(TEXT("abort_confirm"),bAbortConfirm,S->IsReady());ShowButton(TEXT("abort_cancel"),bAbortConfirm);
    for(auto Name:{TEXT("next"),TEXT("again"),TEXT("confirm"),TEXT("pause"),TEXT("resume"),TEXT("abort_confirm")})
        CastChecked<UTextBlock>(Buttons[Name]->GetContent())->SetColorAndOpacity(Buttons[Name]->GetIsEnabled()?CommandTheme::Background:CommandTheme::SecondaryText);
    for(auto Name:{TEXT("height_up"),TEXT("height_down"),TEXT("height_current")})Buttons[Name]->SetIsEnabled(!bPending && !Active && Selected.Num()>1 && HasAnchor);
    auto Caption=[&](const TCHAR* Name,const FText& Value){CastChecked<UTextBlock>(Buttons[Name]->GetContent())->SetText(Value);};
    Caption(TEXT("confirm"),bHasTarget?FText::Format(T(TEXT("FleetUX.Confirm")),FText::AsNumber(Selected.Num())):T(TEXT("FleetUX.NeedTarget")));
    Caption(TEXT("cancel"),T(Mode==EMapFleetMode::Browse?TEXT("FleetUX.Hide"):TEXT("FleetUX.Exit")));
    Caption(TEXT("details"),T(bShowCoordinates?TEXT("FleetUX.HideCoordinates"):TEXT("FleetUX.Coordinates")));
    const bool Blocked=!HeightValid || !ErrorDetails.IsEmpty() || (!Unavailable.IsEmpty() && (Mode!=EMapFleetMode::Browse || Latest) && !Active);
    Result->SetColorAndOpacity(Blocked?CommandTheme::Warning:CommandTheme::SecondaryText);
    Result->SetText(bPending?T(TEXT("Common.Waiting")):!ErrorDetails.IsEmpty()?ErrorDetails:
        (Mode!=EMapFleetMode::Browse || Latest) && !Active && !Unavailable.IsEmpty()?Unavailable:!HeightValid?T(TEXT("Fleet3D.InvalidHeight")):MessageKey.IsEmpty()?FText::GetEmpty():T(*MessageKey));
    InvalidateLayoutAndVolatility();
}
void UMapFleetInteractionWidget::Action(FName Name,int32) {
    if(bPending)return;
    if(Name==TEXT("next") || Name==TEXT("again")){if(UnavailableReason().IsEmpty())SetMode(EMapFleetMode::GroupMove);return;}
    if(Name==TEXT("reselect")){ClearTarget();ErrorDetails=FText::GetEmpty();MessageKey.Empty();Refresh();return;}
    if(Name==TEXT("details")){bShowCoordinates=!bShowCoordinates;Refresh();return;}
    if(Name==TEXT("fit_preview")){
        if(!bHasTarget || !IsAvailable())return;FBox Bounds(ForceInit);
        for(int Id:TargetMembers)if(const auto* Path=PreviewRoutes.Find(FString::FromInt(Id)))for(const auto& P:Path->Waypoints)Bounds+=P.Location;
        if(Bounds.IsValid)if(auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());PC && PC->GetCommandScreenManager())PC->GetCommandScreenManager()->GetMapService()->FocusExecutionBounds(Bounds);
        return;
    }
    if(Name==TEXT("abort_cancel")){bAbortConfirm=false;Refresh();return;}
    if(Name==TEXT("abort")){if(!ExecutionId.IsEmpty()){bAbortConfirm=true;AbortGroup=ManagedGroup;}Refresh();return;}
    if(Name==TEXT("abort_confirm")){if(!bAbortConfirm || AbortGroup!=ManagedGroup)return;bAbortConfirm=false;Name=TEXT("abort");}
    if(Name==TEXT("height_up") || Name==TEXT("height_down") || Name==TEXT("height_current")){
        if(bPending)return;
        if(Name==TEXT("height_current")){ResetHeight();TargetHeightChanged(TargetHeightInput->GetText());return;}
        double Height=0;if(!TargetHeight(Height)){const auto Members=OrderedSelection();FVector Anchor;if(Members.IsEmpty() || !Telemetry(Members[0],Anchor))return;Height=Anchor.Z;}
        {
            {TGuardValue<bool> Guard(bUpdatingHeight,true);TargetHeightInput->SetText(User(FString::SanitizeFloat(Height+(Name==TEXT("height_up")?10.:-10.))));}
            TargetHeightChanged(TargetHeightInput->GetText());
        }return;
    }
    if(Name==TEXT("cancel")){const FString Dismiss=ExecutionId;SetMode(EMapFleetMode::Browse);DismissedExecution=Dismiss;Refresh();return;}
    auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("request_id"),FGuid::NewGuid().ToString());
    if(Name==TEXT("confirm")) {
        ErrorDetails=UnavailableReason();if(!bHasTarget || !ErrorDetails.IsEmpty() || !IsAvailable()){Refresh();return;}
        double Height=0;if(!TargetHeight(Height)){MessageKey=TEXT("Fleet3D.InvalidHeight");Refresh();return;}
        TargetGeographic.Z=Height;
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
        const FString Code=PlanUI::Error(Reply);Self->MessageKey=Code.IsEmpty()?TEXT("FleetUX.Accepted"):TEXT("Errors.")+Code;Self->ErrorDetails=ReservationError(Reply);
        const auto Params=Object(Reply,TEXT("params"));if(Params && Params->HasField(TEXT("minimum_separation_m")))Self->ErrorDetails=FText::Format(T(TEXT("Group.Conflict")),User(Field(Params,TEXT("uav_a"))),User(Field(Params,TEXT("uav_b"))),FText::AsNumber(Params->GetNumberField(TEXT("minimum_separation_m"))),FText::AsNumber(Params->GetNumberField(TEXT("required_separation_m"))));
        if(Code.IsEmpty() && Moving){Self->ClearTarget();Self->Mode=EMapFleetMode::Browse;Self->HeightPrimary=0;Self->MessageKey.Empty();}Self->Refresh();
    })){bPending=false;MessageKey=TEXT("Errors.SYNC_OFFLINE");}Refresh();
}
void UMapFleetInteractionWidget::NativeTick(const FGeometry& G,float Delta) {
    Super::NativeTick(G,Delta);
    if(Panel){const float Available=FMath::Max(240.f,float(G.GetLocalSize().Y)-216.f);
        const float Desired=Mode==EMapFleetMode::BoxSelect?460.f:Mode==EMapFleetMode::GroupMove?(bHasTarget?800.f:690.f):bAbortConfirm?510.f:400.f;
        const FVector2D Size(FMath::Min(420.,G.GetLocalSize().X-32.),FMath::Min(Available,Desired));
        auto* LayoutSlot=CastChecked<UCanvasPanelSlot>(Panel->Slot);if(!LayoutSlot->GetSize().Equals(Size,.5))LayoutSlot->SetSize(Size);}
    const bool Markers=IsAvailable() && (Mode!=EMapFleetMode::Browse || !ManagedGroup.IsEmpty());
    if(!Markers){for(const auto& Item:SuppressedLabels)if(Item.Key.IsValid())Item.Key->SetVisibility(Item.Value);SuppressedLabels.Reset();return;}
    const auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    for(const auto& D:Registry->GetFriendlyDroneDescriptors())if(auto* Receiver=Cast<ARealTimeDroneReceiver>(Registry->GetReceiverActor(D.DroneId));Receiver && Receiver->NameLabelWidgetComponent){
        auto* Label=Receiver->NameLabelWidgetComponent.Get();if(!SuppressedLabels.Contains(Label))SuppressedLabels.Add(Label,Label->IsVisible());Label->SetVisibility(false);}
}
int32 UMapFleetInteractionWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const {
    const int32 Base=Super::NativePaint(Args,G,Clip,Out,Layer,Style,Enabled);auto* R=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    static const FSlateRoundedBoxBrush CardBrush(CommandTheme::Surface,7.f,CommandTheme::Border,1.f);
    static const FSlateRoundedBoxBrush RingBrush(FLinearColor(0,0,0,0),14.f,FLinearColor::White,2.f);
    static const FSlateRoundedBoxBrush DotBrush(FLinearColor::White,4.f);
    auto Rect=[&](const FVector2D& A,const FVector2D& B,const FLinearColor& Color){const FVector2D Min(FMath::Min(A.X,B.X),FMath::Min(A.Y,B.Y)),Max(FMath::Max(A.X,B.X),FMath::Max(A.Y,B.Y));
        FSlateDrawElement::MakeLines(Out,Base+1,G.ToPaintGeometry(),{Min,FVector2D(Max.X,Min.Y),Max,FVector2D(Min.X,Max.Y),Min},ESlateDrawEffect::None,Color,true,1.5f);};
    const bool Markers=IsAvailable() && (Mode!=EMapFleetMode::Browse || !ManagedGroup.IsEmpty());
    if(Markers){
        TArray<TPair<int32,FVector2D>> Points;for(const auto& D:R->GetFriendlyDroneDescriptors()){FVector2D P;if(Project(D.DroneId,P) && !IsOverControls(G.LocalToAbsolute(P)))Points.Emplace(D.DroneId,P);}
        Points.Sort([](const auto& A,const auto& B){return FMath::IsNearlyEqual(A.Value.Y,B.Value.Y,1.)?A.Key<B.Key:A.Value.Y<B.Value.Y;});
        TArray<FVector2D> Placed;
        for(const auto& Item:Points){const int Id=Item.Key;const FVector2D P=Item.Value;const bool Primary=Id==R->GetPrimarySelectedDrone(),Selected=R->GetMultiSelectedDrones().Contains(Id);
            const auto Color=Primary?CommandTheme::Warning:Selected?CommandTheme::Cyan:CommandTheme::SecondaryText;
            const float Radius=Selected?14.f:9.f;FSlateDrawElement::MakeBox(Out,Base+2,G.ToPaintGeometry(FVector2D(Radius*2),FSlateLayoutTransform(P-FVector2D(Radius))),&RingBrush,ESlateDrawEffect::None,Color);
            FSlateDrawElement::MakeBox(Out,Base+3,G.ToPaintGeometry(FVector2D(6),FSlateLayoutTransform(P-FVector2D(3))),&DotBrush,ESlateDrawEffect::None,Color);
            const FVector2D LabelSize(166,34);FVector2D Label=P+FVector2D(28,-18);
            if(Label.X+LabelSize.X>G.GetLocalSize().X-450)Label.X=P.X-LabelSize.X-28;
            Label.X=FMath::Clamp(Label.X,16.,G.GetLocalSize().X-LabelSize.X-16);Label.Y=FMath::Clamp(Label.Y,196.,G.GetLocalSize().Y-150.);
            for(const auto& Old:Placed)if(FMath::Abs(Label.X-Old.X)<LabelSize.X+8 && FMath::Abs(Label.Y-Old.Y)<LabelSize.Y+6)Label.Y=Old.Y+LabelSize.Y+6;
            if(Label.Y+LabelSize.Y>G.GetLocalSize().Y-18)continue;Placed.Add(Label);
            const FVector2D Connection(Label.X>P.X?Label.X:Label.X+LabelSize.X,Label.Y+LabelSize.Y*.5);
            FSlateDrawElement::MakeLines(Out,Base+1,G.ToPaintGeometry(),{P,Connection},ESlateDrawEffect::None,Color.CopyWithNewOpacity(.65f),true,1.f);
            FSlateDrawElement::MakeBox(Out,Base+2,G.ToPaintGeometry(LabelSize,FSlateLayoutTransform(Label)),&CardBrush,ESlateDrawEffect::None,CommandTheme::Surface);
            FVector Geo;const FText Caption=Telemetry(Id,Geo)?FText::Format(T(TEXT("FleetUX.Marker")),User(FString::Printf(TEXT("%02d"),Id)),FText::AsNumber(FMath::RoundToInt(Geo.Z))):User(FString::Printf(TEXT("UAV-%02d"),Id));
            FSlateDrawElement::MakeText(Out,Base+3,G.ToPaintGeometry(LabelSize,FSlateLayoutTransform(Label+FVector2D(9,6))),Caption,FCoreStyle::GetDefaultFontStyle("Bold",16),ESlateDrawEffect::None,Color);
        }
    }else for(int32 Id:R->GetMultiSelectedDrones()){FVector2D P;if(Project(Id,P))Rect(P-FVector2D(13),P+FVector2D(13),Id==R->GetPrimarySelectedDrone()?CommandTheme::Warning:CommandTheme::Cyan);}
    if(bDragging && Mode!=EMapFleetMode::GroupMove){const FVector2D Min(FMath::Min(Start.X,Current.X),FMath::Min(Start.Y,Current.Y)),Max(FMath::Max(Start.X,Current.X),FMath::Max(Start.Y,Current.Y));
        FSlateDrawElement::MakeBox(Out,Base+1,G.ToPaintGeometry(Max-Min,FSlateLayoutTransform(Min)),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,CommandTheme::Cyan.CopyWithNewOpacity(.1f));Rect(Start,Current,CommandTheme::Cyan);}
    if(bHasTarget){auto* Coordinates=R->GetCoordinateService().GetObject();FVector Anchor;
        if(Coordinates && Telemetry(TargetPrimary,Anchor)){
            const FVector Top=ICoordinateService::Execute_GeographicToWorld(Coordinates,TargetGeographic.Y,TargetGeographic.X,TargetGeographic.Z);
            FVector2D B;if(UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),Top,B,true) && !IsOverControls(G.LocalToAbsolute(B))){
                if(Is3DView()){const FVector Bottom=ICoordinateService::Execute_GeographicToWorld(Coordinates,TargetGeographic.Y,TargetGeographic.X,Anchor.Z);FVector2D A;
                    if(UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),Bottom,A,true))FSlateDrawElement::MakeLines(Out,Base+2,G.ToPaintGeometry(),{A,B},ESlateDrawEffect::None,CommandTheme::Cyan.CopyWithNewOpacity(.7f),true,1.5f);}
                FSlateDrawElement::MakeBox(Out,Base+3,G.ToPaintGeometry(FVector2D(28),FSlateLayoutTransform(B-FVector2D(14))),&RingBrush,ESlateDrawEffect::None,CommandTheme::Cyan);
                const FVector2D L=B+FVector2D(20,-44);FSlateDrawElement::MakeBox(Out,Base+3,G.ToPaintGeometry(FVector2D(200,34),FSlateLayoutTransform(L)),&CardBrush,ESlateDrawEffect::None,CommandTheme::Surface);
                const auto Caption=FText::Format(T(TEXT("Fleet3D.Marker")),FText::AsNumber(TargetGeographic.Z));
                FSlateDrawElement::MakeText(Out,Base+4,G.ToPaintGeometry(FVector2D(200,34),FSlateLayoutTransform(L+FVector2D(9,6))),Caption,FCoreStyle::GetDefaultFontStyle("Bold",16),ESlateDrawEffect::None,CommandTheme::Cyan);
            }
        }
    }
    return Base+4;
}
void UMapFleetInteractionWidget::NativeDestruct(){
    for(const auto& Item:SuppressedLabels)if(Item.Key.IsValid())Item.Key->SetVisibility(Item.Value);SuppressedLabels.Reset();
    CancelGesture();PreviewRoutes.Reset();Super::NativeDestruct();
}
