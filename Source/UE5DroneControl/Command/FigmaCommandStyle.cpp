#include "Command/FigmaCommandStyle.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"

namespace
{
struct FIconAsset
{
    const TCHAR* Name;
    const TCHAR* File;
    FVector2D NativeSize;
    float Rotation = 0.f;
};

// Dimensions and files come from the eight get_design_context responses.
// asset-manifest.json records each original frame/variable and the byte hash.
const FIconAsset Assets[] = {
    {TEXT("brand-mark"), TEXT("7fed1.svg"), FVector2D(543.134, 314.596)},
    {TEXT("brand-wordmark"), TEXT("32639.svg"), FVector2D(2199.44, 197.22)},
    {TEXT("drone"), TEXT("d302f.svg"), FVector2D(415., 240.377)},
    {TEXT("drone-preview"), TEXT("2da1b.svg"), FVector2D(481.635, 278.973)},
    {TEXT("plan-detail-drone"), TEXT("Refresh20261029/7abc1.svg"), FVector2D(698.563, 405.54)},
    {TEXT("plan-detail-back"), TEXT("Refresh20261029/6bd41.svg"), FVector2D(114.5, 81.0036), 180.f},
    {TEXT("plan-detail-warning"), TEXT("Map/db870.svg"), FVector2D(208, 208)},
    {TEXT("plan-detail-track"), TEXT("Refresh20261029/a3339.svg"), FVector2D(9137, 407)},
    {TEXT("plan-detail-divider"), TEXT("Refresh20261029/d0a41.svg"), FVector2D(10425, 5)},
    {TEXT("plan-detail-review-divider"), TEXT("Refresh20261029/7494c.svg"), FVector2D(5405.07, 5)},
    {TEXT("plan-detail-record-divider"), TEXT("Refresh20261029/5e434.svg"), FVector2D(5155, 5)},
    {TEXT("plan-detail-mission-divider"), TEXT("Refresh20261029/f80f8.svg"), FVector2D(930, 5), -90.f},
    {TEXT("plan-detail-readiness-divider"), TEXT("Refresh20261029/d7712.svg"), FVector2D(361, 5), -90.f},
    {TEXT("workflow-arrow"), TEXT("Refresh20261029/58932.svg"), FVector2D(163.11, 51.9615)},
    {TEXT("online"), TEXT("9a8cd.svg"), FVector2D(297., 297.)},
    {TEXT("alarm"), TEXT("0b02f.svg"), FVector2D(330., 330.)},
    {TEXT("log"), TEXT("dcb0b.svg"), FVector2D(293., 293.)},
    {TEXT("task"), TEXT("a87d1.svg"), FVector2D(454., 454.)},
    {TEXT("plan"), TEXT("0c589.svg"), FVector2D(306., 306.)},
    {TEXT("folder"), TEXT("04d38.svg"), FVector2D(1217., 1217.)},
    {TEXT("no-position"), TEXT("13c46.svg"), FVector2D(601.174, 601.174)},
    {TEXT("no-video"), TEXT("88d4b.svg"), FVector2D(753., 753.)},
    {TEXT("no-alerts"), TEXT("eee63.svg"), FVector2D(968., 968.)},
    {TEXT("edit"), TEXT("af5be.svg"), FVector2D(286., 286.)},
    {TEXT("altitude"), TEXT("d1330.svg"), FVector2D(457.625, 326.875)},
    {TEXT("speed"), TEXT("f2f72.svg"), FVector2D(484., 484.)},
    {TEXT("battery"), TEXT("fc097.svg"), FVector2D(463., 463.)},
    {TEXT("link"), TEXT("2ec8e.svg"), FVector2D(489., 489.)},
    {TEXT("close"), TEXT("725b6.svg"), FVector2D(257.442, 257.442), -45.f},
    {TEXT("close-alert"), TEXT("3d45a.svg"), FVector2D(257.442, 257.442), -45.f},
    {TEXT("close-log"), TEXT("3d45a.svg"), FVector2D(257.442, 257.442), -45.f},
    {TEXT("arrow-right"), TEXT("8ccfc.svg"), FVector2D(360., 360.)},
    {TEXT("chevron-right"), TEXT("e9dcb.svg"), FVector2D(66.1473, 108.254)},
    {TEXT("chevron-down"), TEXT("7a78a.svg"), FVector2D(92.2258, 69.699)},
    {TEXT("plus"), TEXT("f21a9.svg"), FVector2D(138., 138.)},
    {TEXT("plus-plan"), TEXT("f21a9-6f68a840.svg"), FVector2D(138., 138.)},
    {TEXT("plus-black"), TEXT("967dd.svg"), FVector2D(138., 138.)},
    {TEXT("plus-label"), TEXT("fc0e2.svg"), FVector2D(219., 219.)},
    {TEXT("minus"), TEXT("8ea89.svg"), FVector2D(138., 20.)},
    {TEXT("follow"), TEXT("b04c8.svg"), FVector2D(228., 228.)},
    {TEXT("refresh"), TEXT("525ec.svg"), FVector2D(349., 349.)},
    {TEXT("fullscreen"), TEXT("7bd2d.svg"), FVector2D(378., 378.)},
    {TEXT("check"), TEXT("b84be.svg"), FVector2D(251., 251.)},
    {TEXT("check-dark"), TEXT("5f292.svg"), FVector2D(295., 209.)},
    {TEXT("health"), TEXT("ca9f9.svg"), FVector2D(339., 339.)},
    {TEXT("status-online"), TEXT("07f8a.svg"), FVector2D(112., 112.)},
    {TEXT("status-online-large"), TEXT("b4f9d.svg"), FVector2D(156., 156.)},
    {TEXT("status-offline"), TEXT("f261e.svg"), FVector2D(156., 156.)},
    {TEXT("icon-disc"), TEXT("94272.svg"), FVector2D(565., 565.)},
    {TEXT("label-light"), TEXT("661eb.svg"), FVector2D(650., 650.)},
    {TEXT("label-gray"), TEXT("ef49a.svg"), FVector2D(650., 650.)},
    {TEXT("label-blue"), TEXT("57f65.svg"), FVector2D(650., 650.)},
    {TEXT("label-purple"), TEXT("66921.svg"), FVector2D(650., 650.)},
    {TEXT("label-yellow"), TEXT("51b48.svg"), FVector2D(650., 650.)},
    {TEXT("label-green"), TEXT("c90d1.svg"), FVector2D(650., 650.)},
    {TEXT("label-ring"), TEXT("77ab9.svg"), FVector2D(720., 720.)},
    {TEXT("label-dot"), TEXT("bb35b.svg"), FVector2D(257., 257.)},
    {TEXT("label-color-0"), TEXT("661eb.svg"), FVector2D(650., 650.)},
    {TEXT("label-color-1"), TEXT("ef49a.svg"), FVector2D(650., 650.)},
    {TEXT("label-color-2"), TEXT("57f65.svg"), FVector2D(650., 650.)},
    {TEXT("label-color-3"), TEXT("66921.svg"), FVector2D(650., 650.)},
    {TEXT("label-color-4"), TEXT("51b48.svg"), FVector2D(650., 650.)},
    {TEXT("label-color-5"), TEXT("c90d1.svg"), FVector2D(650., 650.)},
    {TEXT("label-color-ring"), TEXT("77ab9.svg"), FVector2D(720., 720.)},
    {TEXT("label-slider-thumb"), TEXT("bb35b.svg"), FVector2D(257., 257.)},
    {TEXT("label-separator"), TEXT("8dd75.svg"), FVector2D(5591., 5.)},
    {TEXT("fleet-divider"), TEXT("428c1.svg"), FVector2D(10726., 5.)},
    {TEXT("metric-divider"), TEXT("d305f.svg"), FVector2D(555.001, 5.), -89.9f},
};

void ButtonText(UWidget* Widget, const FLinearColor& Color)
{
    if (auto* Label = Cast<UTextBlock>(Widget)) Label->SetColorAndOpacity(Color);
    if (auto* Container = Cast<UPanelWidget>(Widget))
        for (UWidget* Child : Container->GetAllChildren()) ButtonText(Child, Color);
}
}

void FigmaUI::Text(UTextBlock* Widget, int32 Size, const FLinearColor& Color, bool Bold)
{
    if (!Widget) return;
    // CoreStyle's composite font retains the engine's CJK fallback on Windows.
    // Slate uses points (96/72 scale); design sizes are logical pixels.
    Widget->SetFont(FCoreStyle::GetDefaultFontStyle(Bold ? "Bold" : "Regular", FMath::Max(1,FMath::RoundToInt(Size*.75f))));
    Widget->SetColorAndOpacity(Color);
}

void FigmaUI::Panel(UBorder* Widget, const FLinearColor& Fill, const FLinearColor& Border,
    float Radius, const FMargin& Padding)
{
    if (!Widget) return;
    Widget->SetBrush(FSlateRoundedBoxBrush(Fill, Radius, Border, 1.f));
    Widget->SetBrushColor(FLinearColor::White);
    Widget->SetContentColorAndOpacity(FLinearColor::White);
    Widget->SetPadding(Padding);
}

void FigmaUI::Button(UButton* Widget, bool Selected, bool Compact)
{
    if (!Widget) return;
    const FLinearColor DisabledFill(FColor(36, 38, 47));
    const FLinearColor DisabledText(FColor(92, 95, 105));
    FButtonStyle Style = Widget->GetStyle();
    // Height-based rounding stays a capsule at both compact and normal sizes.
    Style.SetNormal(FSlateRoundedBoxBrush(Selected ? PrimaryText : Surface));
    Style.SetHovered(FSlateRoundedBoxBrush(Selected ? PrimaryText : FLinearColor(FColor(44, 48, 61)), Selected ? PrimaryText : Accent, 1.f));
    Style.SetPressed(FSlateRoundedBoxBrush(Selected ? Muted : SelectedSurface, Accent, 1.f));
    Style.SetDisabled(FSlateRoundedBoxBrush(DisabledFill, Outline, 1.f));
    Style.SetNormalForeground(Selected ? Background : PrimaryText)
        .SetHoveredForeground(Selected ? Background : PrimaryText)
        .SetPressedForeground(Selected ? Background : PrimaryText)
        .SetDisabledForeground(DisabledText);
    const FMargin Padding = Compact ? FMargin(12.f, 6.f) : FMargin(20.f, 9.f);
    Style.SetNormalPadding(Padding).SetPressedPadding(Padding);
    Widget->SetStyle(Style);
    Widget->SetBackgroundColor(FLinearColor::White);
    ButtonText(Widget->GetContent(), Widget->GetIsEnabled() ? (Selected ? Background : PrimaryText) : DisabledText);
}

void FigmaUI::Input(UEditableTextBox* Widget)
{
    if (!Widget) return;
    FEditableTextBoxStyle Style = Widget->GetWidgetStyle();
    const FLinearColor Fill(FColor(12, 14, 19));
    Style.SetBackgroundImageNormal(FSlateRoundedBoxBrush(Fill, 7.f, Outline, 1.f));
    Style.SetBackgroundImageHovered(FSlateRoundedBoxBrush(Fill, 7.f, Muted, 1.f));
    Style.SetBackgroundImageFocused(FSlateRoundedBoxBrush(Fill, 7.f, Accent, 1.f));
    Style.SetBackgroundImageReadOnly(FSlateRoundedBoxBrush(Surface, 7.f, Outline, 1.f));
    Style.SetForegroundColor(PrimaryText).SetFocusedForegroundColor(PrimaryText).SetReadOnlyForegroundColor(Muted);
    Style.SetBackgroundColor(FLinearColor::White);
    Style.SetPadding(FMargin(12.f, 9.f));
    FTextBlockStyle FieldText = Style.TextStyle;
    FieldText.SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 11));
    FieldText.SetColorAndOpacity(PrimaryText);
    Style.SetTextStyle(FieldText);
    // Slate retains this style's address; the owning UWidget outlives its Slate view.
    Widget->WidgetStyle = Style;
    Widget->SetWidgetStyle(Widget->WidgetStyle);
}

UImage* FigmaUI::Icon(UWidgetTree* Tree, const FString& SemanticName, FVector2D Size)
{
    if (!Tree) return nullptr;
    UImage* Image = Tree->ConstructWidget<UImage>();
    Image->SetVisibility(ESlateVisibility::HitTestInvisible);
    const FIconAsset* Asset = nullptr;
    for (const FIconAsset& Candidate : Assets)
        if (SemanticName == Candidate.Name) { Asset = &Candidate; break; }
    if (!ensureMsgf(Asset, TEXT("Unknown Figma Command asset: %s"), *SemanticName)) return Image;

    // Static ownership also keeps cached brushes alive across widget rebuilds.
    // UImage::SetBrush copies FSlateBrush into its UPROPERTY; no stack pointer escapes.
    static TMap<FString, TUniquePtr<FSlateVectorImageBrush>> Brushes;
    TUniquePtr<FSlateVectorImageBrush>& Cached = Brushes.FindOrAdd(Asset->File);
    if (!Cached)
    {
        const FString File = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("UI/Figma"), Asset->File));
        ensureMsgf(FPaths::FileExists(File), TEXT("Missing local Figma Command SVG: %s"), *File);
        Cached = MakeUnique<FSlateVectorImageBrush>(File, Asset->NativeSize, FLinearColor::White);
    }
    const double Angle = FMath::DegreesToRadians(Asset->Rotation);
    const FVector2D RotatedBounds(
        FMath::Abs(FMath::Cos(Angle)) * Asset->NativeSize.X + FMath::Abs(FMath::Sin(Angle)) * Asset->NativeSize.Y,
        FMath::Abs(FMath::Sin(Angle)) * Asset->NativeSize.X + FMath::Abs(FMath::Cos(Angle)) * Asset->NativeSize.Y);
    const double Scale = FMath::Max(0., FMath::Min(Size.X / RotatedBounds.X, Size.Y / RotatedBounds.Y));
    const FVector2D Fitted = Asset->NativeSize * Scale;
    FSlateBrush Brush = *Cached;
    Brush.SetImageSize(FVector2f(Fitted));
    Image->SetBrush(Brush);
    Image->SetDesiredSizeOverride(Fitted);
    Image->SetColorAndOpacity(FLinearColor::White);
    if (!FMath::IsNearlyZero(Asset->Rotation))
    {
        Image->SetRenderTransformPivot(FVector2D(.5, .5));
        Image->SetRenderTransformAngle(Asset->Rotation);
    }
    return Image;
}
