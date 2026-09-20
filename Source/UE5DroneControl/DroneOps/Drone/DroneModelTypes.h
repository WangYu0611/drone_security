// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DroneModelTypes.generated.h"

/**
 * 每架无人机可独立选择的外观模型。
 * Original 走原有 SkeletalMesh + ABP_Drone 动画蓝图；其余三项为 StaticMesh 组装件。
 */
UENUM(BlueprintType)
enum class EDroneModelType : uint8
{
	Original  UMETA(DisplayName = "原始模型"),
	Military  UMETA(DisplayName = "军用机"),
	Police    UMETA(DisplayName = "警用机"),
	SciFi     UMETA(DisplayName = "科幻机"),
};

/**
 * 组装件中的一个分件。
 *
 * RelativeLocation 使用**未缩放的模型单位**（即 FBX 原始尺度，整机宽约 1.3 单位），
 * 由 UDroneVisualComponent 统一乘以 FDroneModelDefinition::Scale 后写入组件相对位置。
 * 这样调整 Scale 时各分件自动保持相对位置正确，无需手工换算。
 */
USTRUCT()
struct FDroneModelPart
{
	GENERATED_BODY()

	/** StaticMesh 资产路径（含对象名）。 */
	UPROPERTY()
	FString MeshPath;

	/** 相对机身原点的位置，单位为未缩放模型单位。 */
	UPROPERTY()
	FVector RelativeLocation = FVector::ZeroVector;

	/**
	 * 绕本地 Z 轴的自转速度倍率：0 表示静止（机身 / 云台 / 徽章），
	 * ±1 表示按 FDroneModelDefinition::PropellerRpm 正/反向旋转。
	 * 对角螺旋桨同向、相邻反向，与真实四旋翼一致。
	 */
	UPROPERTY()
	float SpinDirection = 0.0f;

	FDroneModelPart() = default;
	FDroneModelPart(const FString& InPath, const FVector& InLocation, float InSpin = 0.0f)
		: MeshPath(InPath)
		, RelativeLocation(InLocation)
		, SpinDirection(InSpin)
	{}
};

/**
 * 一种外观模型的完整定义。
 */
USTRUCT()
struct FDroneModelDefinition
{
	GENERATED_BODY()

	/** 下拉框显示名。 */
	UPROPERTY()
	FString DisplayName;

	/** true 表示使用 Pawn 自带的 SkeletalMesh（仅 Original），此时 BodyMeshPath / Parts 不被读取。 */
	UPROPERTY()
	bool bIsSkeletal = false;

	/** 机身 StaticMesh 路径。承接 ECC_Visibility 命中职责。 */
	UPROPERTY()
	FString BodyMeshPath;

	/** 螺旋桨路径，按 FL / FR / BL / BR 顺序。仅作记录与校验，实际位置在 Parts 中。 */
	UPROPERTY()
	TArray<FString> PropellerMeshPaths;

	/** 机身之外的全部分件（螺旋桨、云台、徽章）。 */
	UPROPERTY()
	TArray<FDroneModelPart> Parts;

	/** 相对现有无人机的尺寸校正。原始 FBX 以米为单位导入成厘米，故需要放大。 */
	UPROPERTY()
	float Scale = 1.0f;

	/** 视觉中心校正（cm），叠加在 Actor 原点之上。 */
	UPROPERTY()
	float ZOffset = 0.0f;

	/** 机头朝向校正（度）。模型自身机头指向 -Y，需 +90 才对齐 UE 的 +X 前方。 */
	UPROPERTY()
	float YawOffset = 0.0f;

	/** 螺旋桨转速。默认值与原始模型 Anim_Flight 的实测转速一致（720°/s）。 */
	UPROPERTY()
	float PropellerRpm = 120.0f;
};

/** 取得指定模型的定义表条目。未知值回退到 Original。 */
UE5DRONECONTROL_API const FDroneModelDefinition& GetDroneModelDefinition(EDroneModelType ModelType);

/**
 * 把任意枚举值收敛为定义表中存在的值：未知值返回 Original。
 * 供需要**记录**模型类型的场合使用，避免把非法值存进状态。
 */
UE5DRONECONTROL_API EDroneModelType ResolveDroneModelType(EDroneModelType ModelType);

/** 按枚举声明顺序返回全部模型类型，供 UI 填充下拉框。 */
UE5DRONECONTROL_API const TArray<EDroneModelType>& GetAllDroneModelTypes();

/** 取得模型的下拉框显示名。 */
UE5DRONECONTROL_API FString GetDroneModelDisplayName(EDroneModelType ModelType);

/** 由显示名反查模型类型。找不到时返回 false 且不修改 OutType。 */
UE5DRONECONTROL_API bool TryGetDroneModelTypeByDisplayName(const FString& DisplayName, EDroneModelType& OutType);
