// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UIManagerBlueprintLibrary.generated.h"

/**
 * UI 管理器蓝图函数库 - 所有 UI 操作的统一入口
 */
UCLASS()
class UE5DRONECONTROL_API UUIManagerBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** 显示 Toast 消息 */
    UFUNCTION(BlueprintCallable, Category = "UI Manager", meta = (WorldContext = "WorldContextObject"))
    static void ShowToast(UObject* WorldContextObject, const FString& Message, float Duration = 2.0f);

    /** 显示无人机列表面板 */
    UFUNCTION(BlueprintCallable, Category = "UI Manager", meta = (WorldContext = "WorldContextObject"))
    static void ShowDroneList(UObject* WorldContextObject);

    /** 隐藏无人机列表面板 */
    UFUNCTION(BlueprintCallable, Category = "UI Manager", meta = (WorldContext = "WorldContextObject"))
    static void HideDroneList(UObject* WorldContextObject);

    /** 显示集结进度弹窗并开始自动演示 */
    UFUNCTION(BlueprintCallable, Category = "UI Manager", meta = (WorldContext = "WorldContextObject"))
    static void ShowAssemblyDemo(UObject* WorldContextObject, int32 TotalCount = 3, float StepInterval = 1.0f);

    /** 显示序列派发面板 */
    UFUNCTION(BlueprintCallable, Category = "UI Manager", meta = (WorldContext = "WorldContextObject"))
    static void ShowSequenceDispatchPanel(UObject* WorldContextObject);

    /** 隐藏序列派发面板 */
    UFUNCTION(BlueprintCallable, Category = "UI Manager", meta = (WorldContext = "WorldContextObject"))
    static void HideSequenceDispatchPanel(UObject* WorldContextObject);

    /** 显示经纬高目标派发面板 */
    UFUNCTION(BlueprintCallable, Category = "UI Manager", meta = (WorldContext = "WorldContextObject"))
    static void ShowGeographicTargetPanel(UObject* WorldContextObject);

    /** 隐藏经纬高目标派发面板 */
    UFUNCTION(BlueprintCallable, Category = "UI Manager", meta = (WorldContext = "WorldContextObject"))
    static void HideGeographicTargetPanel(UObject* WorldContextObject);

    /** 显示敌对目标列表面板 */
    UFUNCTION(BlueprintCallable, Category = "UI Manager", meta = (WorldContext = "WorldContextObject"))
    static void ShowEnemyDroneList(UObject* WorldContextObject);

    /** 隐藏敌对目标列表面板 */
    UFUNCTION(BlueprintCallable, Category = "UI Manager", meta = (WorldContext = "WorldContextObject"))
    static void HideEnemyDroneList(UObject* WorldContextObject);

    /** 把 CurrentEnemyDroneList 注入 CurrentDroneList，供切换按钮使用。
     *  在 ShowDroneList 和 ShowEnemyDroneList 都调用后调用一次即可。 */
    static void LinkPanels();


    /** 显示垂直射线疏密与清理控制面板 */
    UFUNCTION(BlueprintCallable, Category = "UI Manager", meta = (WorldContext = "WorldContextObject"))
    static void ShowGroundProjectionSettings(UObject* WorldContextObject);

    /** 隐藏垂直射线疏密与清理控制面板 */
    UFUNCTION(BlueprintCallable, Category = "UI Manager", meta = (WorldContext = "WorldContextObject"))
    static void HideGroundProjectionSettings(UObject* WorldContextObject);

private:
    // 缓存当前显示的Widget引用
    static TWeakObjectPtr<class UDroneListWidget> CurrentDroneList;
    static TWeakObjectPtr<class UToastWidget> CurrentToast;
    static TWeakObjectPtr<class UAssemblyPopupWidget> CurrentAssemblyPopup;
    static TWeakObjectPtr<class USequenceDispatchPanelWidget> CurrentSequenceDispatchPanel;
    static TWeakObjectPtr<class UGeographicTargetPanelWidget> CurrentGeographicTargetPanel;
    static TWeakObjectPtr<class UEnemyDroneListWidget> CurrentEnemyDroneList;
    static TWeakObjectPtr<class UGroundProjectionSettingsWidget> CurrentGroundProjectionSettings;

    // Widget 类引用（自动加载）
    static TSubclassOf<class UDroneListWidget> GetDroneListClass();
    static TSubclassOf<class UToastWidget> GetToastClass();
    static TSubclassOf<class UAssemblyPopupWidget> GetAssemblyPopupClass();
    static TSubclassOf<class USequenceDispatchPanelWidget> GetSequenceDispatchPanelClass();
    static TSubclassOf<class UGeographicTargetPanelWidget> GetGeographicTargetPanelClass();
    static TSubclassOf<class UGroundProjectionSettingsWidget> GetGroundProjectionSettingsClass();
};
