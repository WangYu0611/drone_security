#include "Map/MapFleetInteractionWidget.h"
#include "Map/MapShellWidget.h"
#include "Map/MapPlanMoveWidget.h"
#include "Shared/ExecutionPresentation.h"
#include "Shared/PlanConfiguration.h"
#include "Shared/ActiveExecutionControlsWidget.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Core/ICoordinateService.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandMapInteractionService.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Rendering/DrawElements.h"
#include "Layout/Clipping.h"
#include "Styling/CoreStyle.h"
#include "GameFramework/Pawn.h"
#include "Map/FleetTargeting.h"
#include "Map/MapMoveSupport.h"
#include "Map/FigmaMapStyle.h"
#include "Map/MapLabelPlacement.h"
#include "Components/MeshComponent.h"
#include "CollisionQueryParams.h"
#include "Components/Image.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/ProgressBar.h"
#include "Components/WidgetComponent.h"
#include "Map/FleetPanelStyle.h"
#include "RealTimeDroneReceiver.h"
#include "Map/MapPaintGeometry.h"
#include "Map/MapSurfaceHeight.h"
#include <limits>
using namespace PlanUI;

void UMapFleetInteractionWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();
    MarkerIcon=MapUI::Icon(WidgetTree,TEXT("drone"),FVector2D(28));
    SelectionMarkerIcon=MapUI::Icon(WidgetTree,TEXT("selected-ring"),FVector2D(36));
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);WidgetTree->RootWidget=Canvas;
    Panel=WidgetTree->ConstructWidget<UBorder>();FleetPanelStyle::Card(Panel);
    auto* Chrome=MapUI::Chrome(WidgetTree,Canvas);MapUI::Place(Chrome,Panel,FVector2D(1024,230),FVector2D(372,548));
    auto* Content=WidgetTree->ConstructWidget<UVerticalBox>();Panel->SetContent(Content);
    auto Text=[&](UPanelWidget* Parent,const FText& Value,int Size,const FLinearColor& Color=MapUI::PrimaryText){auto* W=Label(WidgetTree,Parent,Value,Size);MapUI::Text(W,Size,Color);return W;};
    auto Space=[&](UPanelWidget* Parent,float Height){auto* S=WidgetTree->ConstructWidget<USizeBox>();S->SetHeightOverride(Height);Parent->AddChild(S);};
    auto MakeButton=[&](UPanelWidget* Parent,const TCHAR* Name,const TCHAR* Key,bool Primary=false,bool Danger=false){
        auto* B=Button(WidgetTree,Parent,Name,Key);B->OnAction.AddDynamic(this,&UMapFleetInteractionWidget::Action);Buttons.Add(Name,B);FleetPanelStyle::Button(B,Primary,Danger);return B;
    };
    PanelTitle=Text(Content,T(TEXT("FleetUX.Title")),24);MapUI::Text(PanelTitle,24,MapUI::PrimaryText,true);Space(Content,12);
    auto* StepRow=WidgetTree->ConstructWidget<UHorizontalBox>();Content->AddChild(StepRow);
    for(int I=0;I<3;++I){auto* Card=WidgetTree->ConstructWidget<UBorder>();Card->SetPadding(FMargin(8));
        auto* LayoutSlot=StepRow->AddChildToHorizontalBox(Card);LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));LayoutSlot->SetPadding(FMargin(I?4:0,0,0,0));
        auto* L=WidgetTree->ConstructWidget<UTextBlock>();MapUI::Text(L,14);L->SetJustification(ETextJustify::Center);Card->SetContent(L);Steps.Add(Card);StepLabels.Add(L);
    }Space(Content,14);
    auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();auto* ScrollSlot=Content->AddChildToVerticalBox(Scroll);ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* Body=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Body);
    Summary=Text(Body,FText::GetEmpty(),18);Space(Body,12);
    SelectionText=Text(Body,FText::GetEmpty(),17,MapUI::Cyan);Space(Body,18);
    HeightControls=WidgetTree->ConstructWidget<UVerticalBox>();Body->AddChild(HeightControls);
    HeightLabel=Text(HeightControls,T(TEXT("FleetTerrain.AGL")),16,MapUI::SecondaryText);
    MakeButton(HeightControls,TEXT("height_mode"),TEXT("FleetTerrain.UseAbsolute"));Space(HeightControls,6);
    TargetHeightInput=WidgetTree->ConstructWidget<UEditableTextBox>();MapUI::Input(TargetHeightInput);HeightControls->AddChild(TargetHeightInput);
    auto InputStyle=TargetHeightInput->GetWidgetStyle();InputStyle.TextStyle.SetFont(FCoreStyle::GetDefaultFontStyle("Bold",18));InputStyle.SetPadding(FMargin(12,8));
    TargetHeightInput->WidgetStyle=InputStyle;TargetHeightInput->SetWidgetStyle(TargetHeightInput->WidgetStyle);
    TargetHeightInput->OnTextChanged.AddDynamic(this,&UMapFleetInteractionWidget::TargetHeightChanged);
    Space(HeightControls,8);HeightDeltaText=Text(HeightControls,FText::GetEmpty(),16,MapUI::Cyan);
    TerrainInfo=Text(HeightControls,FText::GetEmpty(),14,MapUI::SecondaryText);
    Space(HeightControls,6);auto* SliderHeight=WidgetTree->ConstructWidget<USizeBox>();SliderHeight->SetHeightOverride(36);HeightControls->AddChild(SliderHeight);
    HeightSlider=WidgetTree->ConstructWidget<USlider>();HeightSlider->SetMinValue(-100);HeightSlider->SetMaxValue(100);HeightSlider->SetStepSize(1);HeightSlider->SetSliderBarColor(MapUI::Border);HeightSlider->SetSliderHandleColor(MapUI::Cyan);
    auto SliderStyle=HeightSlider->GetWidgetStyle();SliderStyle.SetBarThickness(4.f);
    auto Thumb=SliderStyle.NormalThumbImage;Thumb.ImageSize=FVector2D(16,24);SliderStyle.SetNormalThumbImage(Thumb).SetHoveredThumbImage(Thumb).SetDisabledThumbImage(Thumb);HeightSlider->SetWidgetStyle(SliderStyle);
    HeightSlider->SetToolTipText(T(TEXT("FleetUX.SliderHint")));HeightSlider->OnValueChanged.AddDynamic(this,&UMapFleetInteractionWidget::HeightSliderChanged);SliderHeight->SetContent(HeightSlider);
    auto* HeightButtons=WidgetTree->ConstructWidget<UHorizontalBox>();HeightControls->AddChild(HeightButtons);
    for(const auto& P:TArray<TPair<FString,FString>>{{TEXT("height_down"),TEXT("Fleet3D.Down")},{TEXT("height_up"),TEXT("Fleet3D.Up")},{TEXT("height_current"),TEXT("Fleet3D.Current")}}){
        auto* B=MakeButton(HeightButtons,*P.Key,*P.Value);auto* LayoutSlot=CastChecked<UHorizontalBoxSlot>(B->Slot);LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));LayoutSlot->SetPadding(FMargin(2,4));
    }
    auto* HeightTip=WidgetTree->ConstructWidget<UBorder>();MapUI::Panel(HeightTip);HeightTip->SetPadding(FMargin(12));
    auto* TipWidth=WidgetTree->ConstructWidget<USizeBox>();TipWidth->SetWidthOverride(360);HeightTip->SetContent(TipWidth);
    auto* TipText=WidgetTree->ConstructWidget<UTextBlock>();MapUI::Text(TipText,14);TipText->SetAutoWrapText(true);TipText->SetText(T(TEXT("FleetTerrain.HeightHint")));TipWidth->SetContent(TipText);
    TargetHeightInput->SetToolTip(HeightTip);Space(HeightControls,12);
    TargetCard=WidgetTree->ConstructWidget<UBorder>();MapUI::Panel(TargetCard,MapUI::Background);TargetCard->SetPadding(FMargin(12));Body->AddChild(TargetCard);
    auto* TargetBody=WidgetTree->ConstructWidget<UVerticalBox>();TargetCard->SetContent(TargetBody);
    Text(TargetBody,T(TEXT("FleetUX.Preview")),15,MapUI::Cyan);Space(TargetBody,8);TravelText=Text(TargetBody,FText::GetEmpty(),20);
    Text(TargetBody,T(TEXT("FleetUX.Formation")),14,MapUI::SecondaryText);Space(TargetBody,8);
    MakeButton(TargetBody,TEXT("details"),TEXT("FleetUX.Coordinates"));
    TargetText=Text(TargetBody,FText::GetEmpty(),13,MapUI::SecondaryText);
    ProgressBox=WidgetTree->ConstructWidget<UVerticalBox>();Content->AddChild(ProgressBox);StateText=Text(ProgressBox,FText::GetEmpty(),19);
    Space(ProgressBox,10);auto* ProgressHeight=WidgetTree->ConstructWidget<USizeBox>();ProgressHeight->SetHeightOverride(8);ProgressBox->AddChild(ProgressHeight);
    Progress=WidgetTree->ConstructWidget<UProgressBar>();Progress->SetFillColorAndOpacity(MapUI::Cyan);ProgressHeight->SetContent(Progress);Space(ProgressBox,14);
    Space(Content,10);Result=Text(Content,FText::GetEmpty(),15,MapUI::SecondaryText);Space(Content,10);
    for(const auto& P:TArray<TPair<FString,FString>>{{TEXT("recover"),TEXT("FleetTerrain.Recover")},{TEXT("next"),TEXT("FleetUX.Next")},{TEXT("again"),TEXT("FleetUX.Again")},{TEXT("confirm"),TEXT("Fleet.Confirm")},{TEXT("pause"),TEXT("Execution.Pause")},{TEXT("resume"),TEXT("Execution.Resume")}}){
        auto* B=MakeButton(Content,*P.Key,*P.Value,true);CastChecked<UVerticalBoxSlot>(B->Slot)->SetPadding(FMargin(0,0,0,6));
    }
    auto* Secondary=WidgetTree->ConstructWidget<UHorizontalBox>();Content->AddChild(Secondary);
    for(const auto& P:TArray<TPair<FString,FString>>{{TEXT("fit_preview"),TEXT("FleetUX.Fit")},{TEXT("reselect"),TEXT("FleetUX.Reselect")},{TEXT("abort"),TEXT("Execution.Abort")},{TEXT("cancel"),TEXT("FleetUX.Exit")}}){
        auto* B=MakeButton(Secondary,*P.Key,*P.Value,false,P.Key==TEXT("abort"));auto* LayoutSlot=CastChecked<UHorizontalBoxSlot>(B->Slot);LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));LayoutSlot->SetPadding(FMargin(2,4));
        auto ButtonStyle=B->GetStyle();ButtonStyle.SetNormalPadding(FMargin(8,8)).SetPressedPadding(FMargin(8,8));B->SetStyle(ButtonStyle);
        auto Font=CastChecked<UTextBlock>(B->GetContent())->GetFont();Font.Size=10;CastChecked<UTextBlock>(B->GetContent())->SetFont(Font);
    }
    AbortPrompt=WidgetTree->ConstructWidget<UVerticalBox>();Content->AddChild(AbortPrompt);Space(AbortPrompt,8);Text(AbortPrompt,T(TEXT("FleetUX.AbortHint")),15,MapUI::Warning);Space(AbortPrompt,8);
    auto* AbortRow=WidgetTree->ConstructWidget<UHorizontalBox>();AbortPrompt->AddChild(AbortRow);
    for(const auto& P:TArray<TPair<FString,FString>>{{TEXT("abort_cancel"),TEXT("FleetUX.Keep")},{TEXT("abort_confirm"),TEXT("FleetUX.AbortConfirm")}}){auto* B=MakeButton(AbortRow,*P.Key,*P.Value,P.Key==TEXT("abort_confirm"),P.Key==TEXT("abort_confirm"));auto* LayoutSlot=CastChecked<UHorizontalBoxSlot>(B->Slot);LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));LayoutSlot->SetPadding(FMargin(2));}
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);Refresh();
}
bool UMapFleetInteractionWidget::IsAvailable() const {
    auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());
    auto* Manager=PC?PC->GetCommandScreenManager():nullptr;
    const auto* Move=Manager && Manager->GetMapShell()?Manager->GetMapShell()->GetMovePanel():nullptr;
    const auto* Map=Manager?Manager->GetMapService():nullptr;
    return Map && !PlanConfiguration::Active(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans())
        && !Map->IsPlanning() && !Map->IsRouteEditCameraLocked() && !(Move && Move->IsMoving());
}
bool UMapFleetInteractionWidget::IsOverControls(const FVector2D& Screen) const {
    return Panel && Panel->IsVisible() && Panel->GetCachedGeometry().IsUnderLocation(Screen);
}
void UMapFleetInteractionWidget::SetMode(EMapFleetMode Value) {
    if(bPending || (Value!=EMapFleetMode::Browse && !IsAvailable()))return;
    CancelGesture();ClearTarget();Mode=Value;HeightPrimary=0;MessageKey.Empty();ErrorDetails=FText::GetEmpty();bAbortConfirm=false;bShowCoordinates=false;
    if(Value==EMapFleetMode::Browse)bPanelRequested=false;
    else if(Value==EMapFleetMode::GroupMove)bPanelRequested=true;
    if(auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());PC && PC->GetCommandScreenManager())PC->GetCommandScreenManager()->StopCameraFollow();
    Refresh();
}
bool UMapFleetInteractionWidget::CancelMode() {
    if(bPending || (!OwnsPrimaryInput() && !bHasTarget && !bPanelRequested))return false;
    SetMode(EMapFleetMode::Browse);return true;
}
bool UMapFleetInteractionWidget::CanShowExecutionControls() const {
    if(!GetGameInstance() || ManagedGroup.IsEmpty())return false;
    const auto Group=ExecutionControls::Find(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),ManagedGroup);
    TArray<FString> Members;for(int Id:OrderedSelection())Members.Add(FString::Printf(TEXT("UAV-%02d"),Id));Members.Sort();
    return Group.ActiveCount>0 && Group.Members==Members && Field(ExecutionControls::Representative(Group),TEXT("control_kind"))==TEXT("GROUP_MOVE");
}
bool UMapFleetInteractionWidget::ShowExecutionControls() {
    if(bPending || !CanShowExecutionControls())return false;
    // Configuration forbids new movement, but an explicit monitor entry must
    // still reach pause/abort for an existing acknowledged group.
    CancelGesture();ClearTarget();Mode=EMapFleetMode::Browse;HeightPrimary=0;
    MessageKey.Empty();ErrorDetails=FText::GetEmpty();bAbortConfirm=false;bShowCoordinates=false;
    bPanelRequested=true;Refresh();return true;
}
TArray<int32> UMapFleetInteractionWidget::OrderedSelection() const {
    return MapMove::Selection(GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>());
}
bool UMapFleetInteractionWidget::Project(int32 Id,FVector2D& Point) const {
    const auto* R=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    auto* Coordinates=R->GetCoordinateService().GetObject();FVector Geographic=FVector::ZeroVector;
    if(Coordinates && ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates) &&
        MapMove::Position(R,GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),Id,Geographic)){
        const FVector World=ICoordinateService::Execute_GeographicToWorld(Coordinates,Geographic.Y,Geographic.X,Geographic.Z);
        return UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),World,Point,true)
            && FleetSelection::Contains(FVector2D::ZeroVector,GetCachedGeometry().GetLocalSize(),Point);
    }
    FDroneTelemetrySnapshot Snapshot;R->GetTelemetry(Id,Snapshot);
    if(Field(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),TEXT("execution_adapter"))==TEXT("Mock") && Snapshot.TelemetrySource!=TEXT("received"))return false;
    const AActor* Display=R->GetReceiverActor(Id);
    if(!Display || Display->IsHidden())Display=R->GetSenderPawn(Id);
    if(!Display || Display->IsHidden())return false;
    return UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),Display->GetActorLocation(),Point,true)
        && FleetSelection::Contains(FVector2D::ZeroVector,GetCachedGeometry().GetLocalSize(),Point);
}
bool UMapFleetInteractionWidget::Telemetry(int32 Id,FVector& Geographic) const {
    return MapMove::Position(GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>(),GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),Id,Geographic,true);
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
        if(!TargetHeightInput || !FleetTargeting::ParseHeight(TargetHeightInput->GetText().ToString(),Parsed))return false;
        double Ground=0;if(bHeightAGL && (Parsed<1. || !SurfaceHeight(bHasTarget?TargetGeographic:Anchor,Ground)))return false;
        Height=Parsed+Ground;return FMath::Abs(Height-Anchor.Z)<=10000.;
    }
    Height=Anchor.Z;return true;
}
void UMapFleetInteractionWidget::ResetHeight() {
    const auto Members=OrderedSelection();FVector Anchor;
    if(Members.IsEmpty() || !Telemetry(Members[0],Anchor))return;
    double Ground=0;if(bHeightAGL && !SurfaceHeight(Anchor,Ground))return;
    HeightPrimary=Members[0];TGuardValue<bool> Guard(bUpdatingHeight,true);
    const double AGL=Anchor.Z-Ground;
    TargetHeightInput->SetText(User(FString::SanitizeFloat(bHeightAGL?(AGL>=1.?AGL:30.):Anchor.Z)));
}
bool UMapFleetInteractionWidget::SurfaceHeight(const FVector& Geographic,double& Height) const {
    return MapSurface::Height(GetWorld(),GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject(),Geographic,Height);
}
double UMapFleetInteractionWidget::InputHeight(double Absolute,const FVector& Geographic) const {
    double Ground=0;if(bHeightAGL && !SurfaceHeight(Geographic,Ground))return std::numeric_limits<double>::quiet_NaN();return Absolute-Ground;
}
bool UMapFleetInteractionWidget::RecoveryHeight(double& Height) const {
    if(!Is3DView())return false;const auto Members=OrderedSelection();FVector Anchor;
    if(Members.IsEmpty() || !Telemetry(Members[0],Anchor))return false;
    Height=Anchor.Z;bool Below=false;
    for(int Id:Members){FVector P;double Ground=0;if(!Telemetry(Id,P) || !SurfaceHeight(P,Ground))return false;
        Below|=P.Z<Ground+1.;Height=FMath::Max(Height,Anchor.Z+Ground+30.-P.Z);}
    return Below;
}
FText UMapFleetInteractionWidget::TerrainError(bool Force) const {
    if(!Is3DView() || !bHasTarget)return FText::GetEmpty();
    const double Now=FPlatformTime::Seconds();if(!Force && Now<NextTerrainCheck)return CachedTerrainError;
    auto Evaluate=[&]()->FText {FVector Anchor;
    if(TargetMembers.IsEmpty() || !Telemetry(TargetMembers[0],Anchor))return T(TEXT("Errors.COORDINATES_NOT_READY"));
    const bool Recovery=bRecoveryPreview && Field(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),TEXT("execution_adapter"))==TEXT("Mock")
        && FVector2D(TargetGeographic.X-Anchor.X,TargetGeographic.Y-Anchor.Y).Size()<1.e-8 && TargetGeographic.Z>Anchor.Z;
    for(int Id:TargetMembers){FVector From;if(!Telemetry(Id,From))return T(TEXT("Errors.COORDINATES_NOT_READY"));
        const FVector To(TargetGeographic.X+FMath::UnwindDegrees(From.X-Anchor.X),TargetGeographic.Y+From.Y-Anchor.Y,TargetGeographic.Z+From.Z-Anchor.Z);
        const double North=FMath::DegreesToRadians(To.Y-From.Y)*6371000.;
        const double East=FMath::DegreesToRadians(FMath::UnwindDegrees(To.X-From.X))*6371000.*FMath::Cos(FMath::DegreesToRadians((To.Y+From.Y)*.5));
        const int Count=FMath::Clamp(FMath::CeilToInt(FMath::Sqrt(North*North+East*East)/10.),1,1024);
        for(int I=0;I<=Count;++I){const FVector P=FMath::Lerp(From,To,double(I)/Count);double Ground=0;
            if(!SurfaceHeight(P,Ground))return T(TEXT("Errors.TERRAIN_REQUIRED"));
            if(P.Z<Ground+(Recovery?30.:1.)-.01 && !(Recovery && I<Count))return T(Recovery?TEXT("FleetTerrain.TooLow"):I==0?TEXT("FleetTerrain.SourceBelow"):TEXT("FleetTerrain.TooLow"));}
    }return FText::GetEmpty();};
    CachedTerrainError=Evaluate();NextTerrainCheck=Now+.5;return CachedTerrainError;
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
    const double Input=InputHeight(Anchor.Z+FMath::RoundToInt(FMath::Clamp(Value,-100.f,100.f)),bHasTarget?TargetGeographic:Anchor);
    if(!FMath::IsFinite(Input))return;
    {TGuardValue<bool> Guard(bUpdatingHeight,true);TargetHeightInput->SetText(User(FString::SanitizeFloat(Input)));}
    TargetHeightChanged(TargetHeightInput->GetText());
}
bool UMapFleetInteractionWidget::BeginPointer(const FVector2D& Screen,bool Shift,bool Control) {
    if(!IsAvailable() || bPending || bSelectionPending)return false;
    const auto* SelectionPC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());const auto* SelectionShell=SelectionPC && SelectionPC->GetCommandScreenManager()?SelectionPC->GetCommandScreenManager()->GetMapShell():nullptr;
    if(SelectionShell && SelectionShell->IsDeviceSelectionPending())return false;
    if(!GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->IsReady())return false;
    if(SelectionPC && SelectionPC->GetCommandScreenManager())SelectionPC->GetCommandScreenManager()->CancelStartupFraming();
    // Left gestures always select, including after a move ACK or while the
    // destination card is open. Selection never implicitly creates a target.
    CancelGesture();ClearTarget();Start=Current=GetCachedGeometry().AbsoluteToLocal(Screen);bDragging=true;bPointerValid=true;
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
    SetMode(EMapFleetMode::BoxSelect);
    SelectInRectangle(Click);
    Refresh();
}
bool UMapFleetInteractionWidget::PickDestinationAtScreen(const FVector2D& Screen) {
    if(!IsAvailable() || bPending || Screen.ContainsNaN())return false;
    // A rejected move must leave the accepted selection usable. In particular,
    // do not arm destination mode while waiting for a selection ACK.
    ErrorDetails=UnavailableReason();
    if(!ErrorDetails.IsEmpty()){ClearTarget();Refresh();return false;}
    if(Mode!=EMapFleetMode::GroupMove)SetMode(EMapFleetMode::GroupMove);
    const bool Picked=PickTarget(GetCachedGeometry().AbsoluteToLocal(Screen));Refresh();return Picked;
}
void UMapFleetInteractionWidget::CancelGesture() { bDragging=false;bPointerValid=false;InvalidateLayoutAndVolatility(); }
void UMapFleetInteractionWidget::SelectInRectangle(bool Click) {
    auto* R=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();TArray<int32> Hits;float Closest=24.f;int32 Nearest=0;
    for(const auto& D:R->GetFriendlyDroneDescriptors()) {FVector2D P;if(!Project(D.DroneId,P))continue;
        if(!Click && FleetSelection::Contains(Start,Current,P))Hits.Add(D.DroneId);
        if(Click && FVector2D::Distance(Current,P)<Closest){Closest=FVector2D::Distance(Current,P);Nearest=D.DroneId;}
    }
    // At close range the native wings can extend beyond the icon's minimum
    // hit radius. Accept the actually displayed model's collision hit too.
    if(Click && GetOwningPlayer() && !DisplayedModelActors.IsEmpty()){
        FVector2D Pixel,Viewport;
        USlateBlueprintLibrary::AbsoluteToViewport(this,GetCachedGeometry().LocalToAbsolute(Current),Pixel,Viewport);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(FleetModelSelection),false);
        for(const auto& D:R->GetFriendlyDroneDescriptors()){
            const auto* Display=GetDisplayedModelActor(D.DroneId);
            for(AActor* Actor:{R->GetReceiverActor(D.DroneId),static_cast<AActor*>(R->GetSenderPawn(D.DroneId))})
                if(Actor && Actor!=Display)Query.AddIgnoredActor(Actor);
        }
        FHitResult Hit;
        if(GetOwningPlayer()->GetHitResultAtScreenPosition(Pixel,ECC_Visibility,Query,Hit))
            if(const auto* Mesh=Cast<UMeshComponent>(Hit.GetComponent());Mesh && Mesh->IsVisible() && !Mesh->bHiddenInGame)
                for(const auto& Item:DisplayedModelActors)if(Item.Value.Get()==Hit.GetActor()){Nearest=Item.Key;break;}
    }
    if(Click && Nearest>0)Hits.Add(Nearest);
    const auto Selected=FleetSelection::Apply(R->GetMultiSelectedDrones(),Hits,SelectionOperation);
    const int32 Primary=Selected.Contains(R->GetPrimarySelectedDrone())?R->GetPrimarySelectedDrone():Selected.IsEmpty()?0:Selected[0];
    ClearTarget();MessageKey=TEXT("Fleet.SelectionPending");
    RequestedSelection=Selected;RequestedPrimary=Primary;
    SelectionRequestedAt=FPlatformTime::Seconds();
    bSelectionPending=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->SetLocalSelection(Primary,Selected);
    if(!bSelectionPending)MessageKey=TEXT("Errors.SYNC_OFFLINE");
}
void UMapFleetInteractionWidget::ClearTarget() { bHasTarget=false;bRecoveryPreview=false;NextTerrainCheck=0;TargetMembers.Empty();TargetPrimary=0;PreviewRoutes.Reset(); }
bool UMapFleetInteractionWidget::PickTarget(const FVector2D& Local) {
    ClearTarget();ErrorDetails=UnavailableReason();if(!ErrorDetails.IsEmpty())return false;
    const auto Members=OrderedSelection();FVector Anchor;if(Members.IsEmpty() || !Telemetry(Members[0],Anchor))return false;
    double Height=0;if(!TargetHeight(Height)){MessageKey=TEXT("Fleet3D.InvalidHeight");return false;}
    auto* Coordinates=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetCoordinateService().GetObject();
    if(!Coordinates || !ICoordinateService::Execute_IsCoordinateSystemReady(Coordinates)){MessageKey=TEXT("Errors.COORDINATES_NOT_READY");return false;}
    const FVector AnchorWorld=ICoordinateService::Execute_GeographicToWorld(Coordinates,Anchor.Y,Anchor.X,Anchor.Z);
    const FVector2D Pixel=Local*UWidgetLayoutLibrary::GetViewportScale(this);FVector Origin,Direction;
    FVector Point;if(!GetOwningPlayer()->DeprojectScreenPositionToWorld(Pixel.X,Pixel.Y,Origin,Direction)
        || !(Is3DView()?MapSurface::Raycast(GetWorld(),Origin,Origin+Direction*1000000.,Point):FleetTargeting::IntersectFlightPlane(Origin,Direction,AnchorWorld,Point))){MessageKey=Is3DView()?TEXT("Errors.TERRAIN_REQUIRED"):TEXT("Map.InvalidPosition");return false;}
    const FVector Geo=ICoordinateService::Execute_WorldToGeographic(Coordinates,Point);
    if(Geo.ContainsNaN()){MessageKey=TEXT("Map.InvalidPosition");return false;}
    TargetGeographic=FVector(Geo.X,Geo.Y,Height);TargetMembers=Members;TargetPrimary=Members[0];bHasTarget=true;
    if(!TargetHeight(Height)){ClearTarget();MessageKey=TEXT("Errors.TERRAIN_REQUIRED");return false;}TargetGeographic.Z=Height;
    MessageKey=TEXT("FleetUX.PreviewOnly");BuildPreview();return true;
}
void UMapFleetInteractionWidget::BuildPreview() {
    NextTerrainCheck=0;
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
    const auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());const auto* Shell=PC && PC->GetCommandScreenManager()?PC->GetCommandScreenManager()->GetMapShell():nullptr;
    if(Shell && Shell->IsDeviceSelectionPending())return T(TEXT("Fleet.SelectionPending"));
    return MapMove::Unavailable(GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>(),GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>(),OrderedSelection());
}
void UMapFleetInteractionWidget::Refresh() {
    if(!Summary)return;
    const bool View3D=Is3DView();
    if(bHadMapMode && View3D!=bWas3D){CancelGesture();ClearTarget();HeightPrimary=0;MessageKey=Mode!=EMapFleetMode::Browse?TEXT("Fleet3D.ViewChanged"):TEXT("");ErrorDetails=FText::GetEmpty();}
    bHadMapMode=true;bWas3D=View3D;
    if(!IsAvailable() && Mode!=EMapFleetMode::Browse && !bPending){CancelGesture();ClearTarget();Mode=EMapFleetMode::Browse;bAbortConfirm=false;}
    const auto Selected=OrderedSelection();
    if(Mode==EMapFleetMode::GroupMove && Selected.IsEmpty() && !bSelectionPending){CancelGesture();ClearTarget();Mode=EMapFleetMode::BoxSelect;HeightPrimary=0;}
    if(Mode==EMapFleetMode::GroupMove && View3D && !Selected.IsEmpty() && HeightPrimary!=Selected[0])ResetHeight();
    auto Accepted=Selected;Accepted.Sort();
    if(bSelectionPending && Accepted==RequestedSelection && GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()->GetPrimarySelectedDrone()==RequestedPrimary) {
        bSelectionPending=false;if(MessageKey==TEXT("Fleet.SelectionPending"))MessageKey=TEXT("Fleet.SelectionReady");
    }
    if(bSelectionPending && (!GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->IsReady() || FPlatformTime::Seconds()-SelectionRequestedAt>10.)){
        bSelectionPending=false;MessageKey=TEXT("Errors.SYNC_FAILED");
    }
    if(bHasTarget && (Selected!=TargetMembers || Selected.IsEmpty() || Selected[0]!=TargetPrimary)){ClearTarget();MessageKey=TEXT("Fleet.SelectionChanged");}
    auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();const auto All=Object(S->GetPlans(),TEXT("executions"));
    TArray<FString> SelectedIds;for(int Id:Selected)SelectedIds.Add(FString::Printf(TEXT("UAV-%02d"),Id));SelectedIds.Sort();
    ExecutionControls::FGroup Group;
    for(const auto& Candidate:ExecutionControls::Groups(S->GetPlans(),false))if(Candidate.Members==SelectedIds
        && Field(ExecutionControls::Representative(Candidate),TEXT("control_kind"))==TEXT("GROUP_MOVE")){Group=Candidate;break;}
    const auto Latest=ExecutionControls::Representative(Group);ExecutionId=Field(Latest,TEXT("execution_id"));ManagedGroup=Group.Id;const FString State=Group.State;
    const bool Active=Group.ActiveCount>0;if(!Active || (bAbortConfirm && ManagedGroup!=AbortGroup))bAbortConfirm=false;
    PanelTitle->SetText(T(Active && Mode==EMapFleetMode::Browse?TEXT("FleetUX.ExecutionControls"):TEXT("FleetUX.Title")));
    double RecoveryAltitude=0;const bool BelowSurface=!Active && RecoveryHeight(RecoveryAltitude);
    const bool Mock=Field(S->GetPlans(),TEXT("execution_adapter"))==TEXT("Mock");
    const bool Configuring=PlanConfiguration::Active(S->GetPlans());
    // Selection, live telemetry and execution snapshots update the card's
    // contents; they never open a card that the operator has not requested.
    const bool Show=bPanelRequested && (!Configuring || Active);
    Panel->SetVisibility(Show?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    const int Step=Mode==EMapFleetMode::BoxSelect || Selected.IsEmpty()?0:bHasTarget?2:1;
    Steps[0]->GetParent()->SetVisibility(Active || Mode==EMapFleetMode::Browse?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
    for(int I=0;I<Steps.Num();++I){MapUI::Panel(Steps[I],I==Step?MapUI::Selected:MapUI::Background,I<=Step?MapUI::Cyan:MapUI::Border);
        StepLabels[I]->SetText(T(*FString::Printf(TEXT("FleetUX.Step%d"),I+1)));StepLabels[I]->SetColorAndOpacity(I<=Step?MapUI::Cyan:MapUI::SecondaryText);}
    Summary->SetText(T(Active?TEXT("FleetUX.RunningHint"):BelowSurface?TEXT("FleetTerrain.SourceBelow"):Mode==EMapFleetMode::BoxSelect?(!Selected.IsEmpty()?TEXT("FleetUX.SelectionReady"):TEXT("FleetUX.SelectHint")):
        Mode==EMapFleetMode::GroupMove?(bHasTarget?TEXT("FleetUX.ReviewHint"):TEXT("FleetUX.PickHint")):TEXT("FleetUX.FinishedHint")));
    SelectionText->SetText(Selected.IsEmpty()?T(TEXT("FleetUX.EmptySelection")):FText::Format(T(TEXT("FleetUX.Selected")),FText::AsNumber(Selected.Num()),User(FString::Printf(TEXT("UAV-%02d"),Selected[0]))));
    SelectionText->SetVisibility(Mode!=EMapFleetMode::Browse || Active?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    TArray<FString> Ids;for(int Id:Selected)Ids.Add(FString::Printf(TEXT("UAV-%02d"),Id));SelectionText->SetToolTipText(User(FString::Join(Ids,TEXT(", "))));
    const FText Unavailable=UnavailableReason();if(bHasTarget && !Unavailable.IsEmpty())ClearTarget();
    double Height=0;const bool HeightValid=Active || !View3D || Mode!=EMapFleetMode::GroupMove || Selected.IsEmpty() || TargetHeight(Height);
    if(bHasTarget && View3D && HeightValid && FMath::Abs(TargetGeographic.Z-Height)>.01){TargetGeographic.Z=Height;BuildPreview();}
    const FText SurfaceError=TerrainError();
    HeightControls->SetVisibility(Mode==EMapFleetMode::GroupMove && View3D && !Selected.IsEmpty() && !Active?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    FVector Anchor=FVector::ZeroVector;const bool HasAnchor=!Selected.IsEmpty() && Telemetry(Selected[0],Anchor);
    TargetHeightInput->SetIsEnabled(!bPending && !Active && HasAnchor);HeightSlider->SetIsEnabled(!bPending && !Active && !Selected.IsEmpty() && HasAnchor);
    HeightLabel->SetText(T(bHeightAGL?TEXT("FleetTerrain.AGL"):TEXT("Fleet3D.Height")));
    CastChecked<UTextBlock>(Buttons[TEXT("height_mode")]->GetContent())->SetText(T(bHeightAGL?TEXT("FleetTerrain.UseAbsolute"):TEXT("FleetTerrain.UseAGL")));
    Buttons[TEXT("height_mode")]->SetIsEnabled(!bPending && !Active);
    double SourceGround=0;const bool GroundReady=HasAnchor && SurfaceHeight(Anchor,SourceGround);
    TerrainInfo->SetText(GroundReady?FText::Format(T(TEXT("FleetTerrain.Datum")),FText::AsNumber(SourceGround),FText::AsNumber(Anchor.Z-SourceGround),FText::AsNumber(Height)):T(TEXT("Errors.TERRAIN_REQUIRED")));
    TerrainInfo->SetColorAndOpacity(BelowSurface?MapUI::Warning:MapUI::SecondaryText);
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
        const auto Color=State==TEXT("COMPLETED")?MapUI::Online:State==TEXT("PAUSED")?MapUI::Warning:State==TEXT("FAILED")?MapUI::Critical:MapUI::Cyan;
        Progress->SetFillColorAndOpacity(Color);StateText->SetColorAndOpacity(Color);StateText->SetText(FText::Format(T(TEXT("FleetUX.State")),T(*(TEXT("Execution.")+State)),FText::AsPercent(P)));}
    auto ShowButton=[&](const TCHAR* Name,bool Visible,bool Enabled=true){Buttons[Name]->SetVisibility(Visible?ESlateVisibility::Visible:ESlateVisibility::Collapsed);Buttons[Name]->SetIsEnabled(Enabled && !bPending);};
    ShowButton(TEXT("next"),Mode==EMapFleetMode::BoxSelect,!Selected.IsEmpty() && Unavailable.IsEmpty());
    ShowButton(TEXT("again"),Mode==EMapFleetMode::Browse && Latest && !Active,Unavailable.IsEmpty() && IsAvailable());
    ShowButton(TEXT("recover"),BelowSurface && Mock && (!bRecoveryPreview || !SurfaceError.IsEmpty()),Unavailable.IsEmpty() && IsAvailable());
    ShowButton(TEXT("confirm"),Mode==EMapFleetMode::GroupMove && !Active,bHasTarget && HeightValid && SurfaceError.IsEmpty() && Unavailable.IsEmpty() && IsAvailable());
    ShowButton(TEXT("pause"),Group.CanPause && !bAbortConfirm,S->IsReady() && S->IsHydrated());ShowButton(TEXT("resume"),Group.CanResume && !bAbortConfirm,S->IsReady() && S->IsHydrated());
    ShowButton(TEXT("abort"),Group.CanAbort && !bAbortConfirm,S->IsReady() && S->IsHydrated());ShowButton(TEXT("reselect"),Mode==EMapFleetMode::GroupMove && bHasTarget && !Active);
    ShowButton(TEXT("fit_preview"),Mode==EMapFleetMode::GroupMove && bHasTarget && !Active,IsAvailable());
    ShowButton(TEXT("cancel"),!bAbortConfirm);AbortPrompt->SetVisibility(bAbortConfirm?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    ShowButton(TEXT("abort_confirm"),bAbortConfirm,S->IsReady() && S->IsHydrated());ShowButton(TEXT("abort_cancel"),bAbortConfirm);
    for(auto Name:{TEXT("next"),TEXT("again"),TEXT("confirm"),TEXT("pause"),TEXT("resume"),TEXT("abort_confirm")})
        CastChecked<UTextBlock>(Buttons[Name]->GetContent())->SetColorAndOpacity(Buttons[Name]->GetIsEnabled()?MapUI::Background:MapUI::SecondaryText);
    for(auto Name:{TEXT("height_up"),TEXT("height_down"),TEXT("height_current")})Buttons[Name]->SetIsEnabled(!bPending && !Active && !Selected.IsEmpty() && HasAnchor);
    auto Caption=[&](const TCHAR* Name,const FText& Value){CastChecked<UTextBlock>(Buttons[Name]->GetContent())->SetText(Value);};
    Caption(TEXT("confirm"),bHasTarget?FText::Format(T(TEXT("FleetUX.Confirm")),FText::AsNumber(Selected.Num())):T(TEXT("FleetUX.NeedTarget")));
    Caption(TEXT("cancel"),T(Mode==EMapFleetMode::Browse?TEXT("FleetUX.Hide"):TEXT("FleetUX.Exit")));
    Caption(TEXT("details"),T(bShowCoordinates?TEXT("FleetUX.HideCoordinates"):TEXT("FleetUX.Coordinates")));
    const bool Blocked=!HeightValid || !SurfaceError.IsEmpty() || !ErrorDetails.IsEmpty() || (!Unavailable.IsEmpty() && (Mode!=EMapFleetMode::Browse || Latest) && !Active);
    Result->SetColorAndOpacity(Blocked?MapUI::Warning:MapUI::SecondaryText);
    Result->SetText(bPending?T(TEXT("Common.Waiting")):!ErrorDetails.IsEmpty()?ErrorDetails:
        (Mode!=EMapFleetMode::Browse || Latest) && !Active && !Unavailable.IsEmpty()?Unavailable:!HeightValid?T(View3D && bHeightAGL && !GroundReady?TEXT("Errors.TERRAIN_REQUIRED"):TEXT("FleetTerrain.InvalidHeight")):!SurfaceError.IsEmpty()?SurfaceError:MessageKey.IsEmpty()?FText::GetEmpty():T(*MessageKey));
    InvalidateLayoutAndVolatility();
}
void UMapFleetInteractionWidget::Action(FName Name,int32) {
    if(bPending)return;
    if(Name==TEXT("height_mode")){
        double Absolute=0;const bool Valid=TargetHeight(Absolute);bHeightAGL=!bHeightAGL;
        if(Valid){FVector Anchor;const auto Members=OrderedSelection();if(!Members.IsEmpty() && Telemetry(Members[0],Anchor)){
            const double Value=InputHeight(Absolute,bHasTarget?TargetGeographic:Anchor);
            if(FMath::IsFinite(Value)){TGuardValue<bool> Guard(bUpdatingHeight,true);TargetHeightInput->SetText(User(FString::SanitizeFloat(Value)));}}}
        else ResetHeight();TargetHeightChanged(TargetHeightInput->GetText());return;
    }
    if(Name==TEXT("recover")){
        double Safe=0;if(!RecoveryHeight(Safe) || !UnavailableReason().IsEmpty() || !IsAvailable()
            || Field(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),TEXT("execution_adapter"))!=TEXT("Mock"))return;
        SetMode(EMapFleetMode::GroupMove);TargetMembers=OrderedSelection();TargetPrimary=TargetMembers[0];
        FVector Anchor;Telemetry(TargetPrimary,Anchor);TargetGeographic=Anchor;TargetGeographic.Z=Safe;bHasTarget=true;bRecoveryPreview=true;
        {TGuardValue<bool> Guard(bUpdatingHeight,true);TargetHeightInput->SetText(User(FString::SanitizeFloat(InputHeight(Safe,Anchor))));}
        MessageKey=TEXT("FleetTerrain.RecoveryPreview");BuildPreview();Refresh();Action(TEXT("fit_preview"),0);return;
    }
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
        double Height=0;if(!FleetTargeting::ParseHeight(TargetHeightInput->GetText().ToString(),Height)){ResetHeight();if(!FleetTargeting::ParseHeight(TargetHeightInput->GetText().ToString(),Height))return;}
        {
            {TGuardValue<bool> Guard(bUpdatingHeight,true);TargetHeightInput->SetText(User(FString::SanitizeFloat(Height+(Name==TEXT("height_up")?10.:-10.))));}
            TargetHeightChanged(TargetHeightInput->GetText());
        }return;
    }
    if(Name==TEXT("cancel")){SetMode(EMapFleetMode::Browse);return;}
    auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("request_id"),FGuid::NewGuid().ToString());
    if(Name==TEXT("confirm")) {
        ErrorDetails=UnavailableReason();if(!bHasTarget || !ErrorDetails.IsEmpty() || !IsAvailable()){Refresh();return;}
        double Height=0;if(!TargetHeight(Height)){MessageKey=TEXT("Fleet3D.InvalidHeight");Refresh();return;}
        TargetGeographic.Z=Height;
        ErrorDetails=TerrainError(true);if(!ErrorDetails.IsEmpty()){Refresh();return;}
        if(OrderedSelection()!=TargetMembers){ClearTarget();MessageKey=TEXT("Fleet.SelectionChanged");Refresh();return;}
        R=MapMove::Request(TargetMembers,TargetGeographic);
    } else {
        if(!S->IsReady() || !S->IsHydrated()){MessageKey=TEXT("Errors.SYNC_OFFLINE");Refresh();return;}
        const auto Group=ExecutionControls::Find(S->GetPlans(),ManagedGroup);TArray<FString> SelectedIds;
        for(int Id:OrderedSelection())SelectedIds.Add(FString::Printf(TEXT("UAV-%02d"),Id));SelectedIds.Sort();
        const auto E=ExecutionControls::Representative(Group,Name);if(!E || Group.Members!=SelectedIds){MessageKey=TEXT("Errors.EXECUTION_STATE_INVALID");Refresh();return;}
        R->SetStringField(TEXT("action"),TEXT("execution_")+Name.ToString());R->SetStringField(TEXT("execution_id"),Field(E,TEXT("execution_id")));R->SetNumberField(TEXT("control_version"),E->GetNumberField(TEXT("control_version")));
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
    // Reserve the map footer at y=748; the preview's action row must remain
    // fully visible and clickable above the persistent pointer hint.
    if(Panel){const float Available=508.f;
        const bool ExecutionDetails=ProgressBox && ProgressBox->GetVisibility()==ESlateVisibility::Visible;
        const float Desired=Mode==EMapFleetMode::BoxSelect?460.f:Mode==EMapFleetMode::GroupMove?(bHasTarget?900.f:790.f):bAbortConfirm || ExecutionDetails?510.f:Buttons[TEXT("recover")]->GetVisibility()==ESlateVisibility::Visible?510.f:400.f;
        const FVector2D Size(372,FMath::Min(Available,Desired));
        auto* LayoutSlot=CastChecked<UCanvasPanelSlot>(Panel->Slot);if(!LayoutSlot->GetSize().Equals(Size,.5))LayoutSlot->SetSize(Size);}
    // Far/2D symbols have a fixed screen size; close 3D views reveal the native
    // model without changing its transform, collision or selected model type.
    const auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    FVector CameraLocation=FVector::ZeroVector;FRotator CameraRotation;
    if(GetOwningPlayer())GetOwningPlayer()->GetPlayerViewPoint(CameraLocation,CameraRotation);
    const auto PreviousModels=MoveTemp(DisplayedModelActors);DisplayedModelActors.Reset();ModelScreenBounds.Reset();
    ModelVisibility.BeginFrame();
    for(const auto& D:Registry->GetFriendlyDroneDescriptors()){
        if(auto* Receiver=Cast<ARealTimeDroneReceiver>(Registry->GetReceiverActor(D.DroneId));Receiver && Receiver->NameLabelWidgetComponent){
            auto* Label=Receiver->NameLabelWidgetComponent.Get();if(!SuppressedLabels.Contains(Label))SuppressedLabels.Add(Label,Label->IsVisible());Label->SetVisibility(false);}
        AActor* Receiver=Registry->GetReceiverActor(D.DroneId);
        AActor* Sender=Registry->GetSenderPawn(D.DroneId);
        AActor* Display=Receiver;FBox Bounds(ForceInit);
        bool Ready=ModelVisibility.ActiveBounds(Display,Bounds);
        if(!Ready){Display=Sender;Ready=ModelVisibility.ActiveBounds(Display,Bounds);}
        FBox2D ScreenBounds(ForceInit);FVector2D Anchor;
        Ready=Ready && Project(D.DroneId,Anchor) && MapDronePresentation::ProjectBounds(GetOwningPlayer(),Bounds,ScreenBounds);
        const double Diameter=Ready?ScreenBounds.GetSize().GetMax():0.;
        const double Distance=Ready?FVector::Distance(CameraLocation,Display->GetActorLocation()):MAX_dbl;
        const bool ShowModel=MapDronePresentation::ShouldShowModel(PreviousModels.Contains(D.DroneId),Is3DView(),Ready,Diameter,Distance);
        if(ShowModel){DisplayedModelActors.Add(D.DroneId,Display);ModelScreenBounds.Add(D.DroneId,ScreenBounds);}
        // Suppress the sender mirror even when a valid receiver model is shown.
        ModelVisibility.SetSuppressed(Receiver,!ShowModel || Receiver!=Display);
        if(Sender!=Receiver)ModelVisibility.SetSuppressed(Sender,!ShowModel || Sender!=Display);
    }
    ModelVisibility.EndFrame();
}
AActor* UMapFleetInteractionWidget::GetDisplayedModelActor(int32 Id) const {
    const auto* Actor=DisplayedModelActors.Find(Id);return Actor && Actor->IsValid() && !Actor->Get()->IsHidden()?Actor->Get():nullptr;
}
int32 UMapFleetInteractionWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const {
    const int32 Base=Super::NativePaint(Args,G,Clip,Out,Layer,Style,Enabled);auto* R=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    const FGeometry Desktop=MapPaint::DesktopGeometry(G,Args);PaintedDroneIds.Reset();
    static const FSlateRoundedBoxBrush CardBrush(MapUI::Surface,7.f,MapUI::Border,1.f);
    static const FSlateRoundedBoxBrush TargetRingBrush(FLinearColor::Transparent,14.f,MapUI::Cyan,2.f);
    auto Rect=[&](const FVector2D& A,const FVector2D& B,const FLinearColor& Color){const FVector2D Min(FMath::Min(A.X,B.X),FMath::Min(A.Y,B.Y)),Max(FMath::Max(A.X,B.X),FMath::Max(A.Y,B.Y));
        FSlateDrawElement::MakeLines(Out,Base+1,G.ToPaintGeometry(),{Min,FVector2D(Max.X,Min.Y),Max,FVector2D(Min.X,Max.Y),Min},ESlateDrawEffect::None,Color,true,1.5f);};
    const auto* PC=Cast<ADroneOpsPlayerController>(GetOwningPlayer());
    const auto* Shell=PC && PC->GetCommandScreenManager()?PC->GetCommandScreenManager()->GetMapShell():nullptr;
    if(Shell)Out.PushClip(FSlateClippingZone(MapPaint::ClipGeometry(G,Desktop,Shell->GetMapViewportGeometry())));
    auto ClearPoint=[&](const FVector2D& Local){return Shell?Shell->IsPointOverMap(Desktop.LocalToAbsolute(Local)):!IsOverControls(Desktop.LocalToAbsolute(Local));};
    TArray<FBox2D> ControlBounds;if(Shell)for(const auto& B:Shell->GetMapControlBounds())ControlBounds.Add(FBox2D(Desktop.AbsoluteToLocal(B.Min),Desktop.AbsoluteToLocal(B.Max)));
    auto ClearLabel=[&](const FVector2D& Local,const FVector2D& Size){return MapLabels::ClearOfRects(Local,Size,ControlBounds) && ClearPoint(Local) && ClearPoint(Local+Size) && ClearPoint(Local+FVector2D(Size.X,0)) && ClearPoint(Local+FVector2D(0,Size.Y));};
    {
        TArray<TPair<int32,FVector2D>> Points;for(const auto& D:R->GetFriendlyDroneDescriptors()){FVector2D P;if(Project(D.DroneId,P) && ClearPoint(P))Points.Emplace(D.DroneId,P);}
        Points.Sort([](const auto& A,const auto& B){return FMath::IsNearlyEqual(A.Value.Y,B.Value.Y,1.)?A.Key<B.Key:A.Value.Y<B.Value.Y;});
        TArray<FBox2D> Placed=ControlBounds;for(const auto& Point:Points){
            const auto* Bounds=ModelScreenBounds.Find(Point.Key);
            Placed.Add(Bounds && IsModelPresentation(Point.Key)?*Bounds:FBox2D(Point.Value-FVector2D(20),Point.Value+FVector2D(20)));}
        FBox2D LabelBounds(FVector2D::ZeroVector,G.GetLocalSize());if(Shell){const auto MG=Shell->GetMapViewportGeometry();LabelBounds=FBox2D(Desktop.AbsoluteToLocal(MG.GetAbsolutePosition()),Desktop.AbsoluteToLocal(MG.LocalToAbsolute(MG.GetLocalSize())));}
        const auto Plans=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans();

        for(const auto& Item:Points){const int Id=Item.Key;const FVector2D P=Item.Value;const bool Primary=Id==R->GetPrimarySelectedDrone(),Selected=R->GetMultiSelectedDrones().Contains(Id);
            const auto Color=(Primary || Selected)?MapUI::Cyan:MapUI::SecondaryText;
            if(!IsModelPresentation(Id)){
                PaintedDroneIds.Add(Id);
                const float Radius=MapDronePresentation::IconDiameter*.5;
                const FSlateRoundedBoxBrush MarkerBrush(MapUI::Surface,Radius,Color,2.f);
                FSlateDrawElement::MakeBox(Out,Base+2,G.ToPaintGeometry(FVector2D(Radius*2),FSlateLayoutTransform(P-FVector2D(Radius))),&MarkerBrush,ESlateDrawEffect::None,MapUI::Surface);
                if(MarkerIcon){const auto& Brush=MarkerIcon->GetBrush();const FVector2D IconSize=Brush.ImageSize;
                    FSlateDrawElement::MakeBox(Out,Base+3,G.ToPaintGeometry(IconSize,FSlateLayoutTransform(P-IconSize*.5)),&Brush,ESlateDrawEffect::None,FLinearColor::White);}
            }
            if((Primary || Selected) && SelectionMarkerIcon)FSlateDrawElement::MakeBox(Out,Base+2,G.ToPaintGeometry(FVector2D(36),FSlateLayoutTransform(P-FVector2D(18))),&SelectionMarkerIcon->GetBrush(),ESlateDrawEffect::None,FLinearColor::White);
            FDroneLabelSettings Settings;R->GetDroneLabelSettings(Id,Settings);FDroneDescriptor Descriptor;R->GetDroneDescriptor(Id,Descriptor);
            FString DisplayName=Settings.DisplayName.IsEmpty()?Descriptor.Name:Settings.DisplayName;
            const auto NameFont=FCoreStyle::GetDefaultFontStyle("Bold",FMath::Clamp(Settings.FontSize,8,72));
            const auto CaptionFont=FCoreStyle::GetDefaultFontStyle("Regular",12);
            const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
            const double MaxLabelWidth=FMath::Min(460.,LabelBounds.GetSize().X-24.);
            while(DisplayName.Len()>2 && Measure->Measure(DisplayName,NameFont).X>MaxLabelWidth-30.)DisplayName.LeftChopInline(1);
            if(DisplayName!=(Settings.DisplayName.IsEmpty()?Descriptor.Name:Settings.DisplayName))DisplayName+=TEXT("…");
            FVector Geo;double Ground=0;const bool HasGeo=Telemetry(Id,Geo),AGL=HasGeo && Is3DView() && SurfaceHeight(Geo,Ground);
            FNumberFormattingOptions MarkerFormat;MarkerFormat.SetMaximumFractionalDigits(1);
            FText Caption=HasGeo?FText::Format(T(AGL?TEXT("FleetTerrain.MarkerAGL"):TEXT("FleetUX.Marker")),User(FString::Printf(TEXT("%02d"),Id)),AGL?FText::AsNumber(Geo.Z-Ground,&MarkerFormat):FText::AsNumber(FMath::RoundToInt(Geo.Z))):User(FString::Printf(TEXT("UAV-%02d"),Id));
            FDroneTelemetrySnapshot Snapshot;if(R->GetTelemetry(Id,Snapshot) && Snapshot.bSimulation)Caption=FText::Format(T(TEXT("MapMove.MockMarker")),Caption);
            FLinearColor CaptionColor=Color;
            if(const auto E=ExecutionUI::PositionExecution(Plans,FString::Printf(TEXT("UAV-%02d"),Id))){const FString State=Field(E,TEXT("state"));Caption=FText::Format(User(TEXT("{0} · {1}")),Caption,T(*(TEXT("Execution.")+State)));CaptionColor=State==TEXT("FAILED")?MapUI::Critical:State==TEXT("PAUSED")?MapUI::Warning:ExecutionUI::Active(E)?MapUI::Online:MapUI::SecondaryText;}
            const auto NameSize=Measure->Measure(DisplayName,NameFont),CaptionSize=Measure->Measure(Caption,CaptionFont);
            const FVector2D LabelSize(FMath::Max3(130.,double(NameSize.X)+18.,double(CaptionSize.X)+18.),NameSize.Y+CaptionSize.Y+12.);FVector2D Label=FVector2D::ZeroVector;
            if(!MapLabels::Place(P,LabelSize,LabelBounds,Placed,ClearLabel,Label))continue;
            Placed.Add(FBox2D(Label,Label+LabelSize));
            const FVector2D Connection(FMath::Clamp(P.X,Label.X,Label.X+LabelSize.X),FMath::Clamp(P.Y,Label.Y,Label.Y+LabelSize.Y));
            FSlateDrawElement::MakeLines(Out,Base+1,G.ToPaintGeometry(),TArray<FVector2D>{P,Connection},ESlateDrawEffect::None,Color.CopyWithNewOpacity(.7f),true,1.f);
            const FSlateRoundedBoxBrush LabelBrush(MapUI::Surface,6.f,(Primary || Selected)?MapUI::Cyan:MapUI::Border,1.f);
            FSlateDrawElement::MakeBox(Out,Base+2,G.ToPaintGeometry(LabelSize,FSlateLayoutTransform(Label)),&LabelBrush,ESlateDrawEffect::None,MapUI::Surface);
            FSlateDrawElement::MakeText(Out,Base+3,G.ToPaintGeometry(LabelSize,FSlateLayoutTransform(Label+FVector2D(9,4))),User(DisplayName),NameFont,ESlateDrawEffect::None,Settings.LabelColor);
            FSlateDrawElement::MakeText(Out,Base+3,G.ToPaintGeometry(LabelSize,FSlateLayoutTransform(Label+FVector2D(9,NameSize.Y+7))),Caption,CaptionFont,ESlateDrawEffect::None,CaptionColor);
        }
    }
    if(bDragging){const FVector2D Min(FMath::Min(Start.X,Current.X),FMath::Min(Start.Y,Current.Y)),Max(FMath::Max(Start.X,Current.X),FMath::Max(Start.Y,Current.Y));
        FSlateDrawElement::MakeBox(Out,Base+1,G.ToPaintGeometry(Max-Min,FSlateLayoutTransform(Min)),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,MapUI::Cyan.CopyWithNewOpacity(.1f));Rect(Start,Current,MapUI::Cyan);}
    if(bHasTarget){auto* Coordinates=R->GetCoordinateService().GetObject();FVector Anchor;
        if(Coordinates && Telemetry(TargetPrimary,Anchor)){
            const FVector Top=ICoordinateService::Execute_GeographicToWorld(Coordinates,TargetGeographic.Y,TargetGeographic.X,TargetGeographic.Z);
            FVector2D B;if(UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),Top,B,true) && ClearPoint(B)){
                if(Is3DView()){const FVector Bottom=ICoordinateService::Execute_GeographicToWorld(Coordinates,TargetGeographic.Y,TargetGeographic.X,Anchor.Z);FVector2D A;
                    if(UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),Bottom,A,true))FSlateDrawElement::MakeLines(Out,Base+2,G.ToPaintGeometry(),{A,B},ESlateDrawEffect::None,MapUI::Cyan.CopyWithNewOpacity(.7f),true,1.5f);}
                FSlateDrawElement::MakeBox(Out,Base+3,G.ToPaintGeometry(FVector2D(28),FSlateLayoutTransform(B-FVector2D(14))),&TargetRingBrush,ESlateDrawEffect::None,FLinearColor::White);
                const FVector2D LabelSize(240,34);FVector2D L=B+FVector2D(20,-44);
                L.X=FMath::Clamp(L.X,16.,G.GetLocalSize().X-LabelSize.X-16.);
                if(IsOverControls(Desktop.LocalToAbsolute(L+FVector2D(LabelSize.X,LabelSize.Y*.5))))L.X=FMath::Max(16.,B.X-LabelSize.X-20.);
                L.Y=FMath::Clamp(L.Y,196.,G.GetLocalSize().Y-LabelSize.Y-16.);
                if(!ClearLabel(L,LabelSize)){if(Shell)Out.PopClip();return Base+4;}
                FSlateDrawElement::MakeBox(Out,Base+3,G.ToPaintGeometry(LabelSize,FSlateLayoutTransform(L)),&CardBrush,ESlateDrawEffect::None,MapUI::Surface);
                double Ground=0;const bool AGL=Is3DView() && SurfaceHeight(TargetGeographic,Ground);
                FNumberFormattingOptions MarkerFormat;MarkerFormat.SetMaximumFractionalDigits(1);
                const auto Caption=FText::Format(T(AGL?TEXT("FleetTerrain.TargetAGL"):TEXT("Fleet3D.Marker")),FText::AsNumber(TargetGeographic.Z-Ground,&MarkerFormat));
                FSlateDrawElement::MakeText(Out,Base+4,G.ToPaintGeometry(LabelSize,FSlateLayoutTransform(L+FVector2D(9,6))),Caption,FCoreStyle::GetDefaultFontStyle("Bold",16),ESlateDrawEffect::None,MapUI::Cyan);
            }
        }
    }
    if(Shell)Out.PopClip();
    return Base+4;
}
void UMapFleetInteractionWidget::NativeDestruct(){
    for(const auto& Item:SuppressedLabels)if(Item.Key.IsValid())Item.Key->SetVisibility(Item.Value);SuppressedLabels.Reset();
    ModelVisibility.RestoreAll();DisplayedModelActors.Reset();ModelScreenBounds.Reset();
    CancelGesture();PreviewRoutes.Reset();Super::NativeDestruct();
}
