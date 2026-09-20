#include "Command/CommandShellWidget.h"
#include "Shared/ExecutionPresentation.h"
#include "Components/WidgetSwitcher.h"
#include "Shared/PlanWidgetSupport.h"
#include "Shared/OperationalEventStore.h"
#include "Shared/SecurityPlanWorkspaceWidget.h"
#include "Shared/Stage1HeaderWidget.h"
#include "Shared/ProductText.h"
#include "Command/CommandCenterPanel.h"
#include "Command/OperationalLogWidget.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Engine/GameInstance.h"
#include "Command/CommandTheme.h"
#include "Shared/DroneMissionService.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandMapInteractionService.h"
#include "Command/CommandAlertStore.h"
#include "Command/CommandAlarmPanel.h"
#include "Command/CommandActionButton.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "UI/DroneListWidget.h"
#include "UI/DroneListItemWidget.h"
#include "UI/SequenceDispatchPanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"

namespace
{
FString AvailabilityText(EDroneAvailability State)
{
    switch (State) {
    case EDroneAvailability::Online: return TEXT("在线");
    case EDroneAvailability::Lost: return TEXT("失联");
    default: return TEXT("离线"); }
}
FString TaskText(const FDroneTaskStateSnapshot& State)
{
    return StaticEnum<EDroneTaskState>()->GetDisplayNameTextByValue(static_cast<int64>(State.State)).ToString();
}
bool IsActiveTask(EDroneTaskState State)
{
    return State != EDroneTaskState::Standby && State != EDroneTaskState::Completed && State != EDroneTaskState::Error;
}
}

void UCommandShellWidget::InitializeManager(UCommandScreenManager* InManager)
{
    Manager = InManager;
    if (AlarmPanel) AlarmPanel->SetMapService(InManager->GetMapService());
    if (InManager->GetMapService()) InManager->GetMapService()->OnMapModeChanged.AddUniqueDynamic(this, &UCommandShellWidget::MapModeChanged);
    if (InManager->GetClientRole() != EDroneClientRole::Command)
    {
        Header->SetPadding(FMargin(112, 12, 12, 12));
        if (MapToolbar) MapToolbar->SetVisibility(ESlateVisibility::Collapsed);
        MissionActions->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
}
void UCommandShellWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    WidgetTree->RootWidget = Root;
    auto Text = [this](const FString& Value, int32 Size = 15)
    {
        UTextBlock* Item = WidgetTree->ConstructWidget<UTextBlock>();
        Item->SetText(ProductText::Source(Value));
        CommandTheme::Text(Item, Size);
        Item->SetAutoWrapText(true);
        return Item;
    };
    auto Border = [this]()
    {
        UBorder* Item = WidgetTree->ConstructWidget<UBorder>();
        CommandTheme::Panel(Item);
        Item->SetPadding(FMargin(CommandTheme::Padding));
        return Item;
    };
    auto Fill = [](UVerticalBox* Box, UWidget* Widget, float Weight)
    {
        FSlateChildSize Size(ESlateSizeRule::Fill); Size.Value = Weight;
        UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Widget);
        Slot->SetSize(Size); Slot->SetPadding(FMargin(0, 4));
    };
    Header = Border();
    CommandTheme::Panel(Header, CommandTheme::Background);
    Header->SetPadding(FMargin(0));
    Header->SetContent(CreateWidget<UStage1HeaderWidget>(GetOwningPlayer()));
    UCanvasPanelSlot* TopSlot = Root->AddChildToCanvas(Header);
    TopSlot->SetAnchors(FAnchors(0, 0, 1, 0)); TopSlot->SetOffsets(FMargin(0, 0, 0, 72));

    UBorder* Left = Border();
    UCanvasPanelSlot* LeftSlot = Root->AddChildToCanvas(Left);
    const bool bCommand = UCommandScreenManager::ResolveClientRole() == EDroneClientRole::Command;
    LeftSlot->SetAnchors(FAnchors(0, 0, bCommand ? 0.27f : 0.24f, 1)); LeftSlot->SetOffsets(FMargin(8, 80, 0, 8));
    UVerticalBox* LeftContent = WidgetTree->ConstructWidget<UVerticalBox>();
    Left->SetContent(LeftContent);
    LeftContent->AddChild(Text(TEXT("UAV FLEET  /  机队态势"), CommandTheme::TitleSize));
    Overview = Text(TEXT("等待注册表数据"), 14);
    LeftContent->AddChild(Overview);
    DroneList = CreateWidget<UDroneListWidget>(GetOwningPlayer(), UDroneListWidget::StaticClass());
    DroneList->ListItemClass = UDroneListItemWidget::StaticClass();
    DroneList->ConfigureCommandLayout();
    if(bCommand && DroneList->WidgetTree) for(const TCHAR* Name:{TEXT("TitleText"),TEXT("SubtitleText")})
        if(auto* Item=DroneList->WidgetTree->FindWidget(FName(Name)))Item->SetVisibility(ESlateVisibility::Collapsed);
    Fill(LeftContent, DroneList, bCommand ? 0.66f : 0.52f);
    LeftContent->AddChild(Text(TEXT("告警中心 · 最近200条 / 本会话"), 14));
    AlarmPanel = CreateWidget<UCommandAlarmPanel>(GetOwningPlayer(), UCommandAlarmPanel::StaticClass());
    Fill(LeftContent, AlarmPanel, bCommand ? 0.34f : 0.26f);
    if(!bCommand) LeftContent->AddChild(Text(TEXT("任务状态 · Registry"), 14));
    UScrollBox* TaskScroll = WidgetTree->ConstructWidget<UScrollBox>();
    Tasks = Text(TEXT("尚无任务状态"), 13);
    TaskScroll->AddChild(Tasks);
    if(!bCommand) Fill(LeftContent, TaskScroll, 0.22f);

    if (!bCommand)
    {
    CenterHost = WidgetTree->ConstructWidget<UOverlay>();
    CenterHost->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UCanvasPanelSlot* CenterSlot = Root->AddChildToCanvas(CenterHost);
    CenterSlot->SetAnchors(FAnchors(0.24f, 0, 1, 1)); CenterSlot->SetOffsets(FMargin(8, 80, 8, 8));
    // Transparent center hosts the EXISTING game viewport, not a second map or SceneCapture.
    }
    MissionPanel = Border();
    UVerticalBox* MissionContent = WidgetTree->ConstructWidget<UVerticalBox>();
    MissionPanel->SetContent(MissionContent);
    MissionContent->AddChild(Text(TEXT("TACTICAL DATA  /  无人机态势"), 12));
    SelectedInfo = Text(TEXT("未选择无人机"), 21);
    SelectedInfo->SetMinDesiredWidth(480.f);
    SelectedInfo->SetWrapTextAt(480.f);
    MissionContent->AddChild(SelectedInfo);
    SelectedMetrics = Text(TEXT("ALT N/A   SPD N/A   BAT N/A"), 17);
    SelectedMetrics->SetWrapTextAt(480.f);
    MissionContent->AddChild(SelectedMetrics);
    SelectedMission = Text(TEXT("LINK N/A  |  MISSION N/A"), 12);
    SelectedMission->SetWrapTextAt(480.f);
    CommandTheme::Text(SelectedMission, 12, CommandTheme::SecondaryText);
    MissionContent->AddChild(SelectedMission);
    if(bCommand) {
        auto* TaskHeight=WidgetTree->ConstructWidget<USizeBox>();TaskHeight->SetHeightOverride(70);TaskHeight->SetContent(TaskScroll);MissionContent->AddChild(TaskHeight);
    }
    UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
    UHorizontalBox* MoreActions = WidgetTree->ConstructWidget<UHorizontalBox>();
    auto Button = [&](UHorizontalBox* Row, const TCHAR* Action, const TCHAR* Label, bool bEnabled = true)
    {
        UCommandActionButton* Item = WidgetTree->ConstructWidget<UCommandActionButton>(UCommandActionButton::StaticClass(), FName(Action));
        Item->Configure(FName(Action));
        Item->OnAction.AddDynamic(this, &UCommandShellWidget::ActionRequested);
        UTextBlock* LabelText = Text(Label, 12);
        LabelText->SetAutoWrapText(false);
        Item->SetContent(LabelText);
        Item->SetIsEnabled(bEnabled);
        Item->SetToolTipText(ProductText::Source(Label));
        Row->AddChildToHorizontalBox(Item)->SetPadding(FMargin(3, 3));
        ActionButtons.Add(Item);
    };
    if (!bCommand)
    {
    Button(Actions, TEXT("focus"), TEXT("定位"));
    Button(Actions, TEXT("plan"), TEXT("开始规划"));
    Button(Actions, TEXT("delete"), TEXT("删除选中航点"));
    Button(Actions, TEXT("confirm"), TEXT("确认 / 预演 / 派发"));
    Button(Actions, TEXT("cancel"), TEXT("取消规划"));
    }
    Button(MoreActions, TEXT("pause"), TEXT("暂停请求"));
    Button(MoreActions, TEXT("resume"), TEXT("恢复请求"));
    UHorizontalBox* Tools = nullptr;
    if (!bCommand)
    {
    Button(MoreActions, TEXT("sequence"), TEXT("序列工具"));
    Button(MoreActions, TEXT("add"), TEXT("添加航点 · 坐标输入"));
    Tools = WidgetTree->ConstructWidget<UHorizontalBox>();
    Button(Tools, TEXT("save"), TEXT("保存路径"));
    Button(Tools, TEXT("stop"), TEXT("停止预演"));
    Button(Tools, TEXT("speed"), TEXT("航段速度"));

    }
    MissionContent->AddChild(Actions);
    MissionActions = WidgetTree->ConstructWidget<UVerticalBox>();
    MissionContent->RemoveChild(Actions);
    MissionActions->AddChild(Actions);
    MissionActions->AddChild(MoreActions);
    if (Tools) MissionActions->AddChild(Tools);

    MissionContent->AddChild(MissionActions);
    ActionStatus = Text(TEXT("地图点击添加航点；Delete 删除。执行沿用原预演/派发确认流程。"), 12);
    MissionActions->AddChild(ActionStatus);
    MissionActions->SetVisibility(bCommand ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (bCommand)
    {
        Left->SetVisibility(ESlateVisibility::Collapsed);
        auto* Nav=WidgetTree->ConstructWidget<UVerticalBox>();
        auto* NavSlot=Root->AddChildToCanvas(Nav);
        NavSlot->SetAnchors(FAnchors(0,0,0,1));NavSlot->SetOffsets(FMargin(12,96,152,48));
        const TCHAR* Keys[]={TEXT("Nav.Overview"),TEXT("Nav.Plans"),TEXT("Nav.Fleet"),TEXT("Nav.Alerts"),TEXT("Nav.Logs")};
        for(int I=0;I<5;++I){auto* B=PlanUI::Button(WidgetTree,Nav,TEXT("navigate"),Keys[I],I);
            B->OnAction.AddDynamic(this,&UCommandShellWidget::Navigate);CommandTheme::Button(B,I==0);
            Cast<UVerticalBoxSlot>(B->Slot)->SetPadding(FMargin(0,0,0,12));PrimaryNavigationButtons.Add(B);}
        WorkspacePages=WidgetTree->ConstructWidget<UWidgetSwitcher>();
        auto* WorkspaceSlot=Root->AddChildToCanvas(WorkspacePages);
        WorkspaceSlot->SetAnchors(FAnchors(0,0,1,1));WorkspaceSlot->SetOffsets(FMargin(184,88,16,44));
        auto* Home=WidgetTree->ConstructWidget<UVerticalBox>();WorkspacePages->AddChild(Home);
        PlanUI::Label(WidgetTree,Home,ProductText::Get(TEXT("Nav.Overview")),26);
        Overview->RemoveFromParent();Home->AddChildToVerticalBox(Overview)->SetPadding(FMargin(0,24));
        CurrentExecutionSummary=PlanUI::Label(WidgetTree,Home,ProductText::Get(TEXT("Execution.None")),20);
        auto* OpenExecution=PlanUI::Button(WidgetTree,Home,TEXT("open_execution"),TEXT("Execution.Open"),1);OpenExecution->OnAction.AddDynamic(this,&UCommandShellWidget::Navigate);
        CurrentPlanSummary=PlanUI::Label(WidgetTree,Home,FText::GetEmpty(),20);
        auto* Open=PlanUI::Button(WidgetTree,Home,TEXT("navigate"),TEXT("Workflow.OpenPlan"),1);Open->OnAction.AddDynamic(this,&UCommandShellWidget::Navigate);
        PlanUI::Label(WidgetTree,Home,ProductText::Get(TEXT("Workflow.RecentEvents")),18);
        RecentEvents=PlanUI::Label(WidgetTree,Home,FText::GetEmpty(),14);
        CenterPanel=CreateWidget<UCommandCenterPanel>(GetOwningPlayer());WorkspacePages->AddChild(CenterPanel);
        auto* Fleet=WidgetTree->ConstructWidget<UVerticalBox>();WorkspacePages->AddChild(Fleet);
        PlanUI::Label(WidgetTree,Fleet,ProductText::Get(TEXT("Nav.Fleet")),26);
        DroneList->RemoveFromParent();Fill(Fleet,DroneList,1);
        MissionPanel->RemoveFromParent();Fleet->AddChild(MissionPanel);MissionPanel->SetVisibility(ESlateVisibility::Visible);
        MissionActions->SetVisibility(ESlateVisibility::Collapsed);
        AlarmPanel->RemoveFromParent();WorkspacePages->AddChild(AlarmPanel);
        WorkspacePages->AddChild(CreateWidget<UOperationalLogWidget>(GetOwningPlayer()));
        auto* Hint=PlanUI::Label(WidgetTree,Root,ProductText::Get(TEXT("Workflow.GlobalHint")),12);
        auto* HintSlot=Cast<UCanvasPanelSlot>(Hint->Slot);HintSlot->SetAnchors(FAnchors(0,1,1,1));HintSlot->SetOffsets(FMargin(184,-32,16,28));
        auto* Languages=WidgetTree->ConstructWidget<UHorizontalBox>();auto* LanguageSlot=Root->AddChildToCanvas(Languages);
        LanguageSlot->SetAnchors(FAnchors(1,0));LanguageSlot->SetAlignment(FVector2D(1,0));LanguageSlot->SetPosition(FVector2D(-16,12));LanguageSlot->SetAutoSize(true);LanguageSlot->SetZOrder(20);
        for(const TCHAR* Locale:{TEXT("zh-Hans"),TEXT("en")}){auto* B=PlanUI::Button(WidgetTree,Languages,Locale,Locale==FString(TEXT("en"))?TEXT("Workflow.English"):TEXT("Workflow.Chinese"));B->OnAction.AddDynamic(this,&UCommandShellWidget::Navigate);}
        WorkspacePages->SetActiveWidgetIndex(0);
        SelectedInfo->SetMinDesiredWidth(0);SelectedInfo->SetWrapTextAt(0);
        return;
    }
    UOverlaySlot* MissionSlot = CenterHost->AddChildToOverlay(MissionPanel);
    MissionSlot->SetHorizontalAlignment(HAlign_Left);
    MissionSlot->SetVerticalAlignment(VAlign_Top);
    MissionSlot->SetPadding(FMargin(12, 48));

    MapToolbar = Border();
    CommandTheme::Panel(MapToolbar, CommandTheme::Surface.CopyWithNewOpacity(0.96f));
    MapToolbar->SetPadding(FMargin(5));
    UHorizontalBox* MapRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    MapToolbar->SetContent(MapRow);
    auto ToolbarButton = [&](const TCHAR* Name, const TCHAR* Label)
    {
        UCommandActionButton* Item = WidgetTree->ConstructWidget<UCommandActionButton>(UCommandActionButton::StaticClass(), FName(Name));
        Item->Configure(FName(Name));
        Item->OnAction.AddDynamic(this, &UCommandShellWidget::ActionRequested);
        UTextBlock* Caption = Text(Label, 12); Caption->SetAutoWrapText(false);
        Item->SetContent(Caption);
        MapRow->AddChildToHorizontalBox(Item)->SetPadding(FMargin(4, 0));
        return Item;
    };
    Map2DButton = ToolbarButton(TEXT("map2d"), TEXT("2D"));
    Map3DButton = ToolbarButton(TEXT("map3d"), TEXT("3D"));
    ToolbarButton(TEXT("mapfocus"), TEXT("Locate / 定位"));
    ToolbarButton(TEXT("mission"), TEXT("Mission Tools / 任务"));
    UOverlaySlot* ToolbarSlot = CenterHost->AddChildToOverlay(MapToolbar);
    ToolbarSlot->SetHorizontalAlignment(HAlign_Right);
    ToolbarSlot->SetVerticalAlignment(VAlign_Top);
    ToolbarSlot->SetPadding(FMargin(12, 4));
}
void UCommandShellWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (Manager.IsValid()) AlarmPanel->SetMapService(Manager->GetMapService());
    Refresh();
}
void UCommandShellWidget::Refresh()
{
    if (CenterPanel) {
        CenterPanel->Refresh();
        if(auto* Label=Cast<UTextBlock>(Header->GetContent())) {Label->SetText(ProductText::Source(CenterPanel->GetHeaderText()));CommandTheme::Text(Label,15);}
    }
    if (Manager.IsValid() && Manager->GetMapService()) MapModeChanged(Manager->GetMapService()->GetMapMode());
    UDroneRegistrySubsystem* Registry = Manager.IsValid() ? Manager->GetRegistry() : nullptr;
    if (!Registry || !Overview) return;
    int32 Online = 0, Active = 0, Standby = 0;
    FString TaskLines;
    TArray<FDroneDescriptor> Drones = Registry->GetFriendlyDroneDescriptors();
    Drones.Sort([](const FDroneDescriptor& A, const FDroneDescriptor& B) { return A.DroneId < B.DroneId; });
    for (const FDroneDescriptor& Drone : Drones)
    {
        FDroneTelemetrySnapshot Telemetry;
        if (Registry->GetTelemetry(Drone.DroneId, Telemetry) && Telemetry.Availability == EDroneAvailability::Online) ++Online;
        FDroneTaskStateSnapshot Task;
        const bool bHasTask = Registry->GetTaskState(Drone.DroneId, Task);
        if (bHasTask && IsActiveTask(Task.State)) ++Active;
        if (bHasTask && Task.State == EDroneTaskState::Standby) ++Standby;
        TaskLines += FString::Printf(TEXT("%s (d%d)  %s\n"), *Drone.Name, Drone.DroneId,
            bHasTask ? *FString::Printf(TEXT("%s · %s · %s"), *Task.ArrayId, *DroneCommandModeToProtocolString(Task.Mode), *TaskText(Task)) : TEXT("未收到任务状态"));
    }
    const UCommandAlertStore* Store = GetGameInstance()->GetSubsystem<UCommandAlertStore>();
    Overview->SetText(ProductText::Source(FString::Printf(TEXT("注册 %d   在线 %d   活动任务 %d\n待命 %d   未处理告警 %d\n通信质量 / AI：未接入"),
        Drones.Num(), Online, Active, Standby, Store ? Store->GetUnhandledCount() : 0)));
    if (auto* Sync = GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>(); Sync && Sync->IsEnabled())
        Overview->SetText(ProductText::Source(Sync->GetStatusText() + TEXT("\n") + Overview->GetText().ToString()));
    if(CenterPanel) Overview->SetText(FText::Format(ProductText::Get(TEXT("Command.Count")),FText::AsNumber(Drones.Num()),FText::AsNumber(Online),FText::AsNumber(Store?Store->GetUnhandledCount():0)));
    if(WorkspacePages && !bRestoredWorkspace){auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();if(Sync->IsHydrated()){bRestoredWorkspace=true;if(!PlanUI::Field(Sync->GetContext(),TEXT("active_security_plan_id")).IsEmpty())Navigate(TEXT("navigate"),1);}}
    if(CurrentExecutionSummary)CurrentExecutionSummary->SetText(ExecutionUI::Summary(ExecutionUI::Latest(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),TEXT(""),true)));
    if(CurrentPlanSummary){
        auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
        const auto Plan=PlanUI::Find(Sync->GetPlans(),TEXT("plans"),PlanUI::Field(Sync->GetContext(),TEXT("active_security_plan_id")));
        CurrentPlanSummary->SetText(Plan?FText::Format(ProductText::Get(TEXT("Workflow.CurrentPlan")),PlanUI::User(PlanUI::Field(Plan,TEXT("name"))),ProductText::Get(TEXT("Plan.")+PlanUI::Field(Plan,TEXT("status")))):ProductText::Get(TEXT("Plan.NoPlan")));
        TArray<FText> Lines;const auto& Events=GetGameInstance()->GetSubsystem<UOperationalEventStore>()->GetEvents();
        for(int I=Events.Num()-1;I>=FMath::Max(0,Events.Num()-5);--I)Lines.Add(Events[I].DisplayText());
        RecentEvents->SetText(FText::Join(PlanUI::User(TEXT("\n\n")),Lines));
    }
    const auto Executions=PlanUI::Object(GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->GetPlans(),TEXT("executions"));
    if(Executions)for(const auto& Entry:Executions->Values){const auto E=Entry.Value->AsObject();if(ExecutionUI::Active(E))TaskLines+=TEXT("\n")+ExecutionUI::Summary(E).ToString();}
    Tasks->SetText(ProductText::Source(TaskLines.IsEmpty() ? TEXT("暂无已注册无人机") : TaskLines));
    const int32 Id = Registry->GetPrimarySelectedDrone();
    FDroneDescriptor Desc;
    FDroneTelemetrySnapshot Telemetry;
    FDroneTaskStateSnapshot Task;
    FString Info = TEXT("未选择无人机");
    FString Metrics = TEXT("ALT N/A   SPD N/A   BAT N/A");
    FString Mission = TEXT("LINK N/A  |  MISSION N/A");
    if (Registry->GetDroneDescriptor(Id, Desc))
    {
        const bool bHasTelemetry = Registry->GetTelemetry(Id, Telemetry);
        const bool bHasTask = Registry->GetTaskState(Id, Task);
        const FString Battery = bHasTelemetry && Telemetry.BatteryPercent >= 0
            ? FString::Printf(TEXT("%d%%"), Telemetry.BatteryPercent) : TEXT("N/A");
        const bool bGps = bHasTelemetry && Telemetry.bGpsFix && FMath::IsFinite(Telemetry.GpsLatitude)
            && FMath::IsFinite(Telemetry.GpsLongitude) && FMath::IsFinite(Telemetry.GpsAltitude);
        const bool bLive = bHasTelemetry && Telemetry.Availability == EDroneAvailability::Online;
        const FString Altitude = bGps ? FString::Printf(TEXT("%.1f m"), Telemetry.GpsAltitude) : TEXT("N/A");
        const FString Speed = bLive && Telemetry.LastUpdateTime > 0 && !Telemetry.Velocity.ContainsNaN()
            ? FString::Printf(TEXT("%.1f m/s"), Telemetry.Velocity.Size()) : TEXT("N/A");
        Info = FString::Printf(TEXT("UAV %02d  /  %s   ·   %s"), Id, *Desc.Name, bLive ? TEXT("LIVE") : TEXT("LOST"));
        Metrics=FText::Format(ProductText::Get(TEXT("Command.Metrics")),FText::AsCultureInvariant(Altitude),FText::AsCultureInvariant(Speed),FText::AsCultureInvariant(Battery)).ToString();
        Mission=FText::Format(ProductText::Get(TEXT("Command.Link")),ProductText::Source(bHasTelemetry?AvailabilityText(Telemetry.Availability):TEXT("N/A")),ProductText::Source(bHasTask?TaskText(Task):TEXT("N/A"))).ToString();
    }
    SelectedInfo->SetText(ProductText::Source(Info));
    SelectedMetrics->SetText(ProductText::Source(Metrics));
    SelectedMission->SetText(ProductText::Source(Mission));
    if (Manager->GetClientRole() == EDroneClientRole::Command)
    {
        const bool bCanPause = GetGameInstance()->GetSubsystem<UDroneMissionService>()->CanPauseSelected();
        for (const auto& Button : ActionButtons) Button->SetIsEnabled(bCanPause);
    }
    UCommandMapInteractionService* Map = Manager->GetMapService();
    if (ActionButtons.Num() >= 9 && Map)
    {
        const bool Planning = Map->IsPlanning();
        ActionButtons[0]->SetIsEnabled(Id > 0 && !Planning);
        ActionButtons[1]->SetIsEnabled(!Planning);
        ActionButtons[2]->SetIsEnabled(Planning);
        ActionButtons[3]->SetIsEnabled(Planning);
        ActionButtons[4]->SetIsEnabled(Planning);
        ActionButtons[5]->SetIsEnabled(Map->CanPauseSelected());
        ActionButtons[6]->SetIsEnabled(Map->CanPauseSelected());
        ActionButtons[8]->SetIsEnabled(Planning);
    }
}
void UCommandShellWidget::AttachCenterWidget(UUserWidget* Widget)
{
    if (!Widget || !CenterHost || Widget->GetParent() == CenterHost) return;
    UOverlaySlot* HostSlot = CenterHost->AddChildToOverlay(Widget);
    HostSlot->SetHorizontalAlignment(HAlign_Fill); HostSlot->SetVerticalAlignment(VAlign_Fill);
}
void UCommandShellWidget::ShowDroneList(bool bVisible)
{
    if (DroneList) DroneList->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}
bool UCommandShellWidget::IsCursorOverMap() const
{
    if (Manager.IsValid() && Manager->GetClientRole() == EDroneClientRole::Command) return false;
    if (!IsVisible()) return true;
    if (!CenterHost || !FSlateApplication::IsInitialized()) return false;
    const FVector2D CursorPosition = FSlateApplication::Get().GetCursorPos();
    if (!CenterHost->GetCachedGeometry().IsUnderLocation(CursorPosition)
        || MissionPanel->GetCachedGeometry().IsUnderLocation(CursorPosition)
        || (MapToolbar->IsVisible() && MapToolbar->GetCachedGeometry().IsUnderLocation(CursorPosition))) return false;
    // Attached legacy panels have full-size transparent roots. Only their visible controls/cards block the map.
    bool bOverControl = false;
    for (UWidget* Child : CenterHost->GetAllChildren())
        if (UUserWidget* Panel = Cast<UUserWidget>(Child); Panel && Panel->IsVisible() && Panel->WidgetTree)
            Panel->WidgetTree->ForEachWidget([&](UWidget* Widget)
            {
                for (UWidget* Parent = Widget; Parent; Parent = Parent->GetParent())
                    if (!Parent->IsVisible()) return;
                if ((Widget->IsA<UBorder>() || Widget->IsA<UButton>() || Widget->IsA<UScrollBox>()
                    || Widget->IsA<UEditableTextBox>() || Widget->IsA<UComboBoxString>())
                    && Widget->GetCachedGeometry().IsUnderLocation(CursorPosition)) bOverControl = true;
            });
    return !bOverControl;
}
void UCommandShellWidget::ActionRequested(FName Action, int32 ItemId)
{
    if(Action==TEXT("details")){MissionPanel->SetVisibility(MissionPanel->IsVisible()?ESlateVisibility::Collapsed:ESlateVisibility::Visible);return;}
    if (!Manager.IsValid()) return;
    if (Manager->GetClientRole() == EDroneClientRole::Command)
    {
        const bool bOk = (Action == TEXT("pause") || Action == TEXT("resume"))
            && GetGameInstance()->GetSubsystem<UDroneMissionService>()->SetSelectedPaused(Action == TEXT("pause"));
        ActionStatus->SetText(ProductText::Source(bOk ? TEXT("请求已提交；等待任务状态更新。") : TEXT("请求不可用：检查选择及连接状态。")));
        Refresh();
        return;
    }
    UCommandMapInteractionService* Map = Manager->GetMapService();
    if (!Map) return;
    if (Action == TEXT("map2d")) { Map->SetMapMode(ECommandMapMode::Map2D); return; }
    if (Action == TEXT("map3d")) { Map->SetMapMode(ECommandMapMode::Map3D); return; }
    if (Action == TEXT("mission")) { MissionActions->SetVisibility(MissionActions->IsVisible() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible); return; }
    bool bOk = false;
    if (Action == TEXT("focus")) bOk = Map->FocusDrone(Manager->GetRegistry()->GetPrimarySelectedDrone());
    if (Action == TEXT("mapfocus")) bOk = Map->FocusDrone(Manager->GetRegistry()->GetPrimarySelectedDrone());
    if (Action == TEXT("plan")) { Manager->ShowPathPanel(); bOk = Map->BeginPathPlanning(); }
    if (Action == TEXT("delete")) bOk = Map->DeleteSelectedWaypoint();
    if (Action == TEXT("confirm")) { Manager->ShowPathPanel(); bOk = Map->ConfirmPath(); }
    if (Action == TEXT("cancel")) bOk = Map->CancelPathPlanning();
    if (Action == TEXT("pause")) bOk = Map->SetSelectedPaused(true);
    if (Action == TEXT("resume")) bOk = Map->SetSelectedPaused(false);
    if (Action == TEXT("add")) { Manager->ToggleGeographicPanel(); bOk = true; }
    if (Action == TEXT("sequence")) { Manager->TogglePathTools(); bOk = true; }
    if (Action == TEXT("speed")) { Manager->ToggleSpeedPanel(); bOk = true; }
    if (Action == TEXT("save")) { if (auto* Panel = Manager->ShowPathPanel()) { Panel->SaveCommandPath(); bOk = true; } }
    if (Action == TEXT("stop")) { if (auto* Panel = Manager->ShowPathPanel()) { Panel->StopCommandPlayback(); bOk = true; } }
    ActionStatus->SetText(ProductText::Source(bOk
        ? TEXT("操作已交给现有流程；任务执行结果以返回状态为准。")
        : TEXT("当前操作不可用：检查选中、航点、地图对象或连接状态。")));
    Refresh();
}

void UCommandShellWidget::MapModeChanged(ECommandMapMode Mode)
{
    if (!Map2DButton || !Map3DButton) return;
    CommandTheme::Button(Map2DButton, Mode == ECommandMapMode::Map2D);
    CommandTheme::Button(Map3DButton, Mode == ECommandMapMode::Map3D);
}
void UCommandShellWidget::NativeDestruct()
{
    if (Manager.IsValid() && Manager->GetMapService()) Manager->GetMapService()->OnMapModeChanged.RemoveAll(this);
    Super::NativeDestruct();
}

void UCommandShellWidget::Navigate(FName Action,int32 Index){
    if(Action==TEXT("open_execution")){auto* S=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();const auto E=ExecutionUI::Latest(S->GetPlans(),TEXT(""),true);if(E){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("action"),TEXT("select"));R->SetStringField(TEXT("plan_id"),PlanUI::Field(E,TEXT("plan_id")));R->SetStringField(TEXT("mission_id"),PlanUI::Field(E,TEXT("mission_id")));S->SubmitPlan(R,[](TSharedPtr<FJsonObject>){});}}

    if(Action==TEXT("en") || Action==TEXT("zh-Hans")){GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->SetLanguage(Action.ToString());return;}
    if(!WorkspacePages || Index<0 || Index>=5)return;
    WorkspacePages->SetActiveWidgetIndex(Index);
    for(int I=0;I<PrimaryNavigationButtons.Num();++I)CommandTheme::Button(PrimaryNavigationButtons[I],I==Index);
}
