#include "Command/CommandTacticalMap.h"
#include "Command/FigmaCommandStyle.h"
#include "Shared/PlanWidgetSupport.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Shared/ProductText.h"
#include "Command/CommandTheme.h"
#include "Command/CommandActionButton.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Command/CommandAlertStore.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "ImageUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "GameFramework/PlayerController.h"
#include "Map/MapMoveSupport.h"
#include "Map/MapStartupSettings.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Framework/Application/SlateApplication.h"

FVector2D UCommandTacticalMap::Project(double Lat, double Lon, int32 Z)
{
    const double R = FMath::DegreesToRadians(FMath::Clamp(Lat,-85.05112878,85.05112878));
    const double Scale = 256.0 * FMath::Pow(2.0,Z);
    return FVector2D((Lon+180.0)/360.0*Scale, (1.0-FMath::Loge(FMath::Tan(R)+1.0/FMath::Cos(R))/PI)*0.5*Scale);
}
FVector2D UCommandTacticalMap::Unproject(const FVector2D& Pixel,int32 Z)
{
    const double Scale=256.*FMath::Pow(2.,Z);
    return FVector2D(FMath::UnwindDegrees(Pixel.X/Scale*360.-180.),FMath::RadiansToDegrees(FMath::Atan(FMath::Sinh(PI*(1.-2.*Pixel.Y/Scale)))));
}
void UCommandTacticalMap::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget = Canvas;
    SetClipping(EWidgetClipping::ClipToBounds);
    SetVisibility(ESlateVisibility::Visible);Canvas->SetVisibility(ESlateVisibility::Visible);
    const auto Startup=FMapStartupSettings::Read();
    if(Startup.bLaunchOverride && Startup.bEnabled)
    {
        Center=Project(Startup.LongitudeLatitudeHeight.Y,Startup.LongitudeLatitudeHeight.X,0)/256.;
        bCentered=true;
    }
    // Use the same WGS84 XYZ configuration as the main Cesium raster when provided.
    GConfig->GetString(TEXT("CommandMap"),TEXT("BasemapUrl"),Template,GGameIni);
    GConfig->GetString(TEXT("CommandTacticalMap"),TEXT("BasemapUrl"),Template,GGameIni);
    GConfig->GetString(TEXT("CommandTacticalMap"),TEXT("Attribution"),Attribution,GGameIni);
    Template.ReplaceInline(TEXT("$z"),TEXT("{z}")); Template.ReplaceInline(TEXT("$x"),TEXT("{x}")); Template.ReplaceInline(TEXT("$y"),TEXT("{y}"));
    if ((!Template.StartsWith(TEXT("https://")) && !Template.StartsWith(TEXT("http://"))) || !Template.Contains(TEXT("{z}")) || !Template.Contains(TEXT("{x}")) || !Template.Contains(TEXT("{y}")))
    { Template.Empty(); Error=TEXT("BASEMAP UNAVAILABLE: configure a WGS84 XYZ source"); }
}
void UCommandTacticalMap::LoadTile(const FString& Key,int32 X,int32 Y)
{
    if (Template.IsEmpty() || Tiles.Contains(Key) || Pending.Contains(Key) || RetryAfter.FindRef(Key)>FPlatformTime::Seconds()) return;
    if (Pending.Num()>=12) return;
    const FString CachePath=FPaths::ProjectSavedDir()/TEXT("TacticalTiles")/FString::Printf(TEXT("%08x"),GetTypeHash(Template))/(Key+TEXT(".png"));
    TArray<uint8> Cached;
    if ((FDateTime::UtcNow()-IFileManager::Get().GetTimeStamp(*CachePath)).GetTotalDays()<7 && FFileHelper::LoadFileToArray(Cached,*CachePath))
        if(auto* Texture=FImageUtils::ImportBufferAsTexture2D(Cached)) {if(Tiles.Num()>=256)Tiles.Empty();Tiles.Add(Key,Texture);return;}
    Pending.Add(Key);
    FString Url=Template.Replace(TEXT("{z}"),*FString::FromInt(Zoom)).Replace(TEXT("{x}"),*FString::FromInt(X)).Replace(TEXT("{y}"),*FString::FromInt(Y));
    auto Request=FHttpModule::Get().CreateRequest(); Request->SetURL(Url); Request->SetVerb(TEXT("GET")); Request->SetTimeout(10);
    Request->SetHeader(TEXT("User-Agent"),TEXT("DroneOps-CommandCenter/2.0"));
    Request->OnProcessRequestComplete().BindWeakLambda(this,[this,Key,CachePath](FHttpRequestPtr,FHttpResponsePtr Response,bool Ok)
    {
        Pending.Remove(Key);
        if (Ok && Response && Response->GetResponseCode()==200 && Response->GetContentLength()<8*1024*1024)
        {
            if (auto* Texture=FImageUtils::ImportBufferAsTexture2D(Response->GetContent()))
            { if(Tiles.Num()>=256) Tiles.Empty(); Tiles.Add(Key,Texture); IFileManager::Get().MakeDirectory(*FPaths::GetPath(CachePath),true);FFileHelper::SaveArrayToFile(Response->GetContent(),*CachePath);Error.Empty(); return; }
        }
        RetryAfter.Add(Key,FPlatformTime::Seconds()+30); Error=TEXT("BASEMAP PARTIALLY UNAVAILABLE / retrying");
    });
    if(!Request->ProcessRequest()){Pending.Remove(Key);RetryAfter.Add(Key,FPlatformTime::Seconds()+30);}
}
bool UCommandTacticalMap::SelectUAV(int32 Id)
{
    if(bSelectionPending || bMovePending)return false;
    auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    FDroneDescriptor D; if(!Registry->GetDroneDescriptor(Id,D) || D.bIsEnemyTarget) return false;
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    ClearMoveTarget();MoveMessage=FText::GetEmpty();
    if(Sync->IsEnabled()){
        RequestedSelection={Id};RequestedPrimary=Id;SelectionRequestedAt=FPlatformTime::Seconds();
        bSelectionPending=Sync->SetLocalSelection(Id,RequestedSelection);return bSelectionPending;
    }
    Registry->SetMultiSelectedDrones({Id});Registry->SetPrimarySelectedDrone(Id); return true;
}
void UCommandTacticalMap::MarkerClicked(FName,int32 Id){
    if(bSelectionPending || bMovePending)return;
    const auto Keys=FSlateApplication::Get().GetModifierKeys();
    if(!Keys.IsControlDown() && !Keys.IsShiftDown()){SelectUAV(Id);return;}
    auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    auto Members=Registry->GetMultiSelectedDrones();
    if(Keys.IsControlDown() && Members.Contains(Id))Members.Remove(Id);else Members.AddUnique(Id);Members.Sort();
    RequestedPrimary=Members.Contains(Registry->GetPrimarySelectedDrone())?Registry->GetPrimarySelectedDrone():Members.IsEmpty()?0:Members[0];
    RequestedSelection=Members;SelectionRequestedAt=FPlatformTime::Seconds();ClearMoveTarget();MoveMessage=FText::GetEmpty();
    bSelectionPending=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->SetLocalSelection(RequestedPrimary,Members);
    if(!bSelectionPending)MoveMessage=PlanUI::T(TEXT("Errors.SYNC_OFFLINE"));
}
void UCommandTacticalMap::ClearMoveTarget(){bHasMoveTarget=false;MoveMembers.Empty();MoveTakeover={};}
FText UCommandTacticalMap::MoveUnavailable() const {
    if(bSelectionPending)return PlanUI::T(TEXT("Fleet.SelectionPending"));
    return MapMove::Unavailable(GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>(),GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>(),MapMove::Selection(GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()),true);
}
bool UCommandTacticalMap::PickMoveTarget(const FVector2D& Local){
    if(bMovePending)return false;
    ClearMoveTarget();MoveErrorCode.Empty();MoveMessage=MoveUnavailable();if(!MoveMessage.IsEmpty())return false;
    auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    const auto Members=MapMove::Selection(Registry);FVector Anchor;
    if(!MapMove::Position(Registry,Sync->GetPlans(),Members[0],Anchor,true))return false;
    const FVector2D Geo=Unproject(Center*(256.*FMath::Pow(2.,Zoom))+Local-GetCachedGeometry().GetLocalSize()*.5,Zoom);
    if(Geo.ContainsNaN() || FMath::Abs(Geo.Y)>80.){MoveMessage=PlanUI::T(TEXT("Map.InvalidPosition"));return false;}
    const double North=FMath::DegreesToRadians(Geo.Y-Anchor.Y)*6371000.;
    const double East=FMath::DegreesToRadians(FMath::UnwindDegrees(Geo.X-Anchor.X))*6371000.*FMath::Cos(FMath::DegreesToRadians((Geo.Y+Anchor.Y)*.5));
    if(FVector2D(North,East).Size()>10000.){MoveMessage=PlanUI::T(TEXT("Errors.TRAJECTORY_DOMAIN_UNSUPPORTED"));return false;}
    MoveTarget=FVector(Geo.X,Geo.Y,Anchor.Z);MoveMembers=Members;MoveTakeover=MapMove::CaptureTakeover(Sync->GetPlans(),Members);bHasMoveTarget=true;bFollow=false;MoveMessage=FText::GetEmpty();return true;
}
void UCommandTacticalMap::MoveAction(FName Action,int32){
    if(bMovePending)return;
    if(Action==TEXT("cancel_move")){ClearMoveTarget();MoveMessage=FText::GetEmpty();Refresh();return;}
    if(Action!=TEXT("confirm_move"))return;
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    if(bHasMoveTarget && !MapMove::PreviewIsCurrent(Sync->GetPlans(),MapMove::Selection(GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()),MoveTakeover)){
        ClearMoveTarget();MoveMessage=PlanUI::T(TEXT("Takeover.SourceChanged"));Refresh();return;
    }
    MoveMessage=MoveUnavailable();
    if(!bHasMoveTarget || !MoveMessage.IsEmpty()){Refresh();return;}
    if(MoveMembers!=MapMove::Selection(GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>())){
        ClearMoveTarget();MoveMessage=PlanUI::T(TEXT("Fleet.SelectionChanged"));Refresh();return;
    }
    const auto Request=MapMove::PreviewRequest(Sync->GetPlans(),MoveMembers,MoveTarget,MoveTakeover);
    if(!Request){ClearMoveTarget();MoveMessage=PlanUI::T(TEXT("Takeover.SourceChanged"));Refresh();return;}
    bMovePending=true;MoveErrorCode.Empty();const auto Weak=TWeakObjectPtr<UCommandTacticalMap>(this);
    if(!Sync->SubmitPlan(Request.ToSharedRef(),[Weak](TSharedPtr<FJsonObject> Reply){
        if(!Weak.IsValid())return;auto* Self=Weak.Get();Self->bMovePending=false;
        const FString Code=PlanUI::Error(Reply);Self->MoveErrorCode=Code;Self->MoveMessage=Code.IsEmpty()?PlanUI::T(TEXT("FleetUX.Accepted")):PlanUI::T(*(TEXT("Errors.")+Code));
        const FText Reservation=PlanUI::ReservationError(Reply);if(!Reservation.IsEmpty())Self->MoveMessage=Reservation;
        const auto Params=PlanUI::Object(Reply,TEXT("params"));if(Params && Params->HasField(TEXT("minimum_separation_m")))
            Self->MoveMessage=FText::Format(PlanUI::T(TEXT("Group.Conflict")),PlanUI::User(PlanUI::Field(Params,TEXT("uav_a"))),PlanUI::User(PlanUI::Field(Params,TEXT("uav_b"))),FText::AsNumber(Params->GetNumberField(TEXT("minimum_separation_m"))),FText::AsNumber(Params->GetNumberField(TEXT("required_separation_m"))));
        if(Code.IsEmpty())Self->ClearMoveTarget();Self->Refresh();
    })){bMovePending=false;MoveErrorCode=TEXT("SYNC_OFFLINE");MoveMessage=PlanUI::T(TEXT("Errors.SYNC_OFFLINE"));}Refresh();
}
void UCommandTacticalMap::AddMoveControls(const FVector2D& Size){
    using namespace PlanUI;
    const auto Members=MapMove::Selection(GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>());
    auto* Plate=WidgetTree->ConstructWidget<UBorder>();FigmaUI::Panel(Plate,FigmaUI::Inset,FigmaUI::Outline,8,FMargin(10));
    auto* Body=WidgetTree->ConstructWidget<UVerticalBox>();Plate->SetContent(Body);
    auto* Hint=WidgetTree->ConstructWidget<UTextBlock>();Hint->SetAutoWrapText(true);FigmaUI::Text(Hint,14);
    FText Text=bHasMoveTarget?FText::Format(T(TEXT("MapMove.Preview")),FText::AsNumber(MoveMembers.Num()),User(FString::Printf(TEXT("%.6f"),MoveTarget.Y)),User(FString::Printf(TEXT("%.6f"),MoveTarget.X)),FText::AsNumber(MoveTarget.Z)):
        Members.IsEmpty()?T(TEXT("MapMove.SelectHint")):FText::Format(T(TEXT("MapMove.ClickHint")),FText::AsNumber(Members.Num()));
    const auto Unavailable=MoveUnavailable();
    if(bMovePending)Text=T(TEXT("Common.Waiting"));else if(!MoveMessage.IsEmpty())Text=MoveMessage;else if(bSelectionPending)Text=Unavailable;
    Hint->SetText(Text);Body->AddChildToVerticalBox(Hint);
    if(bHasMoveTarget && MoveTakeover.IsSet()){
        auto* Warning=WidgetTree->ConstructWidget<UTextBlock>();Warning->SetText(T(TEXT("Takeover.Warning")));Warning->SetAutoWrapText(true);
        FigmaUI::Text(Warning,14,FigmaUI::Warning);Body->AddChildToVerticalBox(Warning)->SetPadding(FMargin(0,8,0,0));
    }
    if(bHasMoveTarget){
        auto* Actions=WidgetTree->ConstructWidget<UHorizontalBox>();Body->AddChildToVerticalBox(Actions)->SetPadding(FMargin(0,8,0,0));
        for(const auto& P:TArray<TPair<FString,FString>>{{TEXT("confirm_move"),TEXT("MapMove.Confirm")},{TEXT("cancel_move"),TEXT("Common.Cancel")}}){
            auto* B=WidgetTree->ConstructWidget<UCommandActionButton>();B->Configure(FName(*P.Key));B->OnAction.AddDynamic(this,&UCommandTacticalMap::MoveAction);FigmaUI::Button(B,P.Key==TEXT("confirm_move"),true);
            auto* L=WidgetTree->ConstructWidget<UTextBlock>();L->SetText(T(P.Key==TEXT("confirm_move") && MoveTakeover.IsSet()?TEXT("Takeover.Confirm"):*P.Value));FigmaUI::Text(L,14);B->SetContent(L);B->SetIsEnabled(!bMovePending && (P.Key!=TEXT("confirm_move") || Unavailable.IsEmpty()));
            auto* Layout=Actions->AddChildToHorizontalBox(B);Layout->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Layout->SetPadding(FMargin(0,0,8,0));
        }
        if(!Unavailable.IsEmpty() && !bMovePending){auto* Reason=WidgetTree->ConstructWidget<UTextBlock>();Reason->SetText(Unavailable);Reason->SetAutoWrapText(true);FigmaUI::Text(Reason,13,FigmaUI::Warning);Body->AddChildToVerticalBox(Reason);}
    }
    auto* Width=WidgetTree->ConstructWidget<USizeBox>();Width->SetWidthOverride(FMath::Min(470.,Size.X-24.));Width->SetContent(Plate);MoveControls=Width;
    auto* Layout=Canvas->AddChildToCanvas(Width);Layout->SetPosition(FVector2D(12,Size.Y-12));Layout->SetAlignment(FVector2D(0,1));Layout->SetAutoSize(true);Layout->SetZOrder(20);
}
void UCommandTacticalMap::Refresh()
{
    if(!Canvas) return;
    if(!bDragging && GetOwningPlayer() && GetOwningPlayer()->IsInputKeyDown(EKeys::LeftMouseButton))return;
    const FVector2D Size=GetCachedGeometry().GetLocalSize(); if(Size.X<10 || Size.Y<10)return;
    auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    auto Selected=Registry->GetMultiSelectedDrones();Selected.Sort();
    if(bSelectionPending && Selected==RequestedSelection && Registry->GetPrimarySelectedDrone()==RequestedPrimary)bSelectionPending=false;
    if(bSelectionPending && (!Sync->IsReady() || FPlatformTime::Seconds()-SelectionRequestedAt>10.)){
        bSelectionPending=false;MoveMessage=PlanUI::T(TEXT("Errors.SYNC_FAILED"));
    }
    if(bHasMoveTarget && MapMove::Selection(Registry)!=MoveMembers){ClearMoveTarget();MoveMessage=PlanUI::T(TEXT("Fleet.SelectionChanged"));}
    if(bHasMoveTarget && !bMovePending && !MapMove::PreviewIsCurrent(Sync->GetPlans(),MapMove::Selection(Registry),MoveTakeover)){
        ClearMoveTarget();if(MoveErrorCode.IsEmpty())MoveMessage=PlanUI::T(TEXT("Takeover.SourceChanged"));
    }
    const auto Drones=Registry->GetFriendlyDroneDescriptors();
    if(!bCentered) for(const auto& D:Drones)
    {
        FVector Geographic;
        if(MapMove::Position(Registry,Sync->GetPlans(),D.DroneId,Geographic))
        {Center=Project(Geographic.Y,Geographic.X,0)/256.0;bCentered=true;break;}
    }
    if(bFollow && !bHasMoveTarget){FVector Geographic;if(MapMove::Position(Registry,Sync->GetPlans(),Registry->GetPrimarySelectedDrone(),Geographic)){Center=Project(Geographic.Y,Geographic.X,0)/256.;bCentered=true;}}
    MoveControls=nullptr;Canvas->ClearChildren();
    if(bFigmaPresentation && !bCentered) {
        auto* Icon=FigmaUI::Icon(WidgetTree,TEXT("no-position"),FVector2D(48));
        auto* IS=Canvas->AddChildToCanvas(Icon);IS->SetPosition(Size*.5-FVector2D(24,45));IS->SetSize(FVector2D(48));
        auto* Caption=WidgetTree->ConstructWidget<UTextBlock>();Caption->SetText(ProductText::Get(TEXT("Figma.WaitingPosition")));FigmaUI::Text(Caption,16,FigmaUI::Muted);
        Caption->SetJustification(ETextJustify::Center);Caption->SetVisibility(ESlateVisibility::HitTestInvisible);
        auto* CS=Canvas->AddChildToCanvas(Caption);CS->SetPosition(Size*.5+FVector2D(-200,14));CS->SetSize(FVector2D(400,28));
        return;
    }
    auto Label=[&](const FString& Text,FVector2D Position,int32 Font,FLinearColor Color)
    {
        auto* L=WidgetTree->ConstructWidget<UTextBlock>();L->SetText(FText::FromString(Text));CommandTheme::Text(L,Font,Color);
        auto* Plate=WidgetTree->ConstructWidget<UBorder>();CommandTheme::Panel(Plate,CommandTheme::Background);Plate->SetPadding(FMargin(6,3));Plate->SetContent(L);
        auto* S=Canvas->AddChildToCanvas(Plate);S->SetPosition(Position);S->SetAutoSize(true);S->SetZOrder(3);Plate->SetVisibility(ESlateVisibility::HitTestInvisible);
    };
    const double Scale=256.0*FMath::Pow(2.0,Zoom);const FVector2D Origin=Center*Scale-Size*0.5;
    const int32 Count=1<<Zoom;
    for(int32 Y=FMath::FloorToInt(Origin.Y/256);Y<=FMath::FloorToInt((Origin.Y+Size.Y)/256);++Y)
    for(int32 X=FMath::FloorToInt(Origin.X/256);X<=FMath::FloorToInt((Origin.X+Size.X)/256);++X)
    {
        if(Y<0 || Y>=Count)continue;
        const int32 WrappedX=(X%Count+Count)%Count;
        FString Key=FString::Printf(TEXT("%d/%d/%d"),Zoom,WrappedX,Y);LoadTile(Key,WrappedX,Y);
        if(auto* Texture=Tiles.Find(Key))
        {auto* I=WidgetTree->ConstructWidget<UImage>();I->SetBrushFromTexture(*Texture);I->SetColorAndOpacity(FLinearColor(.65f,.7f,.75f,1.f));I->SetVisibility(ESlateVisibility::HitTestInvisible);
        auto* S=Canvas->AddChildToCanvas(I);S->SetPosition(FVector2D(X*256,Y*256)-Origin);S->SetSize(FVector2D(256));}
    }
    TSet<int32> AlertDrones;
    FString ActiveAlert;
    if(auto Context=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetContext()) Context->TryGetStringField(TEXT("active_alert_id"),ActiveAlert);
    for(const auto& A:GetGameInstance()->GetSubsystem<UCommandAlertStore>()->GetAlerts())
        if(!A.bHandled || (!ActiveAlert.IsEmpty() && A.SharedId==ActiveAlert))AlertDrones.Add(A.DroneId);
    for(const auto& Target:Registry->GetAllDroneDescriptors())if(Target.bIsEnemyTarget){FDroneTelemetrySnapshot T;if(Registry->GetTelemetry(Target.DroneId,T) && T.bGpsFix){
        Label(Target.Name,Project(T.GpsLatitude,T.GpsLongitude,Zoom)-Origin,15,CommandTheme::Warning);}}
    int32 Missing=0;
    for(const auto& D:Drones)
    {
        FDroneTelemetrySnapshot T;Registry->GetTelemetry(D.DroneId,T);FVector Geographic;
        if(!MapMove::Position(Registry,Sync->GetPlans(),D.DroneId,Geographic)){++Missing;continue;}
        const FVector2D P=Project(Geographic.Y,Geographic.X,Zoom)-Origin;
        auto* B=WidgetTree->ConstructWidget<UCommandActionButton>();B->Configure(TEXT("select"),D.DroneId);B->OnAction.AddDynamic(this,&UCommandTacticalMap::MarkerClicked);
        const bool Active=Registry->GetMultiSelectedDrones().Contains(D.DroneId);FigmaUI::Button(B,Active,true);
        auto* Icon=FigmaUI::Icon(WidgetTree,TEXT("drone"),FVector2D(28));B->SetContent(Icon);
        auto IconStyle=B->GetStyle();IconStyle.SetNormalPadding(FMargin(4)).SetPressedPadding(FMargin(4));B->SetStyle(IconStyle);
        B->SetIsEnabled(!bSelectionPending && !bMovePending);
        const auto State=ProductText::Get(T.Availability==EDroneAvailability::Online?TEXT("Fleet.Online"):T.Availability==EDroneAvailability::Lost?TEXT("Fleet.Lost"):TEXT("Fleet.Offline"));
        FText Caption=FText::Format(FText::AsCultureInvariant(TEXT("{0} · {1}")),PlanUI::User(MapMove::UAV(D.DroneId)),State);
        if(T.bSimulation)Caption=FText::Format(ProductText::Get(TEXT("MapMove.MockMarker")),Caption);
        B->SetToolTipText(FText::Format(FText::AsCultureInvariant(TEXT("{0}\n{1}")),PlanUI::User(D.Name),Caption));
        FDroneLabelSettings Settings;Registry->GetDroneLabelSettings(D.DroneId,Settings);
        const FString Name=Settings.DisplayName.IsEmpty()?D.Name:Settings.DisplayName;
        Label(Name,P+FVector2D(24,-22),FMath::Clamp(Settings.FontSize,8,72),Settings.LabelColor);
        Label(Caption.ToString(),P+FVector2D(24,FMath::Clamp(Settings.FontSize,8,72)*1.4-16),11,AlertDrones.Contains(D.DroneId)?CommandTheme::Warning:Active?CommandTheme::Cyan:CommandTheme::PrimaryText);
        auto* MarkerLayout=Canvas->AddChildToCanvas(B);MarkerLayout->SetPosition(P-FVector2D(18));MarkerLayout->SetSize(FVector2D(36));MarkerLayout->SetZOrder(4);
    }
    if(!bFigmaPresentation)Label(FText::Format(ProductText::Get(TEXT("Tactical.Header")),FText::AsNumber(Zoom),FText::AsNumber(Missing)).ToString(),FVector2D(12,8),15,CommandTheme::PrimaryText);
    if(!Attribution.IsEmpty())Label(Attribution,FVector2D(12,Size.Y-172),10,CommandTheme::PrimaryText);
    if(!Error.IsEmpty())Label(Error,FVector2D(12,Size.Y-196),10,CommandTheme::Warning);
    AddMoveControls(Size);
}
FReply UCommandTacticalMap::NativeOnMouseWheel(const FGeometry&,const FPointerEvent& E)
{SetZoom(Zoom+(E.GetWheelDelta()>0?1:-1));return FReply::Handled();}
FReply UCommandTacticalMap::NativeOnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{if(MoveControls && MoveControls->GetCachedGeometry().IsUnderLocation(E.GetScreenSpacePosition()))return FReply::Handled();
    if(E.GetEffectingButton()==EKeys::LeftMouseButton || E.GetEffectingButton()==EKeys::MiddleMouseButton){bDragging=true;bPanning=false;PointerButton=E.GetEffectingButton();PointerStart=PointerLast=G.AbsoluteToLocal(E.GetScreenSpacePosition());return FReply::Handled().CaptureMouse(TakeWidget());}return FReply::Unhandled();}
FReply UCommandTacticalMap::NativeOnMouseButtonUp(const FGeometry& G,const FPointerEvent& E)
{if(!bDragging || E.GetEffectingButton()!=PointerButton)return FReply::Unhandled();bDragging=false;
    if(!bPanning && PointerButton==EKeys::LeftMouseButton && G.IsUnderLocation(E.GetScreenSpacePosition()))PickMoveTarget(G.AbsoluteToLocal(E.GetScreenSpacePosition()));
    PointerButton=FKey();Refresh();return FReply::Handled().ReleaseMouseCapture();}
FReply UCommandTacticalMap::NativeOnMouseMove(const FGeometry& G,const FPointerEvent& E)
{if(!bDragging)return FReply::Unhandled();const auto Local=G.AbsoluteToLocal(E.GetScreenSpacePosition());
    if(!bPanning && FVector2D::Distance(Local,PointerStart)<=5.)return FReply::Handled();bPanning=true;bFollow=false;
    Center-=(Local-PointerLast)/(256.0*FMath::Pow(2.0,Zoom));Center.Y=FMath::Clamp(Center.Y,0.,1.);PointerLast=Local;bCentered=true;Refresh();return FReply::Handled();}

int32 UCommandTacticalMap::NativePaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& C,FSlateWindowElementList& Out,int32 L,const FWidgetStyle& S,bool E) const {
    const int Base=Super::NativePaint(A,G,C,Out,L,S,E);using namespace PlanUI;
    auto PaintOverlays=[&](){
    auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    const auto Origin0=Center*(256.*FMath::Pow(2.,Zoom))-G.GetLocalSize()*.5;
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    for(const auto& D:Registry->GetFriendlyDroneDescriptors()){FDroneTelemetrySnapshot T;Registry->GetTelemetry(D.DroneId,T);FVector Geographic;
        if(!MapMove::Position(Registry,Sync->GetPlans(),D.DroneId,Geographic))continue;
        const auto Pos=Project(Geographic.Y,Geographic.X,Zoom)-Origin0;
        const double Heading=FMath::DegreesToRadians(T.Attitude.Yaw);const FVector2D Dir(FMath::Sin(Heading),-FMath::Cos(Heading)),Side(-Dir.Y,Dir.X);
        TArray<FVector2D> Arrow{Pos+Dir*24.+Side*7.,Pos+Dir*34.,Pos+Dir*24.-Side*7.};
        FSlateDrawElement::MakeLines(Out,L+1,G.ToPaintGeometry(),Arrow,ESlateDrawEffect::None,CommandTheme::Cyan,true,2.f);
    }
    if(bHasMoveTarget && !MoveMembers.IsEmpty()){
        FVector Anchor;if(MapMove::Position(Registry,Sync->GetPlans(),MoveMembers[0],Anchor))for(int32 Id:MoveMembers){FVector From;
            if(!MapMove::Position(Registry,Sync->GetPlans(),Id,From))continue;
            const FVector To(MoveTarget.X+FMath::UnwindDegrees(From.X-Anchor.X),MoveTarget.Y+From.Y-Anchor.Y,MoveTarget.Z+From.Z-Anchor.Z);
            const auto A0=Project(From.Y,From.X,Zoom)-Origin0,B0=Project(To.Y,To.X,Zoom)-Origin0;
            FSlateDrawElement::MakeLines(Out,L+1,G.ToPaintGeometry(),TArray<FVector2D>{A0,B0},ESlateDrawEffect::None,CommandTheme::Cyan,true,2.f);
            FSlateDrawElement::MakeLines(Out,L+1,G.ToPaintGeometry(),TArray<FVector2D>{B0-FVector2D(10,0),B0+FVector2D(10,0)},ESlateDrawEffect::None,CommandTheme::Cyan,true,2.f);
            FSlateDrawElement::MakeLines(Out,L+1,G.ToPaintGeometry(),TArray<FVector2D>{B0-FVector2D(0,10),B0+FVector2D(0,10)},ESlateDrawEffect::None,CommandTheme::Cyan,true,2.f);
        }
    }
    const auto AllExecutions=Object(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),TEXT("executions"));
    if(AllExecutions)for(const auto& Item:AllExecutions->Values){const auto X=Item.Value->AsObject();if(Field(X,TEXT("state"))!=TEXT("EXECUTING") && Field(X,TEXT("state"))!=TEXT("PAUSED"))continue;
        const auto Route=Object(X,TEXT("route_snapshot"));if(!Route)continue;TArray<FVector2D> Line;
        for(const auto& P:Route->GetArrayField(TEXT("waypoints"))){const auto W=P->AsObject();Line.Add(Project(W->GetNumberField(TEXT("latitude")),W->GetNumberField(TEXT("longitude")),Zoom)-Origin0);}
        bool Closed=false;Route->TryGetBoolField(TEXT("bClosedLoop"),Closed);if(Closed && Line.Num()>2)Line.Add(FVector2D(Line[0]));
        if(Line.Num()>1)FSlateDrawElement::MakeLines(Out,Base+2,G.ToPaintGeometry(),Line,ESlateDrawEffect::None,CommandTheme::Warning,true,2.f);
    }

    const auto Plan=Find(Sync->GetPlans(),TEXT("plans"),Field(Sync->GetContext(),TEXT("active_security_plan_id")));if(!Plan)return Base+2;
    const auto Origin=Center*(256.*FMath::Pow(2.,Zoom))-G.GetLocalSize()*.5;
    for(const auto& Id:Plan->GetArrayField(TEXT("mission_ids"))){const auto Mission=Find(Sync->GetPlans(),TEXT("missions"),Id->AsString());const auto Route=Find(Sync->GetPlans(),TEXT("paths"),Field(Mission,TEXT("route_id")));if(!Route)continue;
        TArray<FVector2D> Line;for(const auto& V:Route->GetArrayField(TEXT("waypoints"))){const auto P=V->AsObject();Line.Add(Project(P->GetNumberField(TEXT("latitude")),P->GetNumberField(TEXT("longitude")),Zoom)-Origin);}
        bool Closed=false;Route->TryGetBoolField(TEXT("bClosedLoop"),Closed);if(Closed && Line.Num()>2)Line.Add(FVector2D(Line[0]));
        if(Line.Num()>1)FSlateDrawElement::MakeLines(Out,Base+1,G.ToPaintGeometry(),Line,ESlateDrawEffect::None,CommandTheme::Cyan,true,3);
        if(!Line.IsEmpty())FSlateDrawElement::MakeText(Out,Base+2,G.ToPaintGeometry(FVector2D(220,24),FSlateLayoutTransform(Line[0])),User(Field(Plan,TEXT("name"))),FCoreStyle::GetDefaultFontStyle("Bold",12),ESlateDrawEffect::None,CommandTheme::Cyan);
    }return Base+2;
    };
    if(MoveControls && MoveControls->GetCachedGeometry().GetLocalSize().Y>1.){
        // NativePaint runs after child UMG painting. Clip overlays around the
        // action card so projected routes never cover its text or buttons.
        const auto& ControlGeometry=MoveControls->GetCachedGeometry();
        const auto Min=G.AbsoluteToLocal(ControlGeometry.GetAbsolutePosition());
        const auto Max=G.AbsoluteToLocal(ControlGeometry.LocalToAbsolute(ControlGeometry.GetLocalSize()));
        const auto Size=G.GetLocalSize();
        const TArray<FBox2D> Regions{FBox2D(FVector2D::ZeroVector,FVector2D(Size.X,Min.Y)),FBox2D(FVector2D(0,Min.Y),FVector2D(Min.X,Max.Y)),
            FBox2D(FVector2D(Max.X,Min.Y),FVector2D(Size.X,Max.Y)),FBox2D(FVector2D(0,Max.Y),Size)};
        for(const auto& Region:Regions)if(Region.GetSize().X>0 && Region.GetSize().Y>0){
            Out.PushClip(FSlateClippingZone(G.MakeChild(Region.GetSize(),FSlateLayoutTransform(Region.Min))));PaintOverlays();Out.PopClip();
        }
    }else PaintOverlays();
    return Base+2;
}
