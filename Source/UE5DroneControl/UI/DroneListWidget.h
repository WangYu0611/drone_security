// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ScrollBox.h"
#include "DroneListItemWidget.h"
#include "DroneOps/Core/DroneOpsTypes.h"
#include "DroneListWidget.generated.h"

class UDroneNetworkManager;
class UTextBlock;

// 🌟 1. 在类外面定义文档要求的“真数据结构体”，打通蓝图读取属性的通道
USTRUCT(BlueprintType)
struct FDroneRegistrationViewData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Drone Data")
    int32 Id = 0; //

    UPROPERTY(BlueprintReadOnly, Category = "Drone Data")
    FString IdStr; //

    UPROPERTY(BlueprintReadOnly, Category = "Drone Data")
    FString Name; //

    UPROPERTY(BlueprintReadOnly, Category = "Drone Data")
    FString Status;    // 后端返回的 online / connecting / lost / offline

    UPROPERTY(BlueprintReadOnly, Category = "Drone Data")
    int32 Battery = -1;  // 后端返回的电量

    UPROPERTY(BlueprintReadOnly, Category = "Drone Data")
    FVector WorldLocation = FVector::ZeroVector; // 物理坐标 x/y/z
};

/**
 * 无人机列表 C++ 基类
 * 蓝图只需继承，绑定 ScrollBox 即可
 */
UCLASS()
class UE5DRONECONTROL_API UDroneListWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void ConfigureCommandLayout();
    // 🌟 2. 新增蓝图事件：C++ 解析完后端 JSON 数组后，用这个事件把真数据直接扔给蓝图
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|Drone")
    void OnDroneDataReceived(const TArray<FDroneRegistrationViewData>& Drones);

    /** 滚动框 - 蓝图绑定到 ScrollBox */ //
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional)) //
        UScrollBox* DroneScrollBox = nullptr; //

    /** 列表项 Widget 类 - 在蓝图中设为 WBP_DroneListItem */ //
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") //
        TSubclassOf<UDroneListItemWidget> ListItemClass; //

    /** 刷新间隔（秒） */ //
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") //
        float RefreshInterval = 5.0f; //

    /** 添加无人机到列表 */ //
    UFUNCTION(BlueprintCallable, Category = "UI") //
        void AddDroneItem(const FString& Name, bool bOnline); //

    UFUNCTION(BlueprintCallable, Category = "UI")
        void RefreshFromRegistry();

    /** 清空列表 */ //
    UFUNCTION(BlueprintCallable, Category = "UI") //
        void ClearList(); //

    /** 演示数据：添加3架测试无人机 */ //
    UFUNCTION(BlueprintCallable, Category = "UI") //
        void AddDemoDrones(); //

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override; //
    virtual void NativeDestruct() override; //

private:
    bool bCommandLayout = false;
    FTimerHandle RefreshTimerHandle; //

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> IsolationStatusText = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UDroneNetworkManager> CachedNetworkManager = nullptr;

    void OnRefreshTimer(); //
    void BuildRuntimeWidgetTree();

    /**
     * DroneId -> 列表项映射，供事件驱动的就地更新使用。
     * RefreshFromRegistry 全量重建时填充，ClearList 时清空，避免野指针。
     */
    TMap<int32, TObjectPtr<UDroneListItemWidget>> ItemsByDroneId;

    /**
     * 注册表遥测更新回调：只就地刷新对应单项，不整表重建。
     * 未知 DroneId（尚未在列表中）忽略，交由 OnRegistryDroneRegistered / 定时器兜底补建。
     */
    UFUNCTION()
    void OnRegistryTelemetryUpdated(int32 InDroneId, const FDroneTelemetrySnapshot& Snapshot);

    /** 新无人机注册回调：低频事件，触发一次全量重建以插入新项。 */
    UFUNCTION()
    void OnRegistryDroneRegistered(int32 InDroneId);

    /** Refresh 按钮（BuildRuntimeWidgetTree 中创建） */
    UPROPERTY(meta = (BindWidgetOptional))
    class UButton* RefreshButton = nullptr;

    /** Enter/leave the single-drone video keyboard-control mode. */
    UPROPERTY(Transient)
    class UButton* VideoKeyboardControlButton = nullptr;

    UPROPERTY(Transient)
    class UTextBlock* VideoKeyboardControlLabel = nullptr;

    /** Refresh 操作状态文字（BuildRuntimeWidgetTree 中创建） */
    UPROPERTY(meta = (BindWidgetOptional))
    class UTextBlock* RefreshStatusText = nullptr;

    /** 严格本地预演模式：从 DroneNetworkManager 读取，true 时禁用 Refresh 按钮 */
    bool bStrictLocalPreview = false;

    /** 防止 Refresh 重复点击 */
    bool bRefreshing = false;

    /** 跨刷新持久化每架无人机的折叠状态（key = DroneId） */
    TMap<int32, bool> CollapseStates;

    /** Refresh 按钮点击回调 */
    UFUNCTION()
    void OnRefreshButtonClicked();

    UFUNCTION()
    void OnVideoKeyboardControlButtonClicked();

    /** 处理 POST /api/drones/refresh 响应 */
    void HandleRefreshResponse(bool bSuccess, const TArray<int32>& RefreshedIds);

    /** 隔离状态变化回调：Toggle 切换时同步禁用/启用 Refresh 按钮，并同步 IsolationStatusText 横幅 */
    UFUNCTION()
    void OnIsolationStateChanged(bool bIsolated);

    /** 标签设置变更时实时同步面板名称 & 颜色（无需整列表重建） */
    UFUNCTION()
    void OnLabelSettingsChanged(int32 InDroneId, const FDroneLabelSettings& Settings);

    // ---- 面板切换按钮 ----
    UPROPERTY(Transient)
    class UButton* ToggleDroneListButton = nullptr;

    UPROPERTY(Transient)
    class UButton* ToggleEnemyListButton = nullptr;

    /** 无人机态势面板边框（控制显示/隐藏） */
    UPROPERTY(Transient)
    class UBorder* DronePanelBorder = nullptr;

    /** 无人机态势面板的 Canvas slot（控制位置） */
    UPROPERTY(Transient)
    class UCanvasPanelSlot* DronePanelSlot = nullptr;

    bool bDronePanelVisible = true;
    bool bEnemyPanelVisible = false;

    UFUNCTION()
    void OnToggleDroneListClicked();

    UFUNCTION()
    void OnToggleEnemyListClicked();

    /** 弱引用敌对目标面板，由 PlayerController 在创建后注入 */
    TWeakObjectPtr<class UEnemyDroneListWidget> EnemyListWidgetRef;

public:
    /** 由外部（PlayerController）注入敌对目标面板引用 */
    void SetEnemyListWidget(class UEnemyDroneListWidget* Widget);
};

