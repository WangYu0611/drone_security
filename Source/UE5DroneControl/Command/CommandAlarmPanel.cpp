#include "Command/CommandAlarmPanel.h"
#include "Shared/ProductText.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Command/CommandTheme.h"
#include "Components/Border.h"
#include "Components/ScrollBoxSlot.h"
#include "Command/CommandAlertStore.h"
#include "Command/CommandActionButton.h"
#include "Command/CommandMapInteractionService.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Engine/GameInstance.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"

void UCommandAlarmPanel::SetMapService(UCommandMapInteractionService* Service) { Map = Service; }
void UCommandAlarmPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    Rows = WidgetTree->ConstructWidget<UScrollBox>();
    WidgetTree->RootWidget = Rows;
}
void UCommandAlarmPanel::NativeConstruct()
{
    Super::NativeConstruct();
    Store = GetGameInstance()->GetSubsystem<UCommandAlertStore>();
    if (Store.IsValid())
    {
        Store->OnAlertAdded.AddUniqueDynamic(this, &UCommandAlarmPanel::AlertChanged);
        Store->OnAlertUpdated.AddUniqueDynamic(this, &UCommandAlarmPanel::AlertChanged);
    }
    Refresh();
}
void UCommandAlarmPanel::NativeDestruct()
{
    if (Store.IsValid())
    {
        Store->OnAlertAdded.RemoveAll(this);
        Store->OnAlertUpdated.RemoveAll(this);
    }
    Store.Reset();
    Super::NativeDestruct();
}
void UCommandAlarmPanel::AlertChanged(int32 Id) { Refresh(); }
void UCommandAlarmPanel::Refresh()
{
    if (!Rows) return;
    Rows->ClearChildren();
    const TArray<FCommandAlert> Alerts = Store.IsValid() ? Store->GetAlerts() : TArray<FCommandAlert>();
    if (Alerts.IsEmpty())
    {
        UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>();
        Empty->SetText(ProductText::Source(TEXT("尚未收到告警（本会话）")));
        CommandTheme::Text(Empty, 13, CommandTheme::SecondaryText);
        Rows->AddChild(Empty);
    }
    for (int32 Index = Alerts.Num() - 1; Index >= 0; --Index)
    {
        const FCommandAlert& Alert = Alerts[Index];
        UVerticalBox* Row = WidgetTree->ConstructWidget<UVerticalBox>();
        // Severity is presentation derived from the existing event type; the store remains unchanged.
        const bool bCritical = Alert.Type == TEXT("lost_connection") || Alert.Type == TEXT("critical");
        const bool bWarning = Alert.Type == TEXT("low_battery") || Alert.Type == TEXT("warning");
        const FLinearColor SeverityColor = bCritical ? CommandTheme::Critical : (bWarning ? CommandTheme::Warning : CommandTheme::Cyan);
        UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
        CommandTheme::Panel(Card, CommandTheme::Elevated, Alert.bHandled ? CommandTheme::Border : SeverityColor);
        Card->SetPadding(FMargin(10, 8));
        Card->SetContent(Row);
        UTextBlock* Severity = WidgetTree->ConstructWidget<UTextBlock>();
        CommandTheme::Text(Severity, 12, SeverityColor, true);
        Severity->SetText(FText::Format(ProductText::Get(TEXT("Alarm.Severity")),
            ProductText::Get(bCritical?TEXT("Alarm.Critical"):bWarning?TEXT("Alarm.Warning"):TEXT("Alarm.Info")),
            FText::AsCultureInvariant(Alert.ReceivedAt.ToString(TEXT("%H:%M:%S"))),Alert.bHandled?ProductText::Get(TEXT("Alarm.Handled")):FText::GetEmpty()));
        Row->AddChild(Severity);
        UTextBlock* Message = WidgetTree->ConstructWidget<UTextBlock>();
        Message->SetAutoWrapText(true);
        CommandTheme::Text(Message, 13, Alert.bHandled ? CommandTheme::SecondaryText : CommandTheme::PrimaryText);
        Message->SetText(ProductText::Source(Alert.Message));
        Row->AddChild(Message);
        UTextBlock* Target = WidgetTree->ConstructWidget<UTextBlock>();
        CommandTheme::Text(Target, 11, CommandTheme::SecondaryText);
        Target->SetAutoWrapText(true);
        Target->SetText(FText::Format(ProductText::Get(TEXT("Alarm.Target")),FText::AsNumber(Alert.DroneId),FText::AsCultureInvariant(Alert.Type)));
        Row->AddChild(Target);
        UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
        auto AddButton = [&](const TCHAR* Action, const TCHAR* Label, bool bEnabled)
        {
            UCommandActionButton* Button = WidgetTree->ConstructWidget<UCommandActionButton>();
            Button->Configure(FName(Action), Alert.Id);
            Button->OnAction.AddDynamic(this, &UCommandAlarmPanel::ActionRequested);
            Button->SetIsEnabled(bEnabled);
            UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
            Text->SetText(ProductText::Source(Label));
            CommandTheme::Text(Text, 11);
            Button->SetContent(Text);
            Actions->AddChild(Button);
        };
        AddButton(TEXT("open_video"), TEXT("OPEN VIDEO"), Alert.DroneId > 0);
        AddButton(TEXT("focus"), TEXT("选择无人机"), Alert.DroneId > 0);
        AddButton(TEXT("handled"), TEXT("标记已处理"), !Alert.bHandled);
        AddButton(TEXT("clear"), TEXT("移除记录"), true);
        Row->AddChild(Actions);
        CastChecked<UScrollBoxSlot>(Rows->AddChild(Card))->SetPadding(FMargin(0, 0, 0, 6));
    }
}
void UCommandAlarmPanel::ActionRequested(FName Action, int32 Id)
{
    if(Action==TEXT("open_video") && Store.IsValid()){for(const auto& Alert:Store->GetAlerts())if(Alert.Id==Id){GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>()->OpenVideo(FString::Printf(TEXT("UAV-%02d"),Alert.DroneId));break;}return;}
    if (Action == TEXT("focus"))
    {
        auto* Sync = GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
        if (Sync && Sync->IsEnabled() && Store.IsValid()) {
            for (const auto& Alert : Store->GetAlerts()) if (Alert.Id == Id) {
                Sync->SelectAlert(Alert.SharedId, Alert.DroneId); break;
            }
            return;
        }

        if (Map.IsValid()) Map->FocusAlertOnMap(Id);
        else if (Store.IsValid())
            for (const auto& Alert : Store->GetAlerts())
                if (Alert.Id == Id)
                {
                    auto* Registry = GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
                    if (Registry && Registry->IsDroneRegistered(Alert.DroneId))
                    {
                        Registry->SetPrimarySelectedDrone(Alert.DroneId);
                        Registry->SetMultiSelectedDrones({Alert.DroneId});
                    }
                    break;
                }
    }
    else if (Store.IsValid())
    {
        if (Action == TEXT("handled")) Store->MarkHandled(Id);
        if (Action == TEXT("clear")) Store->ClearAlert(Id);
    }
}
