#include "Command/CommandTacticalMap.h"
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

FVector2D UCommandTacticalMap::Project(double Lat, double Lon, int32 Z)
{
    const double R = FMath::DegreesToRadians(FMath::Clamp(Lat,-85.05112878,85.05112878));
    const double Scale = 256.0 * FMath::Pow(2.0,Z);
    return FVector2D((Lon+180.0)/360.0*Scale, (1.0-FMath::Loge(FMath::Tan(R)+1.0/FMath::Cos(R))/PI)*0.5*Scale);
}
void UCommandTacticalMap::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget = Canvas;
    SetClipping(EWidgetClipping::ClipToBounds);
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
    auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    FDroneDescriptor D; if(!Registry->GetDroneDescriptor(Id,D) || D.bIsEnemyTarget) return false;
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    if(Sync->IsEnabled()) return Sync->SetActiveUAV(Id);
    Registry->SetPrimarySelectedDrone(Id); return true;
}
void UCommandTacticalMap::MarkerClicked(FName,int32 Id){SelectUAV(Id);}
void UCommandTacticalMap::Refresh()
{
    if(!Canvas) return;
    if(!bDragging && GetOwningPlayer() && GetOwningPlayer()->IsInputKeyDown(EKeys::LeftMouseButton))return;
    const FVector2D Size=GetCachedGeometry().GetLocalSize(); if(Size.X<10 || Size.Y<10)return;
    auto* Registry=GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    const auto Drones=Registry->GetFriendlyDroneDescriptors();
    if(!bCentered) for(const auto& D:Drones)
    {
        FDroneTelemetrySnapshot T;
        if(Registry->GetTelemetry(D.DroneId,T) && T.bGpsFix && FMath::IsFinite(T.GpsLatitude) && FMath::IsFinite(T.GpsLongitude) && FMath::Abs(T.GpsLatitude)<=90 && FMath::Abs(T.GpsLongitude)<=180)
        {Center=Project(T.GpsLatitude,T.GpsLongitude,0)/256.0;bCentered=true;break;}
    }
    Canvas->ClearChildren();
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
    int32 Missing=0;
    for(const auto& D:Drones)
    {
        FDroneTelemetrySnapshot T;
        if(!Registry->GetTelemetry(D.DroneId,T) || !T.bGpsFix || !FMath::IsFinite(T.GpsLatitude) || !FMath::IsFinite(T.GpsLongitude) || FMath::Abs(T.GpsLatitude)>90 || FMath::Abs(T.GpsLongitude)>180){++Missing;continue;}
        const FVector2D P=Project(T.GpsLatitude,T.GpsLongitude,Zoom)-Origin;
        auto* B=WidgetTree->ConstructWidget<UCommandActionButton>();B->Configure(TEXT("select"),D.DroneId);B->OnAction.AddDynamic(this,&UCommandTacticalMap::MarkerClicked);
        bool Active=Registry->GetPrimarySelectedDrone()==D.DroneId;CommandTheme::Button(B,Active,true);
        auto* L=WidgetTree->ConstructWidget<UTextBlock>();L->SetText(FText::FromString(FString::Printf(TEXT("%s UAV-%02d%s"),Active?*ProductText::Get(TEXT("Tactical.Active")).ToString():TEXT("+"),D.DroneId,AlertDrones.Contains(D.DroneId)?*ProductText::Get(TEXT("Tactical.Alert")).ToString():TEXT(""))));
        CommandTheme::Text(L,15,AlertDrones.Contains(D.DroneId)?CommandTheme::Warning:Active?CommandTheme::Cyan:CommandTheme::PrimaryText);B->SetContent(L);
        auto* S=Canvas->AddChildToCanvas(B);S->SetPosition(P);S->SetAutoSize(true);S->SetZOrder(2);
    }
    Label(FText::Format(ProductText::Get(TEXT("Tactical.Header")),FText::AsNumber(Zoom),FText::AsNumber(Missing)).ToString(),FVector2D(12,8),15,CommandTheme::PrimaryText);
    Label(Attribution,FVector2D(12,Size.Y-26),12,CommandTheme::PrimaryText);
    if(!Error.IsEmpty())Label(Error,FVector2D(12,Size.Y-52),12,CommandTheme::Warning);
}
FReply UCommandTacticalMap::NativeOnMouseWheel(const FGeometry&,const FPointerEvent& E)
{Zoom=FMath::Clamp(Zoom+(E.GetWheelDelta()>0?1:-1),3,19);Refresh();return FReply::Handled();}
FReply UCommandTacticalMap::NativeOnMouseButtonDown(const FGeometry&,const FPointerEvent& E)
{if(E.GetEffectingButton()==EKeys::LeftMouseButton){bDragging=true;return FReply::Handled().CaptureMouse(TakeWidget());}return FReply::Unhandled();}
FReply UCommandTacticalMap::NativeOnMouseButtonUp(const FGeometry&,const FPointerEvent&)
{bDragging=false;return FReply::Handled().ReleaseMouseCapture();}
FReply UCommandTacticalMap::NativeOnMouseMove(const FGeometry& G,const FPointerEvent& E)
{if(!bDragging)return FReply::Unhandled();Center-=(G.AbsoluteToLocal(E.GetScreenSpacePosition())-G.AbsoluteToLocal(E.GetLastScreenSpacePosition()))/(256.0*FMath::Pow(2.0,Zoom));bCentered=true;Refresh();return FReply::Handled();}
