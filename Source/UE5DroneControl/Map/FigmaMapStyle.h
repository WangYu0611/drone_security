#pragma once
#include "Command/FigmaCommandStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"

/** Map presentation only. World projection and operational state remain in their owners. */
namespace MapUI {
using FigmaUI::Text;
using FigmaUI::Panel;
using FigmaUI::Input;
UE5DRONECONTROL_API void Button(UButton* Widget,bool Selected=false,bool Compact=false);
inline const FLinearColor Background=FigmaUI::Background,Surface(FColor(13,17,26)),Elevated=FigmaUI::Surface;
inline const FLinearColor PrimaryText=FigmaUI::PrimaryText,SecondaryText=FigmaUI::Muted,Border=FigmaUI::Outline;
inline const FLinearColor Cyan=FigmaUI::Accent,Selected=FigmaUI::SelectedSurface,Online=FigmaUI::Online,Warning=FigmaUI::Warning,Critical=FigmaUI::Critical,Hover(FColor(45,49,61));
UE5DRONECONTROL_API UImage* Icon(UWidgetTree* Tree,const FString& Name,FVector2D Size=FVector2D(20));
inline UCanvasPanelSlot* Place(UCanvasPanel* Parent,UWidget* Child,FVector2D Position,FVector2D Size,int32 Z=0){
    auto* Layout=Parent->AddChildToCanvas(Child);Layout->SetPosition(Position);Layout->SetSize(Size);Layout->SetZOrder(Z);return Layout;
}
// Only chrome is scaled. The parent user widget keeps full viewport geometry for
// ProjectWorldLocationToWidgetPosition, native gestures and waypoint hit testing.
inline UCanvasPanel* Chrome(UWidgetTree* Tree,UCanvasPanel* Parent,int32 Z=0){
    auto* Scale=Tree->ConstructWidget<UScaleBox>();Scale->SetStretch(EStretch::ScaleToFit);Scale->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto* Layout=Parent->AddChildToCanvas(Scale);Layout->SetAnchors(FAnchors(0,0,1,1));Layout->SetOffsets(FMargin(0));Layout->SetZOrder(Z);
    auto* Size=Tree->ConstructWidget<USizeBox>();Size->SetWidthOverride(1440);Size->SetHeightOverride(810);Size->SetVisibility(ESlateVisibility::SelfHitTestInvisible);Scale->SetContent(Size);
    auto* Canvas=Tree->ConstructWidget<UCanvasPanel>();Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);Size->SetContent(Canvas);return Canvas;
}
}
