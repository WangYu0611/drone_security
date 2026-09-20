// Copyright Epic Games, Inc. All Rights Reserved.

#include "UIManagerBlueprintLibrary.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandShellWidget.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "DroneListWidget.h"
#include "EnemyDroneListWidget.h"
#include "ToastWidget.h"
#include "AssemblyPopupWidget.h"
#include "SequenceDispatchPanelWidget.h"
#include "GeographicTargetPanelWidget.h"
#include "GroundProjectionSettingsWidget.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

// 静态成员初始化
namespace
{
UCommandScreenManager* CommandManager(UObject* Context)
{
    UWorld* World = Context ? Context->GetWorld() : nullptr;
    ADroneOpsPlayerController* PC = Cast<ADroneOpsPlayerController>(Context);
    if (!PC && World) PC = Cast<ADroneOpsPlayerController>(World->GetFirstPlayerController());
    UCommandScreenManager* Manager = PC ? PC->GetCommandScreenManager() : nullptr;
    return Manager && (Manager->GetShell() || Manager->GetMapShell()) ? Manager : nullptr;
}
}
TWeakObjectPtr<UDroneListWidget> UUIManagerBlueprintLibrary::CurrentDroneList;
TWeakObjectPtr<UEnemyDroneListWidget> UUIManagerBlueprintLibrary::CurrentEnemyDroneList;
TWeakObjectPtr<UToastWidget> UUIManagerBlueprintLibrary::CurrentToast;
TWeakObjectPtr<UAssemblyPopupWidget> UUIManagerBlueprintLibrary::CurrentAssemblyPopup;
TWeakObjectPtr<USequenceDispatchPanelWidget> UUIManagerBlueprintLibrary::CurrentSequenceDispatchPanel;
TWeakObjectPtr<UGeographicTargetPanelWidget> UUIManagerBlueprintLibrary::CurrentGeographicTargetPanel;
TWeakObjectPtr<UGroundProjectionSettingsWidget> UUIManagerBlueprintLibrary::CurrentGroundProjectionSettings;

TSubclassOf<UDroneListWidget> UUIManagerBlueprintLibrary::GetDroneListClass()
{
    UClass* Class = LoadClass<UDroneListWidget>(nullptr, TEXT("/Game/DroneOps/UI/WBP_DroneList.WBP_DroneList_C"));
    return Class ? Class : UDroneListWidget::StaticClass();
}

TSubclassOf<UToastWidget> UUIManagerBlueprintLibrary::GetToastClass()
{
    UClass* Class = LoadClass<UToastWidget>(nullptr, TEXT("/Game/DroneOps/UI/WBP_Toast.WBP_Toast_C"));
    return Class ? Class : UToastWidget::StaticClass();
}

TSubclassOf<UAssemblyPopupWidget> UUIManagerBlueprintLibrary::GetAssemblyPopupClass()
{
    UClass* Class = LoadClass<UAssemblyPopupWidget>(nullptr, TEXT("/Game/DroneOps/UI/WBP_AssemblyPopup.WBP_AssemblyPopup_C"));
    return Class ? Class : UAssemblyPopupWidget::StaticClass();
}

void UUIManagerBlueprintLibrary::ShowToast(UObject* WorldContextObject, const FString& Message, float Duration)
{
    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return;

    // 关闭上一条还在显示的 Toast，避免叠加
    if (CurrentToast.IsValid())
    {
        CurrentToast->RemoveFromParent();
    }

    UToastWidget* Toast = CreateWidget<UToastWidget>(World, GetToastClass());
    if (Toast)
    {
        Toast->SetMessage(Message);
        Toast->AutoCloseDuration = Duration;
        Toast->AddToViewport();
        Toast->ShowAndAutoClose();
        CurrentToast = Toast;
        UE_LOG(LogTemp, Log, TEXT("UIManager: Toast shown - %s"), *Message);
    }
}

void UUIManagerBlueprintLibrary::ShowDroneList(UObject* WorldContextObject)
{
    if (UCommandScreenManager::ResolveClientRole() == EDroneClientRole::Video) return;
    if (UCommandScreenManager* Manager = CommandManager(WorldContextObject)) { Manager->ShowDroneList(true); return; }
    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return;

    if (CurrentDroneList.IsValid())
    {
        CurrentDroneList->SetVisibility(ESlateVisibility::Visible);
        UE_LOG(LogTemp, Log, TEXT("UIManager: DroneList already exists, shown"));
        return;
    }

    UDroneListWidget* DroneList = CreateWidget<UDroneListWidget>(World, GetDroneListClass());
    if (DroneList)
    {
        if (!DroneList->ListItemClass)
        {
            UClass* ItemClass = LoadClass<UDroneListItemWidget>(nullptr, TEXT("/Game/DroneOps/UI/WBP_DroneListItem.WBP_DroneListItem_C"));
            if (ItemClass)
            {
                DroneList->ListItemClass = ItemClass;
                UE_LOG(LogTemp, Log, TEXT("UIManager: Auto-set ListItemClass"));
            }
        }
        DroneList->AddToViewport();
        CurrentDroneList = DroneList;
        UE_LOG(LogTemp, Log, TEXT("UIManager: DroneList created and shown"));
    }
}

void UUIManagerBlueprintLibrary::HideDroneList(UObject* WorldContextObject)
{
    if (UCommandScreenManager* Manager = CommandManager(WorldContextObject)) { Manager->ShowDroneList(false); return; }
    if (CurrentDroneList.IsValid())
    {
        CurrentDroneList->SetVisibility(ESlateVisibility::Hidden);
        UE_LOG(LogTemp, Log, TEXT("UIManager: DroneList hidden"));
    }
}

void UUIManagerBlueprintLibrary::ShowAssemblyDemo(UObject* WorldContextObject, int32 TotalCount, float StepInterval)
{
    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return;

    UAssemblyPopupWidget* Popup = CreateWidget<UAssemblyPopupWidget>(World, GetAssemblyPopupClass());
    if (Popup)
    {
        Popup->TotalCount = TotalCount;
        Popup->AddToViewport();
        Popup->StartAutoDemo(StepInterval);
        CurrentAssemblyPopup = Popup;
        UE_LOG(LogTemp, Log, TEXT("UIManager: Assembly demo started, TotalCount=%d"), TotalCount);
    }
}

TSubclassOf<USequenceDispatchPanelWidget> UUIManagerBlueprintLibrary::GetSequenceDispatchPanelClass()
{
    UClass* Class = LoadClass<USequenceDispatchPanelWidget>(nullptr, TEXT("/Game/DroneOps/UI/WBP_SequenceDispatchPanel.WBP_SequenceDispatchPanel_C"));
    return Class ? Class : USequenceDispatchPanelWidget::StaticClass();
}

void UUIManagerBlueprintLibrary::ShowSequenceDispatchPanel(UObject* WorldContextObject)
{
    if (UCommandScreenManager::ResolveClientRole() == EDroneClientRole::Command) return;
    if (UCommandScreenManager::ResolveClientRole() == EDroneClientRole::Video) return;
    if (UCommandScreenManager* Manager = CommandManager(WorldContextObject)) { Manager->ShowPathPanel(); return; }
    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return;

    if (CurrentSequenceDispatchPanel.IsValid())
    {
        CurrentSequenceDispatchPanel->SetVisibility(ESlateVisibility::Visible);
        return;
    }

    USequenceDispatchPanelWidget* Panel = CreateWidget<USequenceDispatchPanelWidget>(World, GetSequenceDispatchPanelClass());
    if (Panel)
    {
        Panel->AddToViewport();
        CurrentSequenceDispatchPanel = Panel;
        UE_LOG(LogTemp, Log, TEXT("UIManager: SequenceDispatchPanel created"));
    }
}

void UUIManagerBlueprintLibrary::HideSequenceDispatchPanel(UObject* WorldContextObject)
{
    if (UCommandScreenManager* Manager = CommandManager(WorldContextObject)) { Manager->HidePathPanel(); return; }
    if (CurrentSequenceDispatchPanel.IsValid())
    {
        CurrentSequenceDispatchPanel->RemoveFromParent();
        CurrentSequenceDispatchPanel = nullptr;
    }
}

TSubclassOf<UGeographicTargetPanelWidget> UUIManagerBlueprintLibrary::GetGeographicTargetPanelClass()
{
    UClass* Class = LoadClass<UGeographicTargetPanelWidget>(nullptr, TEXT("/Game/DroneOps/UI/WBP_GeographicTargetPanel.WBP_GeographicTargetPanel_C"));
    return Class ? Class : UGeographicTargetPanelWidget::StaticClass();
}

void UUIManagerBlueprintLibrary::ShowGeographicTargetPanel(UObject* WorldContextObject)
{
    if (UCommandScreenManager::ResolveClientRole() == EDroneClientRole::Command) return;
    if (UCommandScreenManager::ResolveClientRole() == EDroneClientRole::Video) return;
    if (UCommandScreenManager* Manager = CommandManager(WorldContextObject)) { Manager->ShowGeographicPanel(); return; }
    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World) return;

    if (CurrentGeographicTargetPanel.IsValid())
    {
        CurrentGeographicTargetPanel->SetVisibility(ESlateVisibility::Visible);
        return;
    }

    APlayerController* OwningPlayer = World->GetFirstPlayerController();
    UGeographicTargetPanelWidget* Panel = OwningPlayer
        ? CreateWidget<UGeographicTargetPanelWidget>(OwningPlayer, GetGeographicTargetPanelClass())
        : CreateWidget<UGeographicTargetPanelWidget>(World, GetGeographicTargetPanelClass());
    if (Panel)
    {
        Panel->AddToViewport();
        CurrentGeographicTargetPanel = Panel;
        UE_LOG(LogTemp, Log, TEXT("UIManager: GeographicTargetPanel created"));
    }
}

void UUIManagerBlueprintLibrary::HideGeographicTargetPanel(UObject* WorldContextObject)
{
    if (UCommandScreenManager* Manager = CommandManager(WorldContextObject)) { Manager->HideGeographicPanel(); return; }
    if (CurrentGeographicTargetPanel.IsValid())
    {
        CurrentGeographicTargetPanel->RemoveFromParent();
        CurrentGeographicTargetPanel = nullptr;
    }
}

void UUIManagerBlueprintLibrary::ShowEnemyDroneList(UObject* WorldContextObject)
{
    if (UCommandScreenManager::ResolveClientRole() != EDroneClientRole::Standalone) return;
    if (UCommandScreenManager::ResolveClientRole() == EDroneClientRole::Video) return;
    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World) return;

    if (CurrentEnemyDroneList.IsValid() && CurrentEnemyDroneList->GetWorld() == World)
    {
        CurrentEnemyDroneList->SetVisibility(ESlateVisibility::Visible);
        return;
    }
    // 旧 Widget 属于其他 World（关卡切换后 GC 尚未回收），强制重建
    CurrentEnemyDroneList = nullptr;

    // 用传入的 WorldContextObject（本地 PC）作为 Outer，避免 UE 找到非本地 PC 报错
    APlayerController* LocalPC = Cast<APlayerController>(WorldContextObject);
    UEnemyDroneListWidget* Widget = LocalPC
        ? CreateWidget<UEnemyDroneListWidget>(LocalPC, UEnemyDroneListWidget::StaticClass())
        : CreateWidget<UEnemyDroneListWidget>(World, UEnemyDroneListWidget::StaticClass());
    if (Widget)
    {
        Widget->AddToViewport();
        CurrentEnemyDroneList = Widget;
        UE_LOG(LogTemp, Log, TEXT("UIManager: EnemyDroneList created and shown"));
    }
}

void UUIManagerBlueprintLibrary::HideEnemyDroneList(UObject* WorldContextObject)
{
    if (CurrentEnemyDroneList.IsValid())
    {
        CurrentEnemyDroneList->SetVisibility(ESlateVisibility::Hidden);
        UE_LOG(LogTemp, Log, TEXT("UIManager: EnemyDroneList hidden"));
    }
}

void UUIManagerBlueprintLibrary::LinkPanels()
{
    if (CurrentDroneList.IsValid() && CurrentEnemyDroneList.IsValid())
    {
        CurrentDroneList->SetEnemyListWidget(CurrentEnemyDroneList.Get());
    }
}


TSubclassOf<UGroundProjectionSettingsWidget> UUIManagerBlueprintLibrary::GetGroundProjectionSettingsClass()
{
    return UGroundProjectionSettingsWidget::StaticClass();
}

void UUIManagerBlueprintLibrary::ShowGroundProjectionSettings(UObject* WorldContextObject)
{
    if (UCommandScreenManager::ResolveClientRole() == EDroneClientRole::Command) return;
    if (UCommandScreenManager::ResolveClientRole() == EDroneClientRole::Video) return;
    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World) return;
    if (CurrentGroundProjectionSettings.IsValid())
    {
        CurrentGroundProjectionSettings->SetVisibility(ESlateVisibility::Visible);
        return;
    }
    APlayerController* OwningPlayer = World->GetFirstPlayerController();
    UGroundProjectionSettingsWidget* Panel = OwningPlayer
        ? CreateWidget<UGroundProjectionSettingsWidget>(OwningPlayer, GetGroundProjectionSettingsClass())
        : CreateWidget<UGroundProjectionSettingsWidget>(World, GetGroundProjectionSettingsClass());
    if (Panel)
    {
        Panel->AddToViewport(5);
        CurrentGroundProjectionSettings = Panel;
    }
}

void UUIManagerBlueprintLibrary::HideGroundProjectionSettings(UObject* WorldContextObject)
{
    if (CurrentGroundProjectionSettings.IsValid())
    {
        CurrentGroundProjectionSettings->RemoveFromParent();
        CurrentGroundProjectionSettings = nullptr;
    }
}
