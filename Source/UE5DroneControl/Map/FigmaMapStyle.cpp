#include "Map/FigmaMapStyle.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Misc/Paths.h"

UImage* MapUI::Icon(UWidgetTree* Tree,const FString& Name,FVector2D Size){
    struct Asset {const TCHAR* Name;const TCHAR* File;FVector2D Native;float Rotation=0;};
    static const Asset Assets[]{
        {TEXT("locate"),TEXT("6edcf.svg"),FVector2D(260)},
        {TEXT("route"),TEXT("18e1c.svg"),FVector2D(267.584),24.87f},
        {TEXT("route-active"),TEXT("1d2be.svg"),FVector2D(267.584),24.87f},
        {TEXT("video"),TEXT("d6015.svg"),FVector2D(263)},
        {TEXT("hint"),TEXT("54a8a.svg"),FVector2D(251)},
        {TEXT("drone"),TEXT("a7c2b.svg"),FVector2D(543,314.517)},
        {TEXT("drone-route"),TEXT("24dbc.svg"),FVector2D(543,314.517)},
        {TEXT("drone-option"),TEXT("80c89.svg"),FVector2D(319,184.771)},
        {TEXT("selected-ring"),TEXT("d6343.svg"),FVector2D(610.2)},
        {TEXT("radio-active"),TEXT("417f1.svg"),FVector2D(212)},
        {TEXT("radio-dot"),TEXT("41176.svg"),FVector2D(114)},
        {TEXT("radio"),TEXT("59e33.svg"),FVector2D(212)},
        {TEXT("chevron"),TEXT("1c6e1.svg"),FVector2D(108.396,66.2944)},
        {TEXT("chevron-right"),TEXT("9c126.svg"),FVector2D(108.396,66.2944),-90},
        {TEXT("error-disc"),TEXT("db870.svg"),FVector2D(208)},
        {TEXT("divider"),TEXT("6f178.svg"),FVector2D(4590.01,5)},
        {TEXT("menu-divider"),TEXT("6422d.svg"),FVector2D(2908.01,1)}
    };
    if(Name==TEXT("plus") || Name==TEXT("minus"))return FigmaUI::Icon(Tree,Name,Size);
    const Asset* Found=nullptr;for(const auto& A:Assets)if(Name==A.Name){Found=&A;break;}
    if(!ensureMsgf(Found,TEXT("Unknown Map design asset: %s"),*Name))return Tree->ConstructWidget<UImage>();
    static TMap<FString,TUniquePtr<FSlateVectorImageBrush>> Brushes;
    auto& Cached=Brushes.FindOrAdd(Found->File);
    if(!Cached){const auto Path=FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()/TEXT("UI/Figma/Map")/Found->File);ensure(FPaths::FileExists(Path));Cached=MakeUnique<FSlateVectorImageBrush>(Path,Found->Native,FLinearColor::White);}
    const double Angle=FMath::DegreesToRadians(Found->Rotation);
    const FVector2D Bounds(FMath::Abs(FMath::Cos(Angle))*Found->Native.X+FMath::Abs(FMath::Sin(Angle))*Found->Native.Y,FMath::Abs(FMath::Sin(Angle))*Found->Native.X+FMath::Abs(FMath::Cos(Angle))*Found->Native.Y);
    const auto Fitted=Found->Native*FMath::Min(Size.X/Bounds.X,Size.Y/Bounds.Y);
    FSlateBrush Brush=*Cached;Brush.SetImageSize(FVector2f(Fitted));
    auto* Image=Tree->ConstructWidget<UImage>();Image->SetBrush(Brush);Image->SetDesiredSizeOverride(Fitted);Image->SetVisibility(ESlateVisibility::HitTestInvisible);
    if(Found->Rotation!=0){Image->SetRenderTransformPivot(FVector2D(.5));Image->SetRenderTransformAngle(Found->Rotation);}return Image;
}

void MapUI::Button(UButton* Widget,bool IsSelected,bool Compact){
    FigmaUI::Button(Widget,IsSelected,Compact);if(!Widget)return;
    auto Style=Widget->GetStyle();
    Style.SetNormal(FSlateRoundedBoxBrush(IsSelected?FigmaUI::PrimaryText:FigmaUI::Surface,6.f));
    Style.SetHovered(FSlateRoundedBoxBrush(IsSelected?FigmaUI::PrimaryText:FLinearColor(FColor(44,48,61)),6.f,FigmaUI::Accent,1.f));
    Style.SetPressed(FSlateRoundedBoxBrush(IsSelected?FigmaUI::Muted:FigmaUI::SelectedSurface,6.f,FigmaUI::Accent,1.f));
    Style.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(FColor(36,38,47)),6.f,FigmaUI::Outline,1.f));Widget->SetStyle(Style);
}
