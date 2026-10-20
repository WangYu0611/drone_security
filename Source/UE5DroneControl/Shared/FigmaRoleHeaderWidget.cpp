#include "Shared/FigmaRoleHeaderWidget.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Shared/ProductText.h"
#include "Command/CommandScreenManager.h"
#include "Command/FigmaCommandStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

namespace {
void Place(UCanvasPanel* Parent,UWidget* Child,FVector2D Position,FVector2D Size) {
    auto* Slot=Parent->AddChildToCanvas(Child);Slot->SetPosition(Position);Slot->SetSize(Size);
}
}

void UFigmaRoleHeaderWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();
    auto* Bounds=WidgetTree->ConstructWidget<USizeBox>();Bounds->SetWidthOverride(1440);Bounds->SetHeightOverride(90);WidgetTree->RootWidget=Bounds;
    auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>();Bounds->SetContent(Root);
    auto Label=[&](const FText& Text,int32 Size) {
        auto* L=WidgetTree->ConstructWidget<UTextBlock>();L->SetText(Text);FigmaUI::Text(L,Size);
        L->SetVisibility(ESlateVisibility::HitTestInvisible);return L;
    };
    // Coordinates are normalized from the Map/Video 16:9 Figma frames.
    Place(Root,FigmaUI::Icon(WidgetTree,TEXT("brand-mark"),{44,26}),{30,20},{44,25.5});
    Place(Root,FigmaUI::Icon(WidgetTree,TEXT("brand-wordmark"),{180,17}),{83,26},{180,16.1});
    auto* Divider=WidgetTree->ConstructWidget<UBorder>();Divider->SetBrushColor(FigmaUI::Muted);
    Divider->SetVisibility(ESlateVisibility::HitTestInvisible);Place(Root,Divider,{279,20},{1,26});
    Identity=Label(FText::GetEmpty(),14);Place(Root,Identity,{297,24},{250,26});
    auto* Languages=WidgetTree->ConstructWidget<UBorder>();
    FigmaUI::Panel(Languages,FigmaUI::Surface,FLinearColor::Transparent,24,FMargin(0));
    auto* Buttons=WidgetTree->ConstructWidget<UHorizontalBox>();Languages->SetContent(Buttons);
    Place(Root,Languages,{1071,18},{180,35});
    ChineseButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("HeaderChinese"));
    EnglishButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("HeaderEnglish"));
    ChineseButton->SetContent(Label(ProductText::Get(TEXT("Workflow.Chinese")),14));
    EnglishButton->SetContent(Label(ProductText::Get(TEXT("Workflow.English")),14));
    for(auto* Button:{ChineseButton.Get(),EnglishButton.Get()}) {
        auto* LanguageSlot=Buttons->AddChildToHorizontalBox(Button);LanguageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }
    ChineseButton->OnClicked.AddDynamic(this,&UFigmaRoleHeaderWidget::Chinese);
    EnglishButton->OnClicked.AddDynamic(this,&UFigmaRoleHeaderWidget::English);
    Clock=Label(FText::GetEmpty(),13);Clock->SetJustification(ETextJustify::Right);
    Place(Root,Clock,{1260,25},{150,26});
    auto* Health=WidgetTree->ConstructWidget<UHorizontalBox>();Place(Root,Health,{30,66},{1035,23});
    OnlineDot=FigmaUI::Icon(WidgetTree,TEXT("status-online"),{9,9});
    OfflineDot=FigmaUI::Icon(WidgetTree,TEXT("status-offline"),{9,9});
    for(int32 I=0;I<6;++I) {
        auto* Dot=FigmaUI::Icon(WidgetTree,TEXT("status-offline"),{9,9});
        auto* DS=Health->AddChildToHorizontalBox(Dot);DS->SetVerticalAlignment(VAlign_Center);DS->SetPadding(FMargin(0,0,7,0));HealthDots.Add(Dot);
        auto* Text=Label(FText::GetEmpty(),12);HealthLabels.Add(Text);
        auto* TS=Health->AddChildToHorizontalBox(Text);TS->SetVerticalAlignment(VAlign_Center);TS->SetPadding(FMargin(0,0,20,0));
    }
    Feedback=Label(FText::GetEmpty(),12);FigmaUI::Text(Feedback,12,FigmaUI::Critical);
    Feedback->SetJustification(ETextJustify::Right);Place(Root,Feedback,{1070,65},{340,24});
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Refresh();
}
void UFigmaRoleHeaderWidget::NativeTick(const FGeometry& Geometry,float Delta) {
    Super::NativeTick(Geometry,Delta);
    RefreshElapsed+=Delta;
    if(RefreshElapsed>=0.25f){RefreshElapsed=0.f;Refresh();}
}
void UFigmaRoleHeaderWidget::Refresh() {
    if(!Identity || !GetGameInstance())return;
    const auto Role=UCommandScreenManager::ResolveClientRole();
    const FString RoleKey=Role==EDroneClientRole::Command?TEXT("Stage1.Command"):Role==EDroneClientRole::Map?TEXT("Stage1.Map"):TEXT("Stage1.Video");
    Identity->SetText(ProductText::Get(RoleKey));
    Clock->SetText(FText::AsCultureInvariant(FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M"))));
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    if(!Sync)return;
    const TCHAR* Roles[]={TEXT("Command"),TEXT("Map"),TEXT("Video")};
    for(int32 I=0;I<6;++I) {
        const bool Online=I==0?Sync->GetSystemState()==TEXT("READY"):I<4?Sync->IsRoleOnline(Roles[I-1]):I==4?Sync->IsReady():Sync->IsHydrated();
        FText Text;
        if(I==0)Text=ProductText::Get(TEXT("Stage1.System.")+Sync->GetSystemState());
        else if(I<4)Text=FText::Format(ProductText::Get(TEXT("Figma.RoleHealth")),ProductText::Get(TEXT("Stage1.")+FString(Roles[I-1])),ProductText::Get(Online?TEXT("Stage1.Online"):TEXT("Stage1.Offline")));
        else Text=ProductText::Get(I==4?(Online?TEXT("Figma.Header.BackendOnline"):TEXT("Figma.Header.BackendOffline")):(Online?TEXT("Figma.Header.Synced"):TEXT("Figma.Header.SyncPending")));
        HealthLabels[I]->SetText(Text);
        HealthDots[I]->SetBrush(Online?OnlineDot->GetBrush():OfflineDot->GetBrush());
        HealthDots[I]->SetToolTipText(Text);
    }
    FString Language=TEXT("en");
    if(Sync->GetUIPreferences())Sync->GetUIPreferences()->TryGetStringField(TEXT("language"),Language);
    ChineseButton->SetIsEnabled(Sync->IsReady());EnglishButton->SetIsEnabled(Sync->IsReady());
    FigmaUI::Button(ChineseButton,Language==TEXT("zh-Hans"),true);
    FigmaUI::Button(EnglishButton,Language==TEXT("en"),true);
    Feedback->SetText(Sync->ViewSyncError.IsEmpty()?FText::GetEmpty():ProductText::Get(TEXT("Errors.")+Sync->ViewSyncError));
}
void UFigmaRoleHeaderWidget::Chinese() {
    if(auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>())Sync->SetLanguage(TEXT("zh-Hans"));
}
void UFigmaRoleHeaderWidget::English() {
    if(auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>())Sync->SetLanguage(TEXT("en"));
}
