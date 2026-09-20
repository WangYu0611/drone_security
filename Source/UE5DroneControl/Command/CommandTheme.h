#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/EditableTextBox.h"

/** Shared presentation tokens. No mission, telemetry or input ownership. */
namespace CommandTheme
{
inline const FLinearColor Background(FColor(12, 18, 27));
inline const FLinearColor Surface(FColor(19, 28, 40));
inline const FLinearColor Elevated(FColor(27, 40, 55));
inline const FLinearColor Border(FColor(53, 72, 91));
inline const FLinearColor PrimaryText(FColor(235, 243, 250));
inline const FLinearColor SecondaryText(FColor(149, 169, 188));
inline const FLinearColor Cyan(FColor(100, 203, 224));
inline const FLinearColor Online(FColor(88, 195, 151));
inline const FLinearColor Warning(FColor(235, 180, 83));
inline const FLinearColor Critical(FColor(231, 102, 110));
inline const FLinearColor Disabled(FColor(29, 38, 49));
inline const FLinearColor Hover(FColor(40, 63, 80));
inline const FLinearColor Selected(FColor(29, 73, 88));
inline constexpr float Padding = 12.f;
inline constexpr float HeaderHeight = 44.f;
inline constexpr float Radius = 5.f;
inline constexpr int32 CaptionSize = 11;
inline constexpr int32 BodySize = 13;
inline constexpr int32 TitleSize = 17;
inline constexpr int32 NumberSize = 21;

inline void Text(UTextBlock* Widget, int32 Size = BodySize, const FLinearColor& Color = PrimaryText, bool Bold = false)
{
    if (!Widget) return;
    Widget->SetFont(FCoreStyle::GetDefaultFontStyle(Bold ? "Bold" : "Regular", Size));
    Widget->SetColorAndOpacity(Color);
}
inline void Panel(UBorder* Widget, const FLinearColor& Fill = Surface, const FLinearColor& Outline = Border)
{
    if (!Widget) return;
    Widget->SetBrush(FSlateRoundedBoxBrush(Fill, Radius, Outline, 1.f));
    Widget->SetBrushColor(FLinearColor::White);
    Widget->SetContentColorAndOpacity(FLinearColor::White);
}
inline void Button(UButton* Widget, bool IsSelected = false, bool Compact = false)
{
    if (!Widget) return;
    FButtonStyle Style = Widget->GetStyle();
    Style.SetNormal(FSlateRoundedBoxBrush(IsSelected ? Selected : Elevated, Radius, IsSelected ? Cyan : Border, IsSelected ? 2.f : 1.f));
    Style.SetHovered(FSlateRoundedBoxBrush(Hover, Radius, Cyan, 1.f));
    Style.SetPressed(FSlateRoundedBoxBrush(Selected, Radius, Cyan, 2.f));
    Style.SetDisabled(FSlateRoundedBoxBrush(Disabled, Radius, Border, 1.f));
    Style.SetNormalForeground(PrimaryText).SetHoveredForeground(PrimaryText).SetPressedForeground(Cyan).SetDisabledForeground(SecondaryText);
    const FMargin ButtonPadding = Compact ? FMargin(5, 2) : FMargin(10, 6);
    Style.SetNormalPadding(ButtonPadding).SetPressedPadding(ButtonPadding);
    Widget->SetStyle(Style);
    Widget->SetBackgroundColor(FLinearColor::White);
}
inline void Input(UEditableTextBox* Widget)
{
    if (!Widget) return;
    FEditableTextBoxStyle Style = Widget->GetWidgetStyle();
    FTextBlockStyle FieldText = Style.TextStyle;
    FieldText.SetColorAndOpacity(PrimaryText);
    Style.SetTextStyle(FieldText);
    Style.SetForegroundColor(PrimaryText).SetFocusedForegroundColor(PrimaryText).SetReadOnlyForegroundColor(SecondaryText);
    Style.SetBackgroundImageNormal(FSlateRoundedBoxBrush(Background, Radius, Border, 1));
    Style.SetBackgroundImageHovered(FSlateRoundedBoxBrush(Background, Radius, Cyan, 1));
    Style.SetBackgroundImageFocused(FSlateRoundedBoxBrush(Background, Radius, Cyan, 2));
    Style.SetBackgroundImageReadOnly(FSlateRoundedBoxBrush(Disabled, Radius, Border, 1));
    Style.SetBackgroundColor(FLinearColor::White);
    // UE 5.8 forwards &InStyle to Slate, so the argument must outlive this call.
    Widget->WidgetStyle = Style;
    Widget->SetWidgetStyle(Widget->WidgetStyle);
}
// Apply only to an existing Command drawer tree. Does not replace widgets or bindings.
inline void Drawer(UWidgetTree* Tree)
{
    if (!Tree) return;
    Tree->ForEachWidget([](UWidget* Widget)
    {
        if (auto* Card = Cast<UBorder>(Widget)) Panel(Card);
        if (auto* Action = Cast<UButton>(Widget)) Button(Action, false, true);
        if (auto* Label = Cast<UTextBlock>(Widget)) Text(Label, 13);
        if (auto* Field = Cast<UEditableTextBox>(Widget)) Input(Field);
    });
}
}
