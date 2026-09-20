#include "Command/CommandCenterPanel.h"
#include "Shared/ProductText.h"
#include "Shared/SecurityPlanWorkspaceWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Command/CommandTacticalMap.h"
#include "Components/VerticalBoxSlot.h"
#include "Command/CommandTheme.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"

void UCommandCenterPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Root = WidgetTree->ConstructWidget<UBorder>();
    CommandTheme::Panel(Root); Root->SetPadding(FMargin(12)); WidgetTree->RootWidget = Root;
    Content = WidgetTree->ConstructWidget<UVerticalBox>(); Root->SetContent(Content);
    Status = WidgetTree->ConstructWidget<UTextBlock>(); CommandTheme::Text(Status, 16);
    Status->SetAutoWrapText(true);
    PlanPanel=CreateWidget<USecurityPlanWorkspaceWidget>(GetOwningPlayer());
    Content->AddChildToVerticalBox(PlanPanel)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Refresh();
}
void UCommandCenterPanel::Refresh()
{
    auto* Sync = GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    if (!Status || !Sync) return;
    if (TacticalMap) TacticalMap->Refresh();
    if (PlanPanel) PlanPanel->Refresh();
    FString Mode = TEXT("UNKNOWN");
    if (Sync->GetContext()) Sync->GetContext()->TryGetStringField(TEXT("operation_mode"), Mode);
    TSet<FString> OnlineRoles;
    for (const auto& Entry : Sync->Clients)
    {
        FString Role, State;
        Entry.Value->TryGetStringField(TEXT("client_role"), Role);
        Entry.Value->TryGetStringField(TEXT("state"), State);
        if (State == TEXT("ONLINE")) OnlineRoles.Add(Role);
    }
    Status->SetText(FText::Format(ProductText::Get(TEXT("Command.Header")),ProductText::Source(Mode),FText::AsCultureInvariant(Sync->GetStatusText()),FText::AsNumber(OnlineRoles.Num())));
}

FString UCommandCenterPanel::GetHeaderText() const { return Status ? Status->GetText().ToString() : FString(); }
