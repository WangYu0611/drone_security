#include "Command/PilotDispatchWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Command/CommandActionButton.h"
#include "Command/FigmaCommandStyle.h"
#include "Components/Border.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Shared/PilotControlSubsystem.h"

namespace
{
FText Label(const TCHAR* English, const TCHAR* Chinese)
{
    const bool bChinese = FInternationalization::Get().GetCurrentLanguage()->GetTwoLetterISOLanguageName() == TEXT("zh");
    return FText::FromString(bChinese ? Chinese : English);
}

FText ResultText(const FString& Code)
{
    if (Code == TEXT("GRANTED") || Code == TEXT("ALREADY_GRANTED")) return Label(TEXT("Assigned"), TEXT("已分配"));
    if (Code == TEXT("REVOKED")) return Label(TEXT("Control recalled"), TEXT("权限已收回"));
    if (Code == TEXT("UAV_BUSY")) return Label(TEXT("Aircraft in use"), TEXT("飞机正在执行任务或被占用"));
    if (Code == TEXT("UAV_ALREADY_ASSIGNED")) return Label(TEXT("Already assigned"), TEXT("飞机已分配"));
    if (Code == TEXT("STALE_STATE") || Code == TEXT("GRANT_CHANGED")) return Label(TEXT("State changed. Review and try again."), TEXT("状态已变化，请确认后重试"));
    if (Code == TEXT("PILOT_OFFLINE")) return Label(TEXT("Pilot offline"), TEXT("驾驶员离线"));
    if (Code == TEXT("MOCK_UAV_UNAVAILABLE") || Code == TEXT("UAV_NOT_AVAILABLE")) return Label(TEXT("Aircraft unavailable"), TEXT("飞机不可用"));
    return FText::AsCultureInvariant(Code);
}
}

FText UPilotDispatchWidget::Title()
{
    return Label(TEXT("Pilot control"), TEXT("驾驶员控制权"));
}

FText UPilotDispatchWidget::AssignmentText(const FPilotControlAircraft& Aircraft)
{
    if (Aircraft.PilotId.IsEmpty()) return FText::GetEmpty();
    FText Status = Label(TEXT("Assigned"), TEXT("待操控"));
    if (Aircraft.bInputFrozen) Status = Label(TEXT("Emergency stop"), TEXT("已急停"));
    else if (Aircraft.ControlState == TEXT("ACTIVE")) Status = Label(TEXT("Controlling"), TEXT("操控中"));
    return FText::Format(Label(TEXT("{0} · {1}"), TEXT("{0} · {1}")), FText::AsCultureInvariant(Aircraft.PilotId), Status);
}

UTextBlock* UPilotDispatchWidget::MakeText(const FText& Value, int32 Size, bool bBold)
{
    auto* Text = WidgetTree->ConstructWidget<UTextBlock>();
    FigmaUI::Text(Text, Size, FigmaUI::PrimaryText, bBold);
    Text->SetText(Value);
    return Text;
}

UCommandActionButton* UPilotDispatchWidget::MakeButton(FName Action, const FText& Text, int32 Index)
{
    auto* Button = WidgetTree->ConstructWidget<UCommandActionButton>();
    Button->Configure(Action, Index);
    Button->SetContent(MakeText(Text));
    FigmaUI::Button(Button, Action == TEXT("grant"), true);
    Button->OnAction.AddDynamic(this, &UPilotDispatchWidget::PerformAction);
    return Button;
}

void UPilotDispatchWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Layer = WidgetTree->ConstructWidget<UBorder>();
    WidgetTree->RootWidget = Layer;
    Layer->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, .73f));
    Layer->SetHorizontalAlignment(HAlign_Center);
    Layer->SetVerticalAlignment(VAlign_Center);

    auto* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(510.f);
    Layer->SetContent(Size);
    auto* Panel = WidgetTree->ConstructWidget<UBorder>();
    FigmaUI::Panel(Panel, FigmaUI::Surface, FigmaUI::Outline, 12.f, FMargin(24.f));
    Size->SetContent(Panel);
    auto* Body = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(Body);
    auto* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
    Body->AddChildToVerticalBox(Header)->SetPadding(FMargin(0, 0, 0, 14));
    Header->AddChildToHorizontalBox(MakeText(Title(), 23, true))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Header->AddChildToHorizontalBox(MakeButton(TEXT("close"), Label(TEXT("Close"), TEXT("关闭"))));

    SelectionText = MakeText(FText::GetEmpty());
    SelectionText->SetAutoWrapText(true);
    Body->AddChildToVerticalBox(SelectionText)->SetPadding(FMargin(0, 0, 0, 10));
    auto* GrantRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    Body->AddChildToVerticalBox(GrantRow)->SetPadding(FMargin(0, 0, 0, 12));
    PilotChoice = WidgetTree->ConstructWidget<UComboBoxString>();
    PilotChoice->SetContentPadding(FMargin(12, 6));
    auto* ChoiceSlot = GrantRow->AddChildToHorizontalBox(PilotChoice);
    ChoiceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ChoiceSlot->SetPadding(FMargin(0, 0, 12, 0));
    GrantButton = MakeButton(TEXT("grant"), Label(TEXT("Assign control"), TEXT("分配控制权")));
    GrantRow->AddChildToHorizontalBox(GrantButton);

    Feedback = MakeText(FText::GetEmpty(), 13);
    Feedback->SetAutoWrapText(true);
    Body->AddChildToVerticalBox(Feedback)->SetPadding(FMargin(0, 0, 0, 12));
    Body->AddChildToVerticalBox(MakeText(Label(TEXT("Current assignments"), TEXT("当前分配")), 16, true));
    auto* AssignmentSize = WidgetTree->ConstructWidget<USizeBox>();
    AssignmentSize->SetHeightOverride(165.f);
    Body->AddChildToVerticalBox(AssignmentSize)->SetPadding(FMargin(0, 6, 0, 0));
    Assignments = WidgetTree->ConstructWidget<UScrollBox>();
    AssignmentSize->SetContent(Assignments);
}

void UPilotDispatchWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (auto* Instance = GetGameInstance()) Control = Instance->GetSubsystem<UPilotControlSubsystem>();
    if (Control.IsValid()) Control->OnChanged.AddUObject(this, &UPilotDispatchWidget::Refresh);
    Refresh();
}

void UPilotDispatchWidget::NativeDestruct()
{
    if (Control.IsValid()) Control->OnChanged.RemoveAll(this);
    Super::NativeDestruct();
}

void UPilotDispatchWidget::Open(const TArray<FString>& Selection)
{
    Targets = Selection;
    SetVisibility(ESlateVisibility::Visible);
    Refresh();
}

void UPilotDispatchWidget::Refresh()
{
    if (GetVisibility() == ESlateVisibility::Collapsed || !Control.IsValid()) return;
    const auto* Model = Control.Get();
    const bool bCanSend = Model->IsReady() && !Model->IsRequestPending();
    SelectionText->SetText(Targets.IsEmpty() ? Label(TEXT("Select aircraft in the fleet list to assign control."), TEXT("在机队列表勾选要分配的飞机。"))
        : FText::Format(Label(TEXT("Selected aircraft · {0}"), TEXT("已选飞机 · {0}")), FText::AsCultureInvariant(FString::Join(Targets, TEXT(", ")))));

    TArray<FString> CurrentPilots;
    for (const auto& Pilot : Model->GetPilots()) if (Pilot.bOnline) CurrentPilots.Add(Pilot.PilotId);
    if (CurrentPilots != OnlinePilots)
    {
        const FString Previous = PilotChoice->GetSelectedOption();
        OnlinePilots = MoveTemp(CurrentPilots);
        PilotChoice->ClearOptions();
        for (const auto& Pilot : OnlinePilots) PilotChoice->AddOption(Pilot);
        if (OnlinePilots.Contains(Previous)) PilotChoice->SetSelectedOption(Previous);
        else if (!OnlinePilots.IsEmpty()) PilotChoice->SetSelectedIndex(0);
    }
    PilotChoice->SetIsEnabled(bCanSend && !OnlinePilots.IsEmpty());
    GrantButton->SetIsEnabled(bCanSend && !Targets.IsEmpty() && !OnlinePilots.IsEmpty());

    TArray<FString> Lines;
    for (const auto& Result : Model->GetLastResults())
    {
        FString Line = Result.UavId + TEXT(" · ") + ResultText(Result.Code).ToString();
        if (Result.Code == TEXT("REVOKED"))
        {
            const auto* Aircraft = Model->GetAircraft().FindByPredicate([&](const auto& Row) { return Row.UavId == Result.UavId; });
            const bool bConfirmed = Aircraft && Aircraft->AircraftState == TEXT("CONFIRMED");
            Line += TEXT(" · ") + (bConfirmed ? Label(TEXT("Aircraft state confirmed"), TEXT("飞机状态已确认"))
                : Label(TEXT("Aircraft state pending"), TEXT("飞机状态待确认"))).ToString();
        }
        Lines.Add(MoveTemp(Line));
    }
    if (Model->IsRequestPending()) Feedback->SetText(Label(TEXT("Waiting for Backend confirmation…"), TEXT("等待后端确认…")));
    else if (!Model->IsReady()) Feedback->SetText(Label(TEXT("Pilot control connection unavailable"), TEXT("驾驶员控制权连接不可用")));
    else if (!Lines.IsEmpty()) Feedback->SetText(FText::AsCultureInvariant(FString::Join(Lines, TEXT("\n"))));
    else if (OnlinePilots.IsEmpty()) Feedback->SetText(Label(TEXT("No pilot online"), TEXT("没有在线驾驶员")));
    else Feedback->SetText(Model->GetStatusCode() == TEXT("READY") ? FText::GetEmpty() : ResultText(Model->GetStatusCode()));

    Assignments->ClearChildren();
    RevokeTargets.Reset();
    for (const auto& Aircraft : Model->GetAircraft())
    {
        if (Aircraft.GrantId.IsEmpty()) continue;
        auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        Assignments->AddChild(Row);
        auto* TextSlot = Row->AddChildToHorizontalBox(MakeText(FText::AsCultureInvariant(Aircraft.UavId + TEXT(" · ") + AssignmentText(Aircraft).ToString())));
        TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        TextSlot->SetVerticalAlignment(VAlign_Center);
        const int32 Index = RevokeTargets.Add(Aircraft.UavId);
        auto* Revoke = MakeButton(TEXT("revoke"), Label(TEXT("Recall"), TEXT("收回")), Index);
        Revoke->SetIsEnabled(bCanSend);
        Row->AddChildToHorizontalBox(Revoke);
    }
    if (RevokeTargets.IsEmpty()) Assignments->AddChild(MakeText(Label(TEXT("No aircraft assigned"), TEXT("暂无已分配飞机"))));
}

void UPilotDispatchWidget::PerformAction(FName Action, int32 Index)
{
    if (Action == TEXT("close"))
    {
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    if (!Control.IsValid() || !Control->IsReady() || Control->IsRequestPending()) return;
    if (Action == TEXT("grant")) Control->Grant(PilotChoice->GetSelectedOption(), Targets);
    else if (Action == TEXT("revoke") && RevokeTargets.IsValidIndex(Index))
    {
        const FString UavId = RevokeTargets[Index];
        Control->Revoke(UavId);
    }
}
