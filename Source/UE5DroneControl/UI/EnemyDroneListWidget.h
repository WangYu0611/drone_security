// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DroneOps/Core/DroneOpsTypes.h"
#include "UI/DroneNameEditPopupWidget.h"
#include "EnemyDroneListWidget.generated.h"

/**
 * 敌对目标列表面板
 *
 * 独立于友方无人机态势面板，专用于显示 bIsEnemyTarget=true 的条目。
 * 每行只显示：目标名称 | 选中按钮 | 删除按钮
 * 不显示连接状态、GPS、任务模式等友方专属信息。
 */
UCLASS()
class UE5DRONECONTROL_API UEnemyDroneListWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 从 DroneRegistrySubsystem 读取所有 bIsEnemyTarget=true 的条目并刷新列表 */
    UFUNCTION(BlueprintCallable, Category = "UI|Enemy")
    void RefreshFromRegistry();

    /** 显示或隐藏敌对目标面板（只操作 Border，不影响根节点 hit-test） */
    void SetPanelVisible(bool bVisible);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    void BuildRuntimeWidgetTree();

    /** 若 NativeOnInitialized 里 WidgetTree 为 null 导致构建未完成，则在此补建（NativeConstruct 调用）*/
    void EnsureWidgetTreeBuilt();

    /** 列表滚动框 */
    UPROPERTY(meta = (BindWidgetOptional))
    class UScrollBox* EnemyScrollBox = nullptr;

    /** 面板 Border，用于在 NativeConstruct 里按地图名重新定位 */
    class UBorder* EnemyPanelBorder = nullptr;

    /** 面板 CanvasPanelSlot，用于在 NativeConstruct 里设定位置/锚点 */
    class UCanvasPanelSlot* EnemyPanelSlot = nullptr;

    /** 定时刷新 handle */
    FTimerHandle RefreshTimerHandle;

    void OnRefreshTimer();

    /** 响应新无人机注册事件，立即刷新列表（解决敌对目标注册晚于 Widget 初始化的时序问题） */
    UFUNCTION()
    void OnDroneRegisteredHandler(int32 DroneId);

    /** 构建单条敌对目标行并加入 EnemyScrollBox */
    void AddEnemyRow(int32 DroneId, const FString& Name);

    /** 删除按钮回调：按 DroneId 注销敌对目标 */
    UFUNCTION()
    void OnDeleteClicked();

    /** 选中按钮回调：按 DroneId 切换多选状态 */
    UFUNCTION()
    void OnSelectClicked();

    /**
     * 因 UButton::OnClicked 不传参，用独立 wrapper widget 承载每行的 DroneId。
     * 每次 AddEnemyRow 创建一个行 Widget 实例。
     */
    friend class UEnemyDroneRowWidget;
};

// ---------------------------------------------------------------------------
//  行 Widget（内联定义，避免额外文件）
// ---------------------------------------------------------------------------

UCLASS()
class UE5DRONECONTROL_API UEnemyDroneRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 初始化行数据，并创建 Name / Select / Delete 控件 */
    void InitRow(int32 InDroneId, const FString& InName);

    int32 GetDroneId() const { return DroneId; }

    /** 外部（EnemyDroneListWidget）调用以强制刷新选中按钮文字 */
    void RefreshSelectButton();

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    int32 DroneId = 0;

    UPROPERTY(meta = (BindWidgetOptional))
    class UTextBlock* NameText = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    class UButton* SelectButton = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    class UTextBlock* SelectButtonText = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    class UButton* DeleteButton = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    class UButton* NameLabelButton = nullptr;

    void BuildRow();

    UFUNCTION()
    void OnSelectClicked();

    UFUNCTION()
    void OnDeleteClicked();

    UFUNCTION()
    void OnNameLabelButtonClicked();

    UFUNCTION()
    void OnLabelEditConfirmed(int32 InDroneId, const FString& NewName,
        FLinearColor NewColor, int32 NewFontSize);

    /** 响应多选变化委托，刷新选中按钮文字 */
    UFUNCTION()
    void UpdateSelectState();
};
