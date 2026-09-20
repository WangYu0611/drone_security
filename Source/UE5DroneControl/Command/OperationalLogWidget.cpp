#include "Command/OperationalLogWidget.h"
#include "Shared/ProductText.h"
#include "Command/CommandTheme.h"
#include "Shared/OperationalEventStore.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Engine/GameInstance.h"
void UOperationalLogWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Root=WidgetTree->ConstructWidget<UBorder>();CommandTheme::Panel(Root);Root->SetPadding(FMargin(14));WidgetTree->RootWidget=Root;
    auto* Layout=WidgetTree->ConstructWidget<UVerticalBox>();Root->SetContent(Layout);
    auto* Bar=WidgetTree->ConstructWidget<UHorizontalBox>();Layout->AddChild(Bar);
    Counts=WidgetTree->ConstructWidget<UTextBlock>();CommandTheme::Text(Counts,16);
    Bar->AddChildToHorizontalBox(Counts)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Filter=WidgetTree->ConstructWidget<UComboBoxString>();
    Filter->OnGenerateWidgetEvent.BindDynamic(this,&UOperationalLogWidget::FilterLabel);
    auto FilterRows=Filter->GetItemStyle();FilterRows.SetTextColor(CommandTheme::PrimaryText).SetSelectedTextColor(CommandTheme::PrimaryText);Filter->SetItemStyle(FilterRows);
    for(const TCHAR* Category:{TEXT("ALL"),TEXT("SYSTEM"),TEXT("OPERATION"),TEXT("ALERT"),TEXT("MISSION"),TEXT("PLAN")})Filter->AddOption(Category);
    Filter->SetSelectedOption(TEXT("ALL"));Filter->OnSelectionChanged.AddDynamic(this,&UOperationalLogWidget::FilterChanged);Bar->AddChild(Filter);
    auto* Clear=WidgetTree->ConstructWidget<UButton>();CommandTheme::Button(Clear,false,true);
    auto* Text=WidgetTree->ConstructWidget<UTextBlock>();Text->SetText(ProductText::Source(TEXT("CLEAR DISPLAY")));CommandTheme::Text(Text,13);Clear->SetContent(Text);
    Clear->OnClicked.AddDynamic(this,&UOperationalLogWidget::ClearDisplay);Bar->AddChildToHorizontalBox(Clear)->SetPadding(FMargin(12,0,0,0));
    Scroll=WidgetTree->ConstructWidget<UScrollBox>();Layout->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Rows=WidgetTree->ConstructWidget<UTextBlock>();CommandTheme::Text(Rows,15);Rows->SetAutoWrapText(true);Scroll->AddChild(Rows);
    Handle=GetGameInstance()->GetSubsystem<UOperationalEventStore>()->OnChanged.AddUObject(this,&UOperationalLogWidget::Refresh);Refresh();
}
void UOperationalLogWidget::NativeDestruct()
{
    if(GetGameInstance())GetGameInstance()->GetSubsystem<UOperationalEventStore>()->OnChanged.Remove(Handle);
    Super::NativeDestruct();
}
void UOperationalLogWidget::ClearDisplay(){ClearedThrough=GetGameInstance()->GetSubsystem<UOperationalEventStore>()->GetLatestSequence();Refresh();}
void UOperationalLogWidget::FilterChanged(FString,ESelectInfo::Type){Refresh();}
void UOperationalLogWidget::Refresh()
{
    if(!Rows || !Filter)return;
    FString Lines;int32 Critical=0,Warning=0;
    for(const auto& E:GetGameInstance()->GetSubsystem<UOperationalEventStore>()->GetEvents())
    {
        if(E.Sequence<=ClearedThrough)continue;
        Critical+=E.Level==EOperationalLevel::CRITICAL;Warning+=E.Level==EOperationalLevel::WARNING;
        const FString Category=StaticEnum<EOperationalCategory>()->GetNameStringByValue(static_cast<int64>(E.Category));
        if(Filter->GetSelectedOption()!=TEXT("ALL") && Filter->GetSelectedOption()!=Category)continue;
        Lines+=FString::Printf(TEXT("%s   %-10s   %s\n"),*E.Timestamp.ToString(TEXT("%H:%M:%S")),*ProductText::Source(Category).ToString(),*E.DisplayText().ToString());
    }
    Counts->SetText(FText::Format(ProductText::Get(TEXT("Log.Count")),FText::AsNumber(Critical),FText::AsNumber(Warning)));
    Rows->SetText(ProductText::Source(Lines.IsEmpty()?TEXT("No events in this view"):Lines));Scroll->ScrollToEnd();
}

UWidget* UOperationalLogWidget::FilterLabel(FString Value){auto* L=WidgetTree->ConstructWidget<UTextBlock>();L->SetText(ProductText::Source(Value));CommandTheme::Text(L,13);return L;}
