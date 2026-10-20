#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"

class UBorder;
class UButton;
class UEditableTextBox;
class UImage;
class UTextBlock;
class UWidgetTree;

/** Shared Figma presentation tokens and original Command assets. Owns no operational state. */
namespace FigmaUI
{
inline const FLinearColor Background(FColor(0, 0, 0));
inline const FLinearColor Surface(FColor(25, 27, 35));       // #191b23
inline const FLinearColor Inset(FColor(14, 19, 28));         // #0e131c
inline const FLinearColor Outline(FColor(54, 62, 76));      // #363e4c
inline const FLinearColor PrimaryText(FColor(255, 255, 255));
inline const FLinearColor Muted(FColor(170, 180, 193));     // #aab4c1
inline const FLinearColor Accent(FColor(118, 54, 242));     // #7636f2
inline const FLinearColor SelectedSurface(FColor(32, 26, 67));
inline const FLinearColor Online(FColor(67, 218, 78));
inline const FLinearColor Warning(FColor(235, 180, 83));
inline const FLinearColor Critical(FColor(231, 102, 110));

UE5DRONECONTROL_API void Text(UTextBlock* Widget, int32 Size = 14,
    const FLinearColor& Color = PrimaryText, bool Bold = false);
UE5DRONECONTROL_API void Panel(UBorder* Widget, const FLinearColor& Fill = Surface,
    const FLinearColor& Border = Outline, float Radius = 10.f, const FMargin& Padding = FMargin(16.f));
UE5DRONECONTROL_API void Button(UButton* Widget, bool Selected = false, bool Compact = false);
UE5DRONECONTROL_API void Input(UEditableTextBox* Widget);

/**
 * Original local SVG, fitted inside Size without changing its source dimensions.
 * Align the returned image centrally in a parent slot (do not stretch it).
 * close/close-alert/close-log include the design's -45 degree rotation already.
 */
UE5DRONECONTROL_API UImage* Icon(UWidgetTree* Tree, const FString& SemanticName,
    FVector2D Size = FVector2D(24.f, 24.f));
}
