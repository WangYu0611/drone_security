// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DroneOps/Core/DroneOpsTypes.h"
#include "DroneOps/Drone/DroneModelTypes.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "DroneListItemWidget.generated.h"

/**
 * 无人机列表项 C++ 基类
 * 蓝图只需继承此类，绑定 UI 变量即可，无需写逻辑
 */
UCLASS()
class UE5DRONECONTROL_API UDroneListItemWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void ConfigureCommandSelection();
    /** 无人机名称 - 蓝图绑定到 Text */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* DroneNameText = nullptr;

    /** 状态颜色 - 蓝图绑定到 Border 的 Color And Opacity */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UBorder* StatusIndicator = nullptr;

    /** 状态文字 - 蓝图绑定到 Text */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* StatusText = nullptr;

    /** 当前移动模式 - 蓝图绑定到 Text */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* ModeText = nullptr;

    /** 模式切换按钮 - 蓝图绑定到 Button */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    class UButton* ModeButton = nullptr;

    // ==================== 新增控件 ====================
    /** 【新增】连接状态（独立显示）- 显示"在线/离线/失联" */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* ConnectionStatusText = nullptr;

    /** 【新增】任务状态（独立显示）- 显示"待命/巡逻中/攻击中/暂停/异常…" */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* TaskStatusText = nullptr;

    /** 【新增】GPS经纬度 - 显示"经度: xxx.xxxxxx  纬度: xxx.xxxxxx" */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* GpsText = nullptr;

    /** 【新增】海拔 - 显示"海拔: xxx.x m" */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* AltitudeText = nullptr;

    /** 【新增】电量 - 显示"xx%"或"未知" */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* BatteryText = nullptr;

    /** 【新增】航点进度 - 显示"航点: 3 / 10" */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* WaypointText = nullptr;

    /** 【新增】当前目标点编号 - 显示"目标点: 3" */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* TargetIndexText = nullptr;

    /** 【新增】最近更新时间 - 显示"2.5s 前" */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* UpdateTimeText = nullptr;

    /** 状态数据来源：后端同步或 UE 本地预演。 */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* StatusSourceText = nullptr;

    /** 【新增】无人机ID - 显示"ID: x"，位于模式行上方，在可收起区域内 */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* DroneIdText = nullptr;

    /** 【新增】纬度 - 单独一行，与 GpsText（经度行）分开显示 */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* LatitudeText = nullptr;

    /**
     * 外观模型下拉框（原始模型 / 军用机 / 警用机 / 科幻机）。
     * 蓝图未绑定时由 BuildRuntimeCard 创建并挂到可折叠区域。
     */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    class UComboBoxString* ModelComboBox = nullptr;

    /** 设置无人机数据 */
    UFUNCTION(BlueprintCallable, Category = "UI")
    void SetDroneData(const FString& Name, bool bOnline);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void SetDroneDataWithAvailability(const FString& Name, EDroneAvailability Availability);

    /** 设置真实无人机数据，并显示/切换当前指令模式 */
    UFUNCTION(BlueprintCallable, Category = "UI")
    void SetDroneDataWithMode(int32 InDroneId, const FString& Name, bool bOnline, EDroneCommandMode InMode);

    UFUNCTION(BlueprintCallable, Category = "UI")
    void SetDroneDataWithModeAndAvailability(int32 InDroneId, const FString& Name, EDroneAvailability Availability, EDroneCommandMode InMode);

    // ==================== 新增方法 ====================
    /** 【新增】完整状态设置方法：一次性设置连接状态、GPS、电量、任务状态、航点、更新时间（需求1全部10项 + 需求3/4状态覆盖） */
    UFUNCTION(BlueprintCallable, Category = "UI")
    void SetDroneFullState(int32 InDroneId, const FDroneTelemetrySnapshot& Snapshot);

    /** 只更新 DroneId 和任务模式显示，不覆盖连接状态等其他字段 */
    void SetCommandMode(int32 InDroneId, EDroneCommandMode InMode);

    /**
     * 绑定该条目对应的无人机ID，订阅 OnMultiSelectionChanged 委托，
     * 并立即同步一次选中按钮状态。
     * 须在 SetDroneFullState 之后调用（依赖 DroneId 已被设置）。
     */
    void SetDroneId(int32 InDroneId);
    UFUNCTION() void OnCommandPrimaryChanged(int32 OldId, int32 NewId);

    int32 GetDroneId() const { return DroneId; }
    bool IsCollapsed() const { return bIsCollapsed; }
    void SetCollapsed(bool bCollapsed) { bIsCollapsed = bCollapsed; ApplyCollapseState(); }

    /**
     * 隐藏/显示模式+任务状态行（TaskRow）及状态来源行。
     * 可在 widget 加入 ScrollBox 前或之后调用，BuildRuntimeCard 已将 TaskRow 指针保存。
     */
    void SetHideTaskRow(bool bHide);

    /**
     * 同步无人机的顶部标签颜色到面板名称颜色显示。
     * 由 DroneListWidget 在收到 OnDroneLabelSettingsChanged 广播后调用。
     */
    void ApplyLabelSettings(const FDroneLabelSettings& Settings);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    bool bCommandSelection = false;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CommandSummary;
    void RefreshCommandSummary();
    int32 DroneId = 0;
    EDroneCommandMode CurrentMode = EDroneCommandMode::Move;
    bool bIsCollapsed = false;
    bool bHideTaskRow = false;

    /** 模式+任务状态行（BuildRuntimeCard 创建，SetHideTaskRow 可折叠它） */
    UPROPERTY(meta = (BindWidgetOptional))
    class UHorizontalBox* TaskRow = nullptr;

    /** 选中/取消选中按钮（运行时由 BuildRuntimeCard 创建） */
    UPROPERTY(meta = (BindWidgetOptional))
    class UButton* SelectButton = nullptr;

    /** 选中按钮上的文字 */
    UPROPERTY(meta = (BindWidgetOptional))
    class UTextBlock* SelectButtonText = nullptr;

    /** 折叠/展开按钮（Header 行右侧） */
    UPROPERTY(meta = (BindWidgetOptional))
    class UButton* CollapseButton = nullptr;

    /** 折叠按钮上的箭头文字（▼ / ▶） */
    UPROPERTY(meta = (BindWidgetOptional))
    class UTextBlock* CollapseButtonText = nullptr;

    /**
     * 点击"名称"区域弹出标签编辑弹窗。
     * BuildRuntimeCard 创建为透明按钮覆盖在 DroneNameText 上方。
     */
    UPROPERTY(meta = (BindWidgetOptional))
    class UButton* NameLabelButton = nullptr;

    /** 可折叠区域容器（模式行 → TargetIndexText，不含选中按钮） */
    UPROPERTY(meta = (BindWidgetOptional))
    UVerticalBox* CollapsibleContent = nullptr;

    /** 根据注册表当前状态刷新 SelectButton 的文字和启用状态（须为 UFUNCTION 以绑定 Dynamic 委托） */
    UFUNCTION()
    void UpdateSelectionState();

    UFUNCTION()
    void OnSelectButtonClicked();

    UFUNCTION()
    void OnModeButtonClicked();

    UFUNCTION()
    void OnCollapseButtonClicked();

    /** 点击名称按钮 → 弹出标签编辑弹窗 */
    UFUNCTION()
    void OnNameLabelButtonClicked();

    /** 标签编辑弹窗"确认"回调 */
    UFUNCTION()
    void OnLabelEditConfirmed(int32 InDroneId, const FString& NewName,
        FLinearColor NewColor, int32 NewFontSize);

    /** 用户在下拉框中选择了另一个外观模型 → 写入 Registry。 */
    UFUNCTION()
    void OnModelComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    /** Registry 广播模型变更 → 同步下拉框显示，避免与实际不一致。 */
    UFUNCTION()
    void OnRegistryModelTypeChanged(int32 InDroneId, EDroneModelType ModelType);

    /** 依据 Registry 当前值刷新下拉框选中项。 */
    void SyncModelComboSelection();

    /**
     * true 时忽略 OnModelComboChanged。
     * SetSelectedOption() 会以 ESelectInfo::Direct 触发回调，
     * 若不屏蔽会把"同步显示"误当成"用户改动"再写回 Registry。
     */
    bool bSuppressModelComboCallback = false;

    void UpdateModeText();
    void ApplyCollapseState();

    /**
     * 当蓝图未绑定新增控件时，在代码里创建并挂载。
     * 只对指针仍为 nullptr 的字段创建控件，不覆盖已绑定的控件。
     */
    void BuildRuntimeCard();
};
