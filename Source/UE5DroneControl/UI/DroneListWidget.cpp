#include "DroneListWidget.h"
#include "Shared/ProductText.h"
// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyDroneListWidget.h"
#include "UIManagerBlueprintLibrary.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Network/DroneNetworkManager.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UDroneListWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildRuntimeWidgetTree();
}

void UDroneListWidget::BuildRuntimeWidgetTree()
{
    if (!WidgetTree)
    {
        return;
    }

    // Replace the legacy WBP tree: it mixed absolute-positioned labels with C++ appended
    // controls, which is why the screenshot showed every row on top of each other.
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DroneListRoot"));
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    WidgetTree->RootWidget = Root;

    DronePanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DroneListPanel"));
    UBorder* Panel = DronePanelBorder;
    Panel->SetBrushColor(FLinearColor(0.025f, 0.045f, 0.075f, 0.97f));
    Panel->SetPadding(FMargin(16.0f, 14.0f));
    const FString MapName = GetWorld() ? GetWorld()->GetMapName() : FString();
    const bool bIsMainMenu = MapName.Contains(TEXT("MainMenu"));
    const bool bIsCesiumWorld = MapName.Contains(TEXT("CesiumWorld"));
    if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
    {
        DronePanelSlot = PanelSlot;
        PanelSlot->SetAnchors(bIsMainMenu ? FAnchors(1.0f, 0.0f)
            : (bIsCesiumWorld ? FAnchors(0.0f, 1.0f) : FAnchors(0.0f, 0.0f)));
        PanelSlot->SetAlignment(bIsMainMenu ? FVector2D(1.0f, 0.0f)
            : (bIsCesiumWorld ? FVector2D(0.0f, 1.0f) : FVector2D::ZeroVector));
        PanelSlot->SetPosition(bIsMainMenu ? FVector2D(-32.0f, 112.0f)
            : (bIsCesiumWorld ? FVector2D(32.0f, -96.0f) : FVector2D(32.0f, 112.0f)));
        PanelSlot->SetSize(FVector2D(450.0f, 620.0f));
    }

    // ---- 左下角切换按钮栏 ----
    if (!bIsMainMenu)
    {
        auto MakeToggleButton = [this](const FName Name, const FString& Label) -> UButton*
        {
            UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
            {
                FButtonStyle Style = Btn->GetStyle();
                FSlateBrush NormalBrush;
                NormalBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
                NormalBrush.TintColor = FSlateColor(FLinearColor(0.06f, 0.12f, 0.20f, 0.95f));
                NormalBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
                NormalBrush.OutlineSettings.CornerRadii = FVector4(4.f, 4.f, 4.f, 4.f);
                Style.Normal = NormalBrush;
                FSlateBrush HovBrush = NormalBrush;
                HovBrush.TintColor = FSlateColor(FLinearColor(0.10f, 0.22f, 0.38f, 0.95f));
                Style.Hovered = HovBrush;
                Style.NormalPadding = FMargin(14.0f, 8.0f);
                Style.PressedPadding = FMargin(14.0f, 8.0f);
                Btn->SetStyle(Style);
            }
            UTextBlock* BtnText = WidgetTree->ConstructWidget<UTextBlock>(
                UTextBlock::StaticClass(), FName(*(Name.ToString() + TEXT("_Label"))));
            BtnText->SetText(FText::FromString(Label));
            BtnText->SetColorAndOpacity(FLinearColor(0.90f, 0.95f, 1.0f, 1.0f));
            Btn->AddChild(BtnText);
            return Btn;
        };

        UHorizontalBox* BtnBar = WidgetTree->ConstructWidget<UHorizontalBox>(
            UHorizontalBox::StaticClass(), TEXT("PanelToggleBar"));

        ToggleDroneListButton = MakeToggleButton(TEXT("DroneListToggleBtn"), TEXT("无人机态势"));
        if (UHorizontalBoxSlot* S = BtnBar->AddChildToHorizontalBox(ToggleDroneListButton))
            S->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

        ToggleEnemyListButton = MakeToggleButton(TEXT("EnemyListToggleBtn"), TEXT("敌对目标"));
        BtnBar->AddChildToHorizontalBox(ToggleEnemyListButton);

        if (UCanvasPanelSlot* BtnSlot = Root->AddChildToCanvas(BtnBar))
        {
            BtnSlot->SetAnchors(FAnchors(0.0f, 1.0f));
            BtnSlot->SetAlignment(FVector2D(0.0f, 1.0f));
            BtnSlot->SetPosition(FVector2D(32.0f, -32.0f));
            BtnSlot->SetAutoSize(true);
        }
    }

    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DroneListContent"));
    Panel->SetContent(Content);
    auto MakeText = [this](const FName Name, const FString& Value, const FLinearColor& Color)
    {
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        Text->SetText(ProductText::Source(Value));
        Text->SetColorAndOpacity(Color);
        return Text;
    };
    auto Add = [](UVerticalBox* Box, UWidget* Child, const FMargin& ChildPadding = FMargin(0.0f, 3.0f))
    {
        if (UVerticalBoxSlot* ChildSlot = Box->AddChildToVerticalBox(Child)) ChildSlot->SetPadding(ChildPadding);
    };

    Add(Content, MakeText(TEXT("TitleText"), TEXT("无人机态势"), FLinearColor(0.94f, 0.98f, 1.0f, 1.0f)), FMargin(0.0f, 0.0f, 0.0f, 2.0f));
    Add(Content, MakeText(TEXT("SubtitleText"), TEXT("已注册无人机 · 实时状态同步"), FLinearColor(0.45f, 0.67f, 0.86f, 1.0f)), FMargin(0.0f, 0.0f, 0.0f, 6.0f));

    // ---- Refresh 按钮区（垂直排列：按钮在上，状态文字在下，文字自动换行避免溢出） ----
    UVerticalBox* RefreshSection = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("RefreshSection"));
    RefreshButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RefreshButton"));
    UTextBlock* RefreshLabel = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), TEXT("RefreshLabel"));
    RefreshLabel->SetText(FText::FromString(TEXT("刷新连接")));
    RefreshLabel->SetColorAndOpacity(FLinearColor(0.90f, 0.95f, 1.0f, 1.0f));
    RefreshButton->AddChild(RefreshLabel);
    if (UVerticalBoxSlot* BtnSlot = RefreshSection->AddChildToVerticalBox(RefreshButton))
    {
        BtnSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
        BtnSlot->SetHorizontalAlignment(HAlign_Left);
    }
    RefreshStatusText = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), TEXT("RefreshStatusText"));
    RefreshStatusText->SetColorAndOpacity(FLinearColor(0.55f, 0.75f, 0.95f, 1.0f));
    RefreshStatusText->SetAutoWrapText(true);
    if (UVerticalBoxSlot* StatusSlot = RefreshSection->AddChildToVerticalBox(RefreshStatusText))
    {
        StatusSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
        StatusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    }

    VideoKeyboardControlButton = WidgetTree->ConstructWidget<UButton>(
        UButton::StaticClass(), TEXT("VideoKeyboardControlButton"));
    VideoKeyboardControlLabel = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), TEXT("VideoKeyboardControlLabel"));
    VideoKeyboardControlLabel->SetText(FText::FromString(TEXT("视频键盘操控")));
    VideoKeyboardControlLabel->SetColorAndOpacity(FLinearColor(0.90f, 0.95f, 1.0f, 1.0f));
    VideoKeyboardControlButton->AddChild(VideoKeyboardControlLabel);
    if (UVerticalBoxSlot* VideoControlSlot = RefreshSection->AddChildToVerticalBox(VideoKeyboardControlButton))
    {
        VideoControlSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
        VideoControlSlot->SetHorizontalAlignment(HAlign_Left);
    }
    Add(Content, RefreshSection, FMargin(0.0f, 0.0f, 0.0f, 8.0f));

    IsolationStatusText = MakeText(
        TEXT("IsolationStatusText"),
        TEXT("纯本地预演：后端刷新已禁用"),
        FLinearColor(1.0f, 0.16f, 0.12f, 1.0f));
    IsolationStatusText->SetVisibility(ESlateVisibility::Collapsed);
    Add(Content, IsolationStatusText, FMargin(0.0f, 0.0f, 0.0f, 10.0f));

    DroneScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("DroneScrollBox"));
    DroneScrollBox->SetScrollBarVisibility(ESlateVisibility::Visible);
    if (UVerticalBoxSlot* ScrollSlot = Content->AddChildToVerticalBox(DroneScrollBox))
    {
        ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    // Force the code-owned card class so an old WBP_DroneListItem cannot reintroduce overlap.
    ListItemClass = UDroneListItemWidget::StaticClass();
}

void UDroneListWidget::AddDroneItem(const FString& Name, bool bOnline)
{
    if (!ListItemClass || !DroneScrollBox)
    {
        UE_LOG(LogTemp, Warning, TEXT("DroneListWidget: ListItemClass or ScrollBox not set!"));
        return;
    }

    UDroneListItemWidget* Item = CreateWidget<UDroneListItemWidget>(GetWorld(), ListItemClass);
    if (Item)
    {
        Item->SetDroneData(Name, bOnline);
        DroneScrollBox->AddChild(Item);
    }
}

void UDroneListWidget::ClearList()
{
    if (DroneScrollBox)
    {
        DroneScrollBox->ClearChildren();
    }
    // 清空查找表，避免指向已销毁列表项的悬空引用
    ItemsByDroneId.Empty();
}

void UDroneListWidget::AddDemoDrones()
{
    ClearList();
    AddDroneItem(TEXT("无人机01"), true);
    AddDroneItem(TEXT("无人机02"), false);
    AddDroneItem(TEXT("无人机03"), true);
    UE_LOG(LogTemp, Log, TEXT("DroneListWidget: Added 3 demo drones"));
}

void UDroneListWidget::RefreshFromRegistry()
{
    UGameInstance* GI = GetGameInstance();
    UDroneRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UDroneRegistrySubsystem>() : nullptr;
    if (!Registry)
    {
        return;
    }

    // 销毁前保存每个条目的折叠状态，刷新后按 DroneId 还原
    if (DroneScrollBox)
    {
        for (UWidget* Child : DroneScrollBox->GetAllChildren())
        {
            if (UDroneListItemWidget* Item = Cast<UDroneListItemWidget>(Child))
            {
                CollapseStates.Add(Item->GetDroneId(), Item->IsCollapsed());
            }
        }
    }

    ClearList();

    TArray<FDroneRegistrationViewData> ViewData;
    TArray<FDroneDescriptor> Descriptors = Registry->GetAllDroneDescriptors();
    Descriptors.Sort([](const FDroneDescriptor& A, const FDroneDescriptor& B)
    {
        const int32 AOrder = A.Slot > 0 ? A.Slot : A.DroneId;
        const int32 BOrder = B.Slot > 0 ? B.Slot : B.DroneId;
        return AOrder < BOrder;
    });

    for (const FDroneDescriptor& Desc : Descriptors)
    {
        // 态势面板只显示友方无人机，敌对目标由 EnemyDroneListWidget 单独呈现
        if (Desc.bIsEnemyTarget)
        {
            continue;
        }

        FDroneTelemetrySnapshot Snap;
		if (!Registry->GetTelemetry(Desc.DroneId, Snap))
        {
            // 无快照时构造默认值，避免UI空指针
            Snap.DroneId = Desc.DroneId;
            Snap.Availability = EDroneAvailability::Offline;
            Snap.BatteryPercent = -1;
            Snap.TaskState = EDroneTaskState::Standby;
			Snap.LastUpdateTime = FPlatformTime::Seconds();
		}

		// 展示友方无人机；连接状态由单项控件独立显示为在线、离线或失联。

        if (ListItemClass && DroneScrollBox)
        {
            UDroneListItemWidget* Item = CreateWidget<UDroneListItemWidget>(GetWorld(), ListItemClass);
            if (Item)
            {
                // SetDroneFullState 覆盖：连接状态、GPS、海拔、电量、任务状态、航点、更新时间
                Item->SetDroneFullState(Desc.DroneId, Snap);
                if (bCommandLayout) Item->ConfigureCommandSelection();
                // 名称 & 颜色：优先读用户已保存的标签设置，避免刷新后颜色恢复默认
                FDroneLabelSettings LabelSettings;
                if (Registry->GetDroneLabelSettings(Desc.DroneId, LabelSettings))
                {
                    Item->ApplyLabelSettings(LabelSettings);
                }
                else if (Item->DroneNameText)
                {
                    Item->DroneNameText->SetText(FText::FromString(Desc.Name));
                }
                // 单独更新模式显示，不覆盖连接状态等字段
				Item->SetCommandMode(Desc.DroneId, Snap.TaskMode);
                // 绑定 DroneId 并订阅多选变更委托，使选中按钮生效
                Item->SetDroneId(Desc.DroneId);
                // 还原折叠状态（跨刷新持久化）
                if (const bool* bSaved = CollapseStates.Find(Desc.DroneId))
                {
                    Item->SetCollapsed(*bSaved);
                }
                DroneScrollBox->AddChild(Item);
                // 记录 DroneId -> 项，供事件驱动的就地更新查找
                ItemsByDroneId.Add(Desc.DroneId, Item);
            }
        }

        // ---- 构造视图数据（供蓝图事件使用） ----
        FDroneRegistrationViewData Data;
        Data.Id = Desc.DroneId;
        Data.IdStr = Desc.BackendIdString;
        Data.Name = Desc.BackendIdString;
        Data.Battery = Snap.BatteryPercent;
        Data.WorldLocation = Snap.WorldLocation;
        switch (Snap.Availability)
        {
        case EDroneAvailability::Online:
            Data.Status = TEXT("online");
            break;
        case EDroneAvailability::Lost:
            Data.Status = TEXT("lost");
            break;
        case EDroneAvailability::Offline:
        default:
            Data.Status = TEXT("offline");
            break;
        }
        ViewData.Add(Data);
    }

    OnDroneDataReceived(ViewData);
}

void UDroneListWidget::ConfigureCommandLayout()
{
    bCommandLayout = true;
    for (const TCHAR* Name : {TEXT("TitleText"), TEXT("SubtitleText")})
        if (UTextBlock* Label = Cast<UTextBlock>(WidgetTree->FindWidget(FName(Name))))
            Label->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 13));
    if (DronePanelSlot)
    {
        DronePanelSlot->SetAnchors(FAnchors(0, 0, 1, 1));
        DronePanelSlot->SetAlignment(FVector2D::ZeroVector);
        DronePanelSlot->SetOffsets(FMargin(0));
    }
    if (DronePanelBorder) DronePanelBorder->SetPadding(FMargin(4));
    if (ToggleDroneListButton) ToggleDroneListButton->SetVisibility(ESlateVisibility::Collapsed);
    if (ToggleEnemyListButton) ToggleEnemyListButton->SetVisibility(ESlateVisibility::Collapsed);
    // Command list is a state projection. Keep legacy refresh/video controls outside this layout.
    if (RefreshButton) RefreshButton->SetVisibility(ESlateVisibility::Collapsed);
    if (RefreshStatusText) RefreshStatusText->SetVisibility(ESlateVisibility::Collapsed);
    if (VideoKeyboardControlButton) VideoKeyboardControlButton->SetVisibility(ESlateVisibility::Collapsed);
    if (IsolationStatusText)
    {
        IsolationStatusText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 12));
        IsolationStatusText->SetAutoWrapText(true);
    }
}

void UDroneListWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // ---- 读取严格本地预演模式，并订阅后续变化 ----
    if (UGameInstance* GI = GetGameInstance())
    {
        CachedNetworkManager = GI->GetSubsystem<UDroneNetworkManager>();
        if (CachedNetworkManager)
        {
            // 修复：读运行时隔离状态，而非 config 字段 bStrictLocalPreview
            bStrictLocalPreview = CachedNetworkManager->IsStrictLocalPreviewIsolation();
            // 修复：订阅委托，Toggle 切换后实时同步按钮状态
            CachedNetworkManager->OnIsolationStateChanged.AddUniqueDynamic(
                this, &UDroneListWidget::OnIsolationStateChanged);
        }
    }

    // ---- 绑定 Refresh 按钮 ----
    if (RefreshButton)
    {
        // 始终绑定点击回调，由回调内部的 bStrictLocalPreview 守门；
        // 否则若 Widget 构造时已处于隔离模式，Toggle 关闭后按钮永远无响应。
        RefreshButton->OnClicked.AddDynamic(this, &UDroneListWidget::OnRefreshButtonClicked);

        if (bStrictLocalPreview)
        {
            RefreshButton->SetIsEnabled(false);
            if (RefreshStatusText)
            {
                RefreshStatusText->SetText(ProductText::Get(TEXT("Fleet.Local")));
            }
        }
    }

    if (VideoKeyboardControlButton)
    {
        VideoKeyboardControlButton->OnClicked.AddDynamic(this, &UDroneListWidget::OnVideoKeyboardControlButtonClicked);
    }

    // ---- 绑定面板切换按钮 ----
    if (ToggleDroneListButton)
    {
        ToggleDroneListButton->OnClicked.AddDynamic(this, &UDroneListWidget::OnToggleDroneListClicked);
    }
    if (ToggleEnemyListButton)
    {
        ToggleEnemyListButton->OnClicked.AddDynamic(this, &UDroneListWidget::OnToggleEnemyListClicked);
    }

    // ---- 进入时两个面板默认收起 ----
    bDronePanelVisible = bCommandLayout;
    bEnemyPanelVisible = false;
    if (DronePanelBorder)
    {
        DronePanelBorder->SetVisibility(bCommandLayout ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    if (IsolationStatusText)
    {
        IsolationStatusText->SetVisibility(
            bStrictLocalPreview ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }

    RefreshFromRegistry();

    // 启动刷新定时器作为“兜底”：主更新走事件驱动（OnRegistryTelemetryUpdated），
    // 定时器仅低频补偿（如漏播、移除无人机）。RefreshInterval 若被蓝图实例改成 0/负值，
    // 用 5 秒下限，避免定时器根本不启动导致完全不刷新。
    const float EffectiveRefreshInterval = RefreshInterval > 0.0f ? RefreshInterval : 5.0f;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(RefreshTimerHandle, this, &UDroneListWidget::OnRefreshTimer, EffectiveRefreshInterval, true);
    }

    // 订阅注册表委托：
    // - OnTelemetryUpdated：遥测变化时就地刷新对应单项（真正的实时更新，独立于定时器）。
    // - OnDroneRegistered：新机注册时整表重建以插入新项。
    // - OnDroneLabelSettingsChanged：标签名称/颜色变化时就地同步。
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UDroneRegistrySubsystem* Registry = GI->GetSubsystem<UDroneRegistrySubsystem>())
        {
            Registry->OnTelemetryUpdated.AddDynamic(this, &UDroneListWidget::OnRegistryTelemetryUpdated);
            Registry->OnDroneRegistered.AddDynamic(this, &UDroneListWidget::OnRegistryDroneRegistered);
            Registry->OnDroneLabelSettingsChanged.AddDynamic(this, &UDroneListWidget::OnLabelSettingsChanged);
        }
    }
}

void UDroneListWidget::NativeDestruct()
{
    if (CachedNetworkManager)
    {
        CachedNetworkManager->OnIsolationStateChanged.RemoveDynamic(
            this, &UDroneListWidget::OnIsolationStateChanged);
        CachedNetworkManager = nullptr;
    }

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(RefreshTimerHandle);
    }
    // 取消隔离状态委托订阅，避免 Widget 销毁后回调野指针
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UDroneRegistrySubsystem* Registry = GI->GetSubsystem<UDroneRegistrySubsystem>())
        {
            Registry->OnDroneLabelSettingsChanged.RemoveAll(this);
            Registry->OnTelemetryUpdated.RemoveAll(this);
            Registry->OnDroneRegistered.RemoveAll(this);
        }
    }
    Super::NativeDestruct();
}

void UDroneListWidget::OnRefreshTimer()
{
    RefreshFromRegistry();
}

void UDroneListWidget::OnRegistryTelemetryUpdated(int32 InDroneId, const FDroneTelemetrySnapshot& Snapshot)
{
    // 就地刷新对应单项：这是真正的实时更新路径（0.x 秒级），独立于 5 秒兜底定时器。
    // 未知 DroneId（尚未建项）忽略，由 OnRegistryDroneRegistered / 定时器兜底补建，
    // 避免对无对应描述符的遥测反复触发整表重建。
    TObjectPtr<UDroneListItemWidget>* Found = ItemsByDroneId.Find(InDroneId);
    if (!Found || !IsValid(*Found))
    {
        return;
    }

    UDroneListItemWidget* Item = *Found;
    // 覆盖：连接状态、GPS、海拔、电量、任务状态、航点、更新时间（与全量重建保持一致）
    Item->SetDroneFullState(InDroneId, Snapshot);
    // 单独更新模式显示，不覆盖连接状态等字段
    Item->SetCommandMode(InDroneId, Snapshot.TaskMode);
}

void UDroneListWidget::OnRegistryDroneRegistered(int32 InDroneId)
{
    // 新机注册是低频事件：整表重建以插入新项并重建 ItemsByDroneId 映射。
    RefreshFromRegistry();
}

void UDroneListWidget::OnIsolationStateChanged(bool bIsolated)
{
    bStrictLocalPreview = bIsolated;
    if (RefreshButton)
    {
        RefreshButton->SetIsEnabled(!bIsolated);
    }
    if (RefreshStatusText)
    {
        if (bIsolated)
        {
            RefreshStatusText->SetText(ProductText::Get(TEXT("Fleet.Local")));
        }
        else
        {
            RefreshStatusText->SetText(FText::GetEmpty());
        }
    }
    if (IsolationStatusText)
    {
        IsolationStatusText->SetVisibility(
            bIsolated ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    RefreshFromRegistry();
}

void UDroneListWidget::OnRefreshButtonClicked()
{
    if (bCommandLayout) return; // Command is Registry-only, including programmatic button invocation.
    if (bStrictLocalPreview)
    {
        if (RefreshStatusText)
        {
            RefreshStatusText->SetText(FText::FromString(TEXT("严格本地预演模式：无法请求刷新")));
        }
        return;
    }

    if (bRefreshing)
    {
        return;
    }

    bRefreshing = true;
    if (RefreshButton)
    {
        RefreshButton->SetIsEnabled(false);
    }
    if (RefreshStatusText)
    {
        RefreshStatusText->SetText(FText::FromString(TEXT("正在探测断连无人机...")));
    }

    UDroneNetworkManager* NetMgr = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UDroneNetworkManager>()
        : nullptr;
    if (!NetMgr)
    {
        HandleRefreshResponse(false, {});
        return;
    }

    NetMgr->RefreshDroneConnections([this](bool bSuccess, const TArray<int32>& RefreshedIds)
    {
        HandleRefreshResponse(bSuccess, RefreshedIds);
    });
}

void UDroneListWidget::OnVideoKeyboardControlButtonClicked()
{
    ADroneOpsPlayerController* Controller = Cast<ADroneOpsPlayerController>(GetOwningPlayer());
    if (!Controller) return;

    const bool bWasActive = Controller->IsVideoKeyboardControlActive();
    if (!Controller->ToggleVideoKeyboardControl()) return;

    if (VideoKeyboardControlLabel)
    {
        VideoKeyboardControlLabel->SetText(FText::FromString(
            bWasActive ? TEXT("视频键盘操控") : TEXT("退出视频键盘操控")));
    }
}

void UDroneListWidget::HandleRefreshResponse(bool bSuccess, const TArray<int32>& RefreshedIds)
{
    bRefreshing = false;
    if (RefreshButton)
    {
        RefreshButton->SetIsEnabled(true);
    }

    FString Message;
    if (!bSuccess)
    {
        Message = TEXT("刷新请求失败，请检查网络或后端状态");
    }
    else if (RefreshedIds.Num() == 0)
    {
        Message = TEXT("当前无断连无人机需要探测");
    }
    else
    {
        Message = FString::Printf(TEXT("已请求探测 %d 架断连无人机"), RefreshedIds.Num());
    }

    if (RefreshStatusText)
    {
        RefreshStatusText->SetText(FText::FromString(Message));
    }

    // 立即刷新一次列表，展示注册表最新状态（不会将 refreshed_drone_ids 直接置为 online）
    RefreshFromRegistry();
}

void UDroneListWidget::OnLabelSettingsChanged(int32 InDroneId, const FDroneLabelSettings& Settings)
{
    // 找到对应条目，直接刷新其名称和颜色，避免重建整个列表
    if (!DroneScrollBox)
    {
        return;
    }
    for (UWidget* Child : DroneScrollBox->GetAllChildren())
    {
        if (UDroneListItemWidget* Item = Cast<UDroneListItemWidget>(Child))
        {
            if (Item->GetDroneId() == InDroneId)
            {
                Item->ApplyLabelSettings(Settings);
                break;
            }
        }
    }
}

void UDroneListWidget::OnToggleDroneListClicked()
{
    if (!DronePanelBorder) return;
    const bool bVisible = DronePanelBorder->GetVisibility() != ESlateVisibility::Collapsed;
    DronePanelBorder->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UDroneListWidget::OnToggleEnemyListClicked()
{
    if (!EnemyListWidgetRef.IsValid()) return;
    bEnemyPanelVisible = !bEnemyPanelVisible;
    EnemyListWidgetRef->SetPanelVisible(bEnemyPanelVisible);
}

void UDroneListWidget::SetEnemyListWidget(UEnemyDroneListWidget* Widget)
{
    EnemyListWidgetRef = Widget;
}
