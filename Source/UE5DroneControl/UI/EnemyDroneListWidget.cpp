// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyDroneListWidget.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Core/HostileTargetManager.h"
#include "DroneOps/Core/HostileTargetActor.h"
#include "MultiDroneCharacter.h"
#include "EngineUtils.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "UI/DroneNameEditPopupWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

// ===========================================================================
//  UEnemyDroneListWidget
// ===========================================================================

void UEnemyDroneListWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    // 纯 C++ UUserWidget 在 NativeOnInitialized 时 WidgetTree 可能为 null。
    // 强制创建一个，保证 BuildRuntimeWidgetTree 能正常执行。
    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
    }
    BuildRuntimeWidgetTree();
}

void UEnemyDroneListWidget::EnsureWidgetTreeBuilt()
{
    // 若 NativeOnInitialized 里 WidgetTree 为 null 导致构建未完成，则在此补建。
    // NativeConstruct 时 WidgetTree 对纯 C++ Widget 也保证非 null。
    if (!EnemyScrollBox)
    {
        BuildRuntimeWidgetTree();
    }
}

void UEnemyDroneListWidget::BuildRuntimeWidgetTree()
{
    if (!WidgetTree)
    {
        return;
    }

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EnemyListRoot"));
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    WidgetTree->RootWidget = Root;

    EnemyPanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EnemyListPanel"));
    // 深红色背景，与友方面板的深蓝色区分
    EnemyPanelBorder->SetBrushColor(FLinearColor(0.09f, 0.025f, 0.025f, 0.97f));
    EnemyPanelBorder->SetPadding(FMargin(16.0f, 14.0f));

    // 先用默认定位占位；NativeConstruct 里 GetWorld() 可靠后再按地图名修正
    EnemyPanelSlot = Root->AddChildToCanvas(EnemyPanelBorder);
    if (EnemyPanelSlot)
    {
        EnemyPanelSlot->SetAnchors(FAnchors(0.0f, 0.0f));
        EnemyPanelSlot->SetAlignment(FVector2D::ZeroVector);
        EnemyPanelSlot->SetPosition(FVector2D(514.0f, 112.0f));
        EnemyPanelSlot->SetSize(FVector2D(350.0f, 400.0f));
    }

    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EnemyListContent"));
    EnemyPanelBorder->SetContent(Content);

    auto MakeText = [this](const FName Name, const FString& Value, const FLinearColor& Color)
    {
        UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        T->SetText(FText::FromString(Value));
        T->SetColorAndOpacity(Color);
        return T;
    };
    auto Add = [](UVerticalBox* Box, UWidget* Child, const FMargin& Pad = FMargin(0.0f, 3.0f))
    {
        if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Child)) S->SetPadding(Pad);
    };

    Add(Content, MakeText(TEXT("TitleText"), TEXT("敌对目标"), FLinearColor(1.0f, 0.72f, 0.72f, 1.0f)), FMargin(0.0f, 0.0f, 0.0f, 2.0f));
    Add(Content, MakeText(TEXT("SubtitleText"), TEXT("本地构造 · 不发往后端"), FLinearColor(0.75f, 0.45f, 0.45f, 1.0f)), FMargin(0.0f, 0.0f, 0.0f, 8.0f));

    EnemyScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("EnemyScrollBox"));
    EnemyScrollBox->SetScrollBarVisibility(ESlateVisibility::Visible);
    if (UVerticalBoxSlot* ScrollSlot = Content->AddChildToVerticalBox(EnemyScrollBox))
    {
        ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }
}

void UEnemyDroneListWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // NativeConstruct 时 GetWorld() 可靠，按地图名修正面板定位
    if (EnemyPanelSlot)
    {
        const FString MapName = GetWorld() ? GetWorld()->GetMapName() : FString();
        const bool bIsCesiumWorld = MapName.Contains(TEXT("CesiumWorld"));
        EnemyPanelSlot->SetAnchors(bIsCesiumWorld ? FAnchors(0.0f, 1.0f) : FAnchors(0.0f, 0.0f));
        EnemyPanelSlot->SetAlignment(bIsCesiumWorld ? FVector2D(0.0f, 1.0f) : FVector2D::ZeroVector);
        EnemyPanelSlot->SetPosition(bIsCesiumWorld ? FVector2D(514.0f, -96.0f) : FVector2D(514.0f, 112.0f));
    }

    // 默认收起，由左下角切换按钮控制显示
    if (EnemyPanelBorder)
    {
        EnemyPanelBorder->SetVisibility(ESlateVisibility::Collapsed);
    }

    // 订阅注册委托，确保敌对无人机 BeginPlay 晚于 Widget 创建时也能及时刷新
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UDroneRegistrySubsystem* Registry = GI->GetSubsystem<UDroneRegistrySubsystem>())
        {
            Registry->OnDroneRegistered.RemoveDynamic(this, &UEnemyDroneListWidget::OnDroneRegisteredHandler);
            Registry->OnDroneRegistered.AddDynamic(this, &UEnemyDroneListWidget::OnDroneRegisteredHandler);
        }
    }

    RefreshFromRegistry();

    if (GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(RefreshTimerHandle) == false)
    {
        GetWorld()->GetTimerManager().SetTimer(RefreshTimerHandle, this, &UEnemyDroneListWidget::OnRefreshTimer, 3.0f, true);
    }
}

void UEnemyDroneListWidget::NativeDestruct()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UDroneRegistrySubsystem* Registry = GI->GetSubsystem<UDroneRegistrySubsystem>())
        {
            Registry->OnDroneRegistered.RemoveDynamic(this, &UEnemyDroneListWidget::OnDroneRegisteredHandler);
        }
    }

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(RefreshTimerHandle);
    }
    Super::NativeDestruct();
}

void UEnemyDroneListWidget::OnRefreshTimer()
{
    RefreshFromRegistry();
}

void UEnemyDroneListWidget::SetPanelVisible(bool bVisible)
{
    if (EnemyPanelBorder)
    {
        EnemyPanelBorder->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UEnemyDroneListWidget::OnDroneRegisteredHandler(int32 DroneId)
{
    RefreshFromRegistry();
}

void UEnemyDroneListWidget::RefreshFromRegistry()
{
    if (!EnemyScrollBox)
    {
        return;
    }

    UGameInstance* GI = GetGameInstance();
    UDroneRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UDroneRegistrySubsystem>() : nullptr;
    if (!Registry)
    {
        return;
    }

    EnemyScrollBox->ClearChildren();

    TArray<FDroneDescriptor> Descriptors = Registry->GetAllDroneDescriptors();
    Descriptors.Sort([](const FDroneDescriptor& A, const FDroneDescriptor& B)
    {
        return A.DroneId < B.DroneId;
    });

    for (const FDroneDescriptor& Desc : Descriptors)
    {
        if (!Desc.bIsEnemyTarget)
        {
            continue;
        }

        UEnemyDroneRowWidget* Row = CreateWidget<UEnemyDroneRowWidget>(GetWorld(), UEnemyDroneRowWidget::StaticClass());
        if (Row)
        {
            Row->InitRow(Desc.DroneId, Desc.Name);
            EnemyScrollBox->AddChild(Row);
        }
    }
}

void UEnemyDroneListWidget::AddEnemyRow(int32 DroneId, const FString& Name)
{
    // 保留供外部直接调用（非定时刷新路径）
    if (!EnemyScrollBox)
    {
        return;
    }
    UEnemyDroneRowWidget* Row = CreateWidget<UEnemyDroneRowWidget>(GetWorld(), UEnemyDroneRowWidget::StaticClass());
    if (Row)
    {
        Row->InitRow(DroneId, Name);
        EnemyScrollBox->AddChild(Row);
    }
}

void UEnemyDroneListWidget::OnDeleteClicked() {}
void UEnemyDroneListWidget::OnSelectClicked() {}

// ===========================================================================
//  UEnemyDroneRowWidget
// ===========================================================================

void UEnemyDroneRowWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildRow();
}

void UEnemyDroneRowWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (NameLabelButton)
    {
        NameLabelButton->OnClicked.AddDynamic(this, &UEnemyDroneRowWidget::OnNameLabelButtonClicked);
    }
    if (SelectButton)
    {
        SelectButton->OnClicked.AddDynamic(this, &UEnemyDroneRowWidget::OnSelectClicked);
    }
    if (DeleteButton)
    {
        DeleteButton->OnClicked.AddDynamic(this, &UEnemyDroneRowWidget::OnDeleteClicked);
    }

    UpdateSelectState();
}

void UEnemyDroneRowWidget::NativeDestruct()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UDroneRegistrySubsystem* Registry = GI->GetSubsystem<UDroneRegistrySubsystem>())
        {
            Registry->OnMultiSelectionChanged.RemoveDynamic(this, &UEnemyDroneRowWidget::UpdateSelectState);
        }
    }
    Super::NativeDestruct();
}

void UEnemyDroneRowWidget::BuildRow()
{
    if (!WidgetTree)
    {
        return;
    }

    // 行容器：深红底色
    UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EnemyRowCard"));
    Card->SetBrushColor(FLinearColor(0.12f, 0.04f, 0.04f, 0.96f));
    Card->SetPadding(FMargin(10.0f, 8.0f));
    WidgetTree->RootWidget = Card;

    // 垂直布局：名称在上，按钮行在下，整体居中
    UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EnemyRowVBox"));
    Card->SetContent(VBox);

    // 名称：透明按钮包裹 TextBlock，点击弹出编辑弹窗
    NameLabelButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("EnemyNameLabelButton"));
    {
        FButtonStyle Style = NameLabelButton->GetStyle();
        Style.Normal.DrawAs  = ESlateBrushDrawType::NoDrawType;
        Style.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
        Style.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
        NameLabelButton->SetStyle(Style);
    }
    NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EnemyNameText"));
    NameText->SetText(FText::FromString(TEXT("敌方目标")));
    NameText->SetColorAndOpacity(FLinearColor(1.0f, 0.78f, 0.78f, 1.0f));
    NameText->SetJustification(ETextJustify::Center);
    NameLabelButton->AddChild(NameText);
    if (UVerticalBoxSlot* S = VBox->AddChildToVerticalBox(NameLabelButton))
    {
        S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
        S->SetHorizontalAlignment(HAlign_Center);
    }

    // 按钮行（选中 + 删除），居中
    UHorizontalBox* BtnRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EnemyRowBtnHBox"));
    if (UVerticalBoxSlot* S = VBox->AddChildToVerticalBox(BtnRow))
    {
        S->SetPadding(FMargin(0.0f));
        S->SetHorizontalAlignment(HAlign_Center);
    }

    // 选中按钮
    SelectButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EnemySelectBtnText"));
    SelectButtonText->SetText(FText::FromString(TEXT("选中")));
    SelectButtonText->SetColorAndOpacity(FLinearColor(0.90f, 0.95f, 1.0f, 1.0f));
    SelectButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("EnemySelectButton"));
    SelectButton->AddChild(SelectButtonText);
    if (UHorizontalBoxSlot* S = BtnRow->AddChildToHorizontalBox(SelectButton))
    {
        S->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
        S->SetVerticalAlignment(VAlign_Center);
    }

    // 删除按钮（红色文字）
    UTextBlock* DeleteLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EnemyDeleteBtnText"));
    DeleteLabel->SetText(FText::FromString(TEXT("删除")));
    DeleteLabel->SetColorAndOpacity(FLinearColor(1.0f, 0.35f, 0.35f, 1.0f));
    DeleteButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("EnemyDeleteButton"));
    DeleteButton->AddChild(DeleteLabel);
    if (UHorizontalBoxSlot* S = BtnRow->AddChildToHorizontalBox(DeleteButton))
    {
        S->SetPadding(FMargin(0.0f));
        S->SetVerticalAlignment(VAlign_Center);
    }
}

void UEnemyDroneRowWidget::InitRow(int32 InDroneId, const FString& InName)
{
    DroneId = InDroneId;
    if (NameText)
    {
        NameText->SetText(FText::FromString(InName));
    }

    // 订阅多选变更委托
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UDroneRegistrySubsystem* Registry = GI->GetSubsystem<UDroneRegistrySubsystem>())
        {
            Registry->OnMultiSelectionChanged.RemoveDynamic(this, &UEnemyDroneRowWidget::UpdateSelectState);
            Registry->OnMultiSelectionChanged.AddDynamic(this, &UEnemyDroneRowWidget::UpdateSelectState);
        }
    }
    UpdateSelectState();
}

void UEnemyDroneRowWidget::RefreshSelectButton()
{
    UpdateSelectState();
}

void UEnemyDroneRowWidget::UpdateSelectState()
{
    if (!SelectButton || !SelectButtonText || DroneId <= 0)
    {
        return;
    }

    UDroneRegistrySubsystem* Registry = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()
        : nullptr;
    if (!Registry)
    {
        return;
    }

    const bool bSelected = Registry->IsDroneSelected(DroneId);
    SelectButtonText->SetText(FText::FromString(bSelected ? TEXT("取消选中") : TEXT("选中")));
}

void UEnemyDroneRowWidget::OnSelectClicked()
{
    if (DroneId <= 0)
    {
        return;
    }

    UDroneRegistrySubsystem* Registry = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()
        : nullptr;
    if (!Registry)
    {
        return;
    }

    if (Registry->IsDroneSelected(DroneId))
    {
        Registry->RemoveFromMultiSelection(DroneId);
    }
    else
    {
        Registry->AddToMultiSelection(DroneId);
    }
}

void UEnemyDroneRowWidget::OnDeleteClicked()
{
    if (DroneId <= 0)
    {
        return;
    }

    UDroneRegistrySubsystem* Registry = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>()
        : nullptr;
    if (!Registry)
    {
        return;
    }

    // 销毁地图上的 AHostileTargetActor（EndPlay 会自动从 HostileTargetManager 注销）
    if (UWorld* World = GetWorld())
    {
        if (UHostileTargetManager* Manager = World->GetSubsystem<UHostileTargetManager>())
        {
            if (AHostileTargetActor* TargetActor = Manager->GetTarget(DroneId))
            {
                TargetActor->Destroy();
            }
        }

        // 销毁对应的影子机 AMultiDroneCharacter
        for (TActorIterator<AMultiDroneCharacter> It(World); It; ++It)
        {
            if (IsValid(*It) && (*It)->DroneId == DroneId)
            {
                (*It)->Destroy();
                break;
            }
        }
    }

    // 注销敌对目标（从注册表和持久化中移除）
    Registry->UnregisterDrone(DroneId);

    // 从 ScrollBox 中移除自身
    if (UPanelWidget* Parent = GetParent())
    {
        Parent->RemoveChild(this);
    }
}

void UEnemyDroneRowWidget::OnNameLabelButtonClicked()
{
    if (DroneId <= 0)
    {
        return;
    }

    UGameInstance* GI = GetGameInstance();
    UDroneRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UDroneRegistrySubsystem>() : nullptr;
    if (!Registry)
    {
        return;
    }

    FDroneLabelSettings CurrentSettings;
    Registry->GetDroneLabelSettings(DroneId, CurrentSettings);

    UDroneNameEditPopupWidget* Popup = CreateWidget<UDroneNameEditPopupWidget>(
        GetWorld(), UDroneNameEditPopupWidget::StaticClass());
    if (!Popup)
    {
        return;
    }

    Popup->InitPopup(DroneId, CurrentSettings.DisplayName, CurrentSettings.LabelColor, CurrentSettings.FontSize);
    Popup->OnConfirmed.AddDynamic(this, &UEnemyDroneRowWidget::OnLabelEditConfirmed);
    Popup->AddToViewport(100);
}

void UEnemyDroneRowWidget::OnLabelEditConfirmed(int32 InDroneId, const FString& NewName,
    FLinearColor NewColor, int32 NewFontSize)
{
    UGameInstance* GI = GetGameInstance();
    UDroneRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UDroneRegistrySubsystem>() : nullptr;
    if (!Registry)
    {
        return;
    }

    FDroneLabelSettings Settings;
    Settings.DisplayName = NewName;
    Settings.LabelColor  = NewColor;
    Settings.FontSize    = NewFontSize;
    Registry->SetDroneLabelSettings(InDroneId, Settings);

    if (NameText)
    {
        NameText->SetText(FText::FromString(NewName));
        NameText->SetColorAndOpacity(NewColor);
    }
}
