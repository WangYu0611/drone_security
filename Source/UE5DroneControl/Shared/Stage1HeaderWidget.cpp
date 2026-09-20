#include "Shared/Stage1HeaderWidget.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Shared/ProductText.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandTheme.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"

void UStage1HeaderWidget::NativeOnInitialized() {
    Super::NativeOnInitialized();
    auto* Height=WidgetTree->ConstructWidget<USizeBox>();Height->SetHeightOverride(72);WidgetTree->RootWidget=Height;
    auto* Background=WidgetTree->ConstructWidget<UBorder>();CommandTheme::Panel(Background,CommandTheme::Background);
    Background->SetPadding(FMargin(12,5));Height->SetContent(Background);
    auto* Rows=WidgetTree->ConstructWidget<UVerticalBox>();Background->SetContent(Rows);
    auto Label=[&](int Size){auto* T=WidgetTree->ConstructWidget<UTextBlock>();CommandTheme::Text(T,Size);Rows->AddChild(T);return T;};
    Identity=Label(16);Health=Label(12);Detail=Label(10);
    SetVisibility(ESlateVisibility::HitTestInvisible);
}
void UStage1HeaderWidget::NativeTick(const FGeometry& Geometry,float Delta) {
    Super::NativeTick(Geometry,Delta);
    const auto Role=UCommandScreenManager::ResolveClientRole();
    const FString RoleKey=Role==EDroneClientRole::Command?TEXT("Stage1.Command"):Role==EDroneClientRole::Map?TEXT("Stage1.Map"):TEXT("Stage1.Video");
    Identity->SetText(FText::Format(ProductText::Get(TEXT("Stage1.Identity")),ProductText::Get(TEXT("Stage1.Name")),ProductText::Get(RoleKey)));
    auto* Sync=GetGameInstance()->GetSubsystem<UOperationalContextSubsystem>();
    if(!Sync)return;
    const FString State=Sync->GetSystemState();
    Health->SetText(FText::Format(ProductText::Get(TEXT("Stage1.Health")),ProductText::Get(TEXT("Stage1.System.")+State),
        ProductText::Get(Sync->IsRoleOnline(TEXT("Command"))?TEXT("Stage1.Online"):TEXT("Stage1.Offline")),
        ProductText::Get(Sync->IsRoleOnline(TEXT("Map"))?TEXT("Stage1.Online"):TEXT("Stage1.Offline")),
        ProductText::Get(Sync->IsRoleOnline(TEXT("Video"))?TEXT("Stage1.Online"):TEXT("Stage1.Offline"))));
    Health->SetColorAndOpacity(State==TEXT("READY")?CommandTheme::Online:State==TEXT("OFFLINE")?CommandTheme::Critical:CommandTheme::Warning);
    FString Language=TEXT("en");if(Sync->GetUIPreferences())Sync->GetUIPreferences()->TryGetStringField(TEXT("language"),Language);
    Detail->SetText(FText::Format(ProductText::Get(TEXT("Stage1.Detail")),FText::AsCultureInvariant(Language),
        FText::AsCultureInvariant(FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")))));
}
