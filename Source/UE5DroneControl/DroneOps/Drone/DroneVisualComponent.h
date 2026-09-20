// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "DroneOps/Drone/DroneModelTypes.h"
#include "DroneVisualComponent.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;

/** 一个运行时创建出来的分件组件及其自转方向。 */
USTRUCT()
struct FDroneModelSpawnedPart
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Component = nullptr;

	/** 0 表示静止；±1 表示按定义表转速正/反向自转。 */
	UPROPERTY()
	float SpinDirection = 0.0f;
};

/**
 * 无人机外观模型挂载点。
 *
 * 唯一持有“模型如何显示”知识的单元：按 EDroneModelType 从定义表加载 StaticMesh 分件、
 * 挂到自身、转移 ECC_Visibility 命中职责、并逐帧旋转螺旋桨。
 * 切回 Original 时销毁全部分件并恢复 Pawn 自带的 SkeletalMesh。
 *
 * 只依赖 DroneModelTypes.h 与引擎组件 API：不依赖 registry、不依赖具体 Pawn 类型，
 * 因此可独立测试。
 */
UCLASS(ClassGroup = "DroneOps", meta = (BlueprintSpawnableComponent))
class UE5DRONECONTROL_API UDroneVisualComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UDroneVisualComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	/**
	 * 切换外观模型。
	 * 资产加载失败时保持当前外观不变并记录 Warning。
	 * 完成后自动刷新 Owner 上 UDroneSelectionComponent 的选中/悬停高亮（若存在）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Drone Visual")
	void ApplyModelType(EDroneModelType NewType);

	UFUNCTION(BlueprintPure, Category = "Drone Visual")
	EDroneModelType GetCurrentModelType() const { return CurrentModelType; }

	/** 当前已创建的分件组件数量。0 表示正在使用 Original 的 SkeletalMesh。 */
	UFUNCTION(BlueprintPure, Category = "Drone Visual")
	int32 GetSpawnedPartCount() const { return SpawnedParts.Num(); }

	/** 承接 ECC_Visibility 命中的机身组件；Original 模式下为 nullptr。 */
	UFUNCTION(BlueprintPure, Category = "Drone Visual")
	UStaticMeshComponent* GetBodyComponent() const;

private:
	/** 销毁全部已创建的分件组件。 */
	void DestroySpawnedParts();

	/** 取得 Owner 上的 SkeletalMesh 组件（Original 模型的载体）。 */
	USkeletalMeshComponent* FindOwnerSkeletalMesh() const;

	/**
	 * 取得应当由新分件继承的 override material。
	 *
	 * 镜像机（ARealTimeDroneReceiver）在**关卡实例**上把 SkeletalMesh 的全部槽位覆盖成
	 * MI_DefaultColorway（Base Color = 0, 0.092, 0.231，即蓝色），影子机则没有覆盖。
	 * 运行时新建的 StaticMeshComponent 不会自动继承这个覆盖，若不处理，换模型后
	 * 镜像机会失去蓝色、与影子机不一致。
	 *
	 * 仅在「全部槽位被同一个材质覆盖」时返回该材质：分件的槽位数与 SkeletalMesh 不同，
	 * 逐槽位映射没有合理对应关系，而镜像机的用法恰好是整机同色。
	 * 无覆盖、或各槽位覆盖不一致时返回 nullptr，此时分件使用资产自带材质。
	 */
	UMaterialInterface* FindInheritedOverrideMaterial() const;

	/** 显示 / 隐藏原始 SkeletalMesh，并同步转移其碰撞。 */
	void SetSkeletalMeshActive(bool bActive);

	/** 依据是否存在自转分件启停 Tick。 */
	void UpdateTickEnabled();

	/** 重建分件后补一次选中/悬停高亮（Owner 上有 UDroneSelectionComponent 时）。 */
	void RefreshOwnerHighlight();

	UPROPERTY()
	EDroneModelType CurrentModelType = EDroneModelType::Original;

	UPROPERTY(Transient)
	TArray<FDroneModelSpawnedPart> SpawnedParts;

	/** 当前模型的螺旋桨转速，缓存自定义表，避免每帧查表。 */
	UPROPERTY(Transient)
	float ActivePropellerRpm = 0.0f;

	/** 累计自转角度（度），保证转速与帧率无关且不会因浮点累积溢出。 */
	UPROPERTY(Transient)
	float AccumulatedSpinDegrees = 0.0f;
};
