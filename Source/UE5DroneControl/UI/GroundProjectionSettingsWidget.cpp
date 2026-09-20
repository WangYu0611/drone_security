// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroundProjectionSettingsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DroneOps/Drone/DroneGroundProjectionComponent.h"

bool UGroundProjectionSettingsWidget::bStaticPanelInteractive = false;

namespace
{
const FLinearColor PanelBackground(0.025f, 0.045f, 0.075f, 0.96f);
const FLinearColor PrimaryText(0.94f, 0.98f, 1.0f, 1.0f);
const FLinearColor SecondaryText(0.48f, 0.67f, 0.84f, 1.0f);
const FLinearColor AccentText(0.35f, 0.82f, 1.0f, 1.0f);

UTextBlock* MakeText(
	UWidgetTree* WidgetTree,
	const FName Name,
	const FString& Text,
	int32 FontSize,
	const FLinearColor& Color,
	bool bBold = false)
{
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	TextBlock->SetText(FText::FromString(Text));
	FSlateFontInfo Font = TextBlock->GetFont();
	Font.Size = FontSize;
	if (bBold)
	{
		Font.TypefaceFontName = TEXT("Bold");
	}
	TextBlock->SetFont(Font);
	TextBlock->SetColorAndOpacity(FSlateColor(Color));
	return TextBlock;
}
}

void UGroundProjectionSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildFallbackWidgetTree();
}

void UGroundProjectionSettingsWidget::BuildFallbackWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RootCanvas;

	UBorder* PanelBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("GroundProjectionSettingsPanel"));
	PanelBorder->SetBrushColor(PanelBackground);
	PanelBorder->SetPadding(FMargin(16.0f, 13.0f));
	PanelBorder->SetToolTipText(FText::FromString(
		TEXT("调整飞行轨迹到地面的垂直投影线间距；不影响路径规划和无人机飞行。")));
	if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(PanelBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		PanelSlot->SetAlignment(FVector2D::ZeroVector);
		PanelSlot->SetPosition(FVector2D(28.0f, 28.0f));
		PanelSlot->SetSize(FVector2D(390.0f, 154.0f));
	}

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("PanelContent"));
	PanelBorder->AddChild(Content);

	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("HeaderRow"));
	UTextBlock* TitleText = MakeText(
		WidgetTree, TEXT("TitleText"), TEXT("垂直投影线密度"), 17, PrimaryText, true);
	if (UHorizontalBoxSlot* TitleSlot = HeaderRow->AddChildToHorizontalBox(TitleText))
	{
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
	}
	DensityValueText = MakeText(
		WidgetTree, TEXT("DensityValueText"), TEXT("100% · 最高"), 13, AccentText, true);
	HeaderRow->AddChildToHorizontalBox(DensityValueText);
	if (UVerticalBoxSlot* HeaderSlot = Content->AddChildToVerticalBox(HeaderRow))
	{
		HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	UTextBlock* HintText = MakeText(
		WidgetTree,
		TEXT("HintText"),
		TEXT("拖动滑条调整垂直投影线的疏密程度"),
		11,
		SecondaryText);
	Content->AddChildToVerticalBox(HintText);

	UHorizontalBox* SliderRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("SliderRow"));
	UTextBlock* SparseText = MakeText(
		WidgetTree, TEXT("SparseLabel"), TEXT("稀疏"), 11, SecondaryText);
	if (UHorizontalBoxSlot* SparseSlot = SliderRow->AddChildToHorizontalBox(SparseText))
	{
		SparseSlot->SetVerticalAlignment(VAlign_Center);
		SparseSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	}

	USizeBox* SliderSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("SliderSize"));
	SliderSize->SetHeightOverride(40.0f);
	DensitySlider = WidgetTree->ConstructWidget<USlider>(
		USlider::StaticClass(), TEXT("GroundProjectionDensitySlider"));
	DensitySlider->SetMinValue(0.0f);
	DensitySlider->SetMaxValue(1.0f);
	DensitySlider->SetStepSize(0.01f);
	DensitySlider->SetSliderBarColor(FLinearColor(0.18f, 0.43f, 0.58f, 1.0f));
	DensitySlider->SetSliderHandleColor(AccentText);
	DensitySlider->SetToolTipText(FText::FromString(
		TEXT("0% 间距约 2 米，100% 密度最高")));
	DensitySlider->IsFocusable = true;
	DensitySlider->MouseUsesStep = true;
	SliderSize->AddChild(DensitySlider);
	if (UHorizontalBoxSlot* SliderSlot = SliderRow->AddChildToHorizontalBox(SliderSize))
	{
		SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SliderSlot->SetVerticalAlignment(VAlign_Center);
	}

	UTextBlock* DenseText = MakeText(
		WidgetTree, TEXT("DenseLabel"), TEXT("密集"), 11, SecondaryText);
	if (UHorizontalBoxSlot* DenseSlot = SliderRow->AddChildToHorizontalBox(DenseText))
	{
		DenseSlot->SetVerticalAlignment(VAlign_Center);
		DenseSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
	}
	Content->AddChildToVerticalBox(SliderRow);

	UHorizontalBox* FooterRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("FooterRow"));
	StatusText = MakeText(
		WidgetTree,
		TEXT("StatusText"),
		TEXT("投影线将在 30 秒后自动消失"),
		11,
		SecondaryText);
	if (UHorizontalBoxSlot* StatusSlot = FooterRow->AddChildToHorizontalBox(StatusText))
	{
		StatusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		StatusSlot->SetVerticalAlignment(VAlign_Center);
	}

	ClearButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("ClearProjectionRaysButton"));
	ClearButton->SetBackgroundColor(FLinearColor(0.10f, 0.30f, 0.42f, 1.0f));
	ClearButton->SetToolTipText(FText::FromString(TEXT("立即清除当前显示的全部投影线")));
	UTextBlock* ClearText = MakeText(
		WidgetTree, TEXT("ClearButtonText"), TEXT("清除投影线"), 11, PrimaryText, true);
	ClearButton->AddChild(ClearText);
	if (UHorizontalBoxSlot* ClearSlot = FooterRow->AddChildToHorizontalBox(ClearButton))
	{
		ClearSlot->SetVerticalAlignment(VAlign_Center);
		ClearSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
	}
	Content->AddChildToVerticalBox(FooterRow);
}

void UGroundProjectionSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (DensitySlider)
	{
		DensitySlider->OnValueChanged.AddUniqueDynamic(
			this, &UGroundProjectionSettingsWidget::OnDensityChanged);
		DensitySlider->OnMouseCaptureBegin.AddUniqueDynamic(
			this, &UGroundProjectionSettingsWidget::OnSliderCaptureBegin);
		DensitySlider->OnMouseCaptureEnd.AddUniqueDynamic(
			this, &UGroundProjectionSettingsWidget::OnSliderCaptureEnd);
		DensitySlider->SetValue(UDroneGroundProjectionComponent::GetGroundProjectionDensity());
		RefreshDensityReadout(DensitySlider->GetValue());
	}
	if (ClearButton)
	{
		ClearButton->OnClicked.AddUniqueDynamic(
			this, &UGroundProjectionSettingsWidget::OnClearButtonClicked);
	}
}

void UGroundProjectionSettingsWidget::NativeDestruct()
{
	if (DensitySlider)
	{
		DensitySlider->OnValueChanged.RemoveDynamic(
			this, &UGroundProjectionSettingsWidget::OnDensityChanged);
		DensitySlider->OnMouseCaptureBegin.RemoveDynamic(
			this, &UGroundProjectionSettingsWidget::OnSliderCaptureBegin);
		DensitySlider->OnMouseCaptureEnd.RemoveDynamic(
			this, &UGroundProjectionSettingsWidget::OnSliderCaptureEnd);
	}
	if (ClearButton)
	{
		ClearButton->OnClicked.RemoveDynamic(
			this, &UGroundProjectionSettingsWidget::OnClearButtonClicked);
	}
	bPointerInside = false;
	bSliderCaptured = false;
	RefreshInteractionState();
	Super::NativeDestruct();
}

void UGroundProjectionSettingsWidget::NativeOnMouseEnter(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bPointerInside = true;
	RefreshInteractionState();
}

void UGroundProjectionSettingsWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bPointerInside = false;
	RefreshInteractionState();
}

float UGroundProjectionSettingsWidget::GetDensityValue() const
{
	return DensitySlider
		? DensitySlider->GetValue()
		: UDroneGroundProjectionComponent::GetGroundProjectionDensity();
}

float UGroundProjectionSettingsWidget::GetDensityStepSize() const
{
	return DensitySlider ? DensitySlider->GetStepSize() : 0.01f;
}

FString UGroundProjectionSettingsWidget::GetProductCopySnapshot() const
{
	const FString DensityCopy = DensityValueText
		? DensityValueText->GetText().ToString()
		: FString();
	const FString StatusCopy = StatusText
		? StatusText->GetText().ToString()
		: FString();
	const FString SliderTooltip = DensitySlider
		? DensitySlider->GetToolTipText().ToString()
		: FString();
	const FString ClearTooltip = ClearButton
		? ClearButton->GetToolTipText().ToString()
		: FString();
	const TArray<FString> CopyParts = {
		DensityCopy,
		StatusCopy,
		SliderTooltip,
		ClearTooltip
	};
	return FString::Join(CopyParts, TEXT(" | "));
}

void UGroundProjectionSettingsWidget::SetDensityValue(float NormalizedDensity)
{
	const float ClampedDensity = FMath::Clamp(NormalizedDensity, 0.0f, 1.0f);
	if (DensitySlider)
	{
		DensitySlider->SetValue(ClampedDensity);
	}
	OnDensityChanged(ClampedDensity);
}

void UGroundProjectionSettingsWidget::RefreshInteractionState()
{
	bStaticPanelInteractive = bPointerInside || bSliderCaptured;
}

void UGroundProjectionSettingsWidget::RefreshDensityReadout(float NormalizedDensity)
{
	if (!DensityValueText)
	{
		return;
	}

	const int32 Percent = FMath::RoundToInt(FMath::Clamp(NormalizedDensity, 0.0f, 1.0f) * 100.0f);
	if (NormalizedDensity >= 0.9999f)
	{
		DensityValueText->SetText(FText::FromString(
			FString::Printf(TEXT("%d%% · 最高"), Percent)));
		DensityValueText->SetToolTipText(FText::FromString(
			TEXT("当前密度：100%")));
		return;
	}

	const float SpacingMeters =
		UDroneGroundProjectionComponent::DensityToRaySpacingCm(NormalizedDensity) / 100.0f;
	DensityValueText->SetText(FText::FromString(
		FString::Printf(TEXT("%d%% · %.1f m"), Percent, SpacingMeters)));
	DensityValueText->SetToolTipText(FText::FromString(FString::Printf(
		TEXT("当前密度：%d%%，间距约 %.1f 米"),
		Percent,
		SpacingMeters)));
}

void UGroundProjectionSettingsWidget::OnDensityChanged(float NormalizedDensity)
{
	const float ClampedDensity = FMath::Clamp(NormalizedDensity, 0.0f, 1.0f);
	UDroneGroundProjectionComponent::SetGroundProjectionDensity(this, ClampedDensity);
	RefreshDensityReadout(ClampedDensity);
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(TEXT("投影线将在 30 秒后自动消失")));
	}
}

void UGroundProjectionSettingsWidget::OnClearButtonClicked()
{
	ClearAllProjectionRays();
}

int32 UGroundProjectionSettingsWidget::ClearAllProjectionRays()
{
	const int32 ClearedCount = UDroneGroundProjectionComponent::ClearAllTrails(this);
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(TEXT("投影线已清除")));
	}
	return ClearedCount;
}

void UGroundProjectionSettingsWidget::OnSliderCaptureBegin()
{
	bSliderCaptured = true;
	RefreshInteractionState();
}

void UGroundProjectionSettingsWidget::OnSliderCaptureEnd()
{
	bSliderCaptured = false;
	RefreshInteractionState();
}
