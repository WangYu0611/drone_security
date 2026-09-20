#include "DefaultSegmentSpeedWidget.h"
#include "Command/CommandTheme.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "Misc/DefaultValueHelper.h"
#include "Styling/CoreStyle.h"

namespace
{
	constexpr float MinLimitedSpeedMps = 0.5f;
	constexpr float MaxLimitedSpeedMps = 15.0f;
	const FLinearColor StatusNormalColor(0.75f, 0.86f, 1.0f, 1.0f);
	const FLinearColor StatusErrorColor(1.0f, 0.48f, 0.42f, 1.0f);
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Regular", 16);
	const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle("Regular", 13);
	const FSlateFontInfo DetailFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
}

void UDefaultSegmentSpeedWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// 收起态只保留一个按钮；放在右下角操作按钮上方，避开左侧态势、右上地图工具和编辑选项。
	ToggleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ToggleButton"));
	CommandTheme::Button(ToggleButton, false, true);
	ToggleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ToggleText"));
	ToggleText->SetJustification(ETextJustify::Center);
	ToggleText->SetFont(BodyFont);
	ToggleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ToggleButton->SetContent(ToggleText);
	ToggleButton->OnClicked.AddDynamic(this, &UDefaultSegmentSpeedWidget::ToggleExpanded);
	USizeBox* ToggleSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ToggleSize"));
	ToggleSize->SetWidthOverride(64.0f);
	ToggleSize->SetHeightOverride(26.0f);
	ToggleSize->AddChild(ToggleButton);
	if (UCanvasPanelSlot* ToggleSlot = Root->AddChildToCanvas(ToggleSize))
	{
		ToggleSlot->SetAnchors(FAnchors(1.0f, 1.0f));
		ToggleSlot->SetAlignment(FVector2D(1.0f, 1.0f));
		ToggleSlot->SetPosition(FVector2D(-20.0f, -70.0f));
		ToggleSlot->SetSize(FVector2D(64.0f, 26.0f));
	}

	// Use explicit canvas dimensions. Without this, a CanvasPanelSlot can collapse a
	// code-created widget to its smallest child and turn the card into a thin column.
	ExpandedPanel = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ExpandedPanel"));
	ExpandedPanel->SetWidthOverride(240.0f);
	ExpandedPanel->SetHeightOverride(112.0f);
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(ExpandedPanel))
	{
		PanelSlot->SetAnchors(FAnchors(1.0f, 1.0f));
		PanelSlot->SetAlignment(FVector2D(1.0f, 1.0f));
		PanelSlot->SetPosition(FVector2D(-20.0f, -104.0f));
		PanelSlot->SetSize(FVector2D(240.0f, 112.0f));
	}

	UBorder* PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetPadding(FMargin(10.0f, 9.0f));
	CommandTheme::Panel(PanelBorder);
	ExpandedPanel->AddChild(PanelBorder);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Content"));
	PanelBorder->SetContent(Content);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	Title->SetText(FText::FromString(TEXT("飞行速度")));
	Title->SetFont(TitleFont);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Content->AddChildToVerticalBox(Title);

	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Hint"));
	Hint->SetText(FText::FromString(TEXT("新航点和地图点击移动")));
	Hint->SetFont(DetailFont);
	Hint->SetColorAndOpacity(FSlateColor(StatusNormalColor));
	if (UVerticalBoxSlot* HintSlot = Content->AddChildToVerticalBox(Hint))
	{
		HintSlot->SetPadding(FMargin(0.0f, 1.0f, 0.0f, 5.0f));
	}

	UHorizontalBox* InputRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InputRow"));
	Content->AddChildToVerticalBox(InputRow);
	UTextBlock* SpeedLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpeedLabel"));
	SpeedLabel->SetText(FText::FromString(TEXT("速度")));
	SpeedLabel->SetFont(BodyFont);
	SpeedLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.84f, 0.90f, 1.0f, 1.0f)));
	if (UHorizontalBoxSlot* LabelSlot = InputRow->AddChildToHorizontalBox(SpeedLabel))
	{
		LabelSlot->SetPadding(FMargin(0.0f, 3.0f, 5.0f, 0.0f));
	}

	SpeedInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("SpeedInput"));
	SpeedInput->SetHintText(FText::FromString(TEXT("0 / 0.5 - 15")));
	SpeedInput->SetText(FText::FromString(TEXT("1")));
	SpeedInput->WidgetStyle.SetFont(BodyFont);
	CommandTheme::Input(SpeedInput);
	if (UHorizontalBoxSlot* InputSlot = InputRow->AddChildToHorizontalBox(SpeedInput))
	{
		InputSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		InputSlot->SetPadding(FMargin(0.0f, 0.0f, 5.0f, 0.0f));
	}

	UTextBlock* UnitLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("UnitLabel"));
	UnitLabel->SetText(FText::FromString(TEXT("m/s")));
	UnitLabel->SetFont(BodyFont);
	UnitLabel->SetColorAndOpacity(FSlateColor(StatusNormalColor));
	if (UHorizontalBoxSlot* UnitSlot = InputRow->AddChildToHorizontalBox(UnitLabel))
	{
		UnitSlot->SetPadding(FMargin(0.0f, 3.0f, 5.0f, 0.0f));
	}

	ApplyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ApplyButton"));
	CommandTheme::Button(ApplyButton, false, true);
	UTextBlock* ApplyLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ApplyLabel"));
	ApplyLabel->SetText(FText::FromString(TEXT("应用")));
	ApplyLabel->SetJustification(ETextJustify::Center);
	ApplyLabel->SetFont(BodyFont);
	ApplyLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ApplyButton->SetContent(ApplyLabel);
	USizeBox* ApplyButtonSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ApplyButtonSize"));
	ApplyButtonSize->SetWidthOverride(50.0f);
	ApplyButtonSize->SetHeightOverride(25.0f);
	ApplyButtonSize->AddChild(ApplyButton);
	InputRow->AddChildToHorizontalBox(ApplyButtonSize);
	ApplyButton->OnClicked.AddDynamic(this, &UDefaultSegmentSpeedWidget::ApplySpeed);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetFont(DetailFont);
	if (UVerticalBoxSlot* StatusSlot = Content->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 0.0f));
	}
	SetStatus(FText::FromString(TEXT("当前 1 m/s · 0 不限速")), StatusNormalColor);
	SetExpanded(false);
}

void UDefaultSegmentSpeedWidget::ShowOverlay()
{
	// AddToViewport anchors the root to the whole screen (FGameViewportWidgetSlot
	// defaults to anchors 0,0,1,1). Plain Visible would make that screen-wide root
	// self-hit-testable and swallow every click aimed at the panels below it — the
	// path-edit toggle included, which locks the user inside edit mode. Keep the root
	// transparent to hit tests; the card and toggle button are Visible in their own
	// right and stay clickable.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UDefaultSegmentSpeedWidget::SetPathEditController(ADroneOpsPlayerController* InController)
{
	PathEditController = InController;
}

void UDefaultSegmentSpeedWidget::SetDisplayedSpeed(float SpeedMps)
{
	if (!SpeedInput)
	{
		return;
	}

	SpeedInput->SetText(FText::FromString(FString::SanitizeFloat(SpeedMps)));
	const FString Display = FMath::IsNearlyZero(SpeedMps) ? TEXT("不限速") : FString::SanitizeFloat(SpeedMps) + TEXT(" m/s");
	SetStatus(FText::FromString(FString::Printf(TEXT("当前 %s · 0 不限速"), *Display)), StatusNormalColor);
}

void UDefaultSegmentSpeedWidget::ApplySpeed()
{
	if (!PathEditController || !SpeedInput)
	{
		SetStatus(FText::FromString(TEXT("速度控制器不可用")), StatusErrorColor);
		return;
	}

	float RequestedSpeed = 0.0f;
	const FString Input = SpeedInput->GetText().ToString().TrimStartAndEnd();
	if (Input.IsEmpty() || !FDefaultValueHelper::ParseFloat(Input, RequestedSpeed) || !FMath::IsFinite(RequestedSpeed))
	{
		SetStatus(FText::FromString(TEXT("请输入数字：0 或 0.5 - 15")), StatusErrorColor);
		return;
	}

	if (!FMath::IsNearlyZero(RequestedSpeed) && (RequestedSpeed < MinLimitedSpeedMps || RequestedSpeed > MaxLimitedSpeedMps))
	{
		SetStatus(FText::FromString(TEXT("限速值必须是 0.5 - 15 m/s；0 表示不限速")), StatusErrorColor);
		return;
	}

	PathEditController->SetEditDefaultSegmentSpeed(RequestedSpeed);
	const FString Display = FMath::IsNearlyZero(RequestedSpeed) ? TEXT("不限速") : FString::SanitizeFloat(RequestedSpeed) + TEXT(" m/s");
	SetStatus(FText::FromString(FString::Printf(TEXT("已应用 %s · 新航点和点击移动生效"), *Display)), StatusNormalColor);
}

void UDefaultSegmentSpeedWidget::ToggleExpanded()
{
	SetExpanded(!ExpandedPanel || ExpandedPanel->GetVisibility() != ESlateVisibility::Visible);
}

void UDefaultSegmentSpeedWidget::SetStatus(const FText& Message, const FLinearColor& Color)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
		StatusText->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UDefaultSegmentSpeedWidget::SetExpanded(bool bShouldExpand)
{
	if (ExpandedPanel)
	{
		ExpandedPanel->SetVisibility(bShouldExpand ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ToggleText)
	{
		ToggleText->SetText(FText::FromString(bShouldExpand ? TEXT("速度  ▴") : TEXT("速度  ▾")));
	}
}
