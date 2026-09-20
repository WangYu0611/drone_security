// Copyright Epic Games, Inc. All Rights Reserved.

#include "DroneModelTypes.h"

/*
 * 定义表的数值来源（均从资产与源 FBX 二进制中读出，不是估算值）：
 *
 * 1. 分件对应关系。三个模型的源 FBX 里节点声明顺序固定为
 *    Body / FR_Propeller / Camera / FL_Propeller / BL_Propeller / BR_Propeller（police 末尾多一个
 *    ProfessionalAssets_Badge）。Interchange 按同一顺序生成 Mesh / Mesh_ncl_1..N，且各资产顶点数
 *    与对应 FBX 几何体的控制点数一一吻合（机身 4186、螺旋桨 305、云台 232、徽章 179）。
 *    故 Mesh_ncl_2 是**云台**而非螺旋桨——按序号顺序猜测会把它当成螺旋桨而旋转错件。
 *
 * 2. 分件位置。各分件几何体在自身局部坐标系内均以 XY 原点为中心（螺旋桨 bounds origin XY≈0），
 *    位置信息只存在于 FBX 节点的 Lcl Translation 中，导入后未烘进顶点。因此必须由此表提供。
 *    FBX 场景为 Y-up 右手系（UpAxis=1），转 UE 的 Z-up 左手系为 (x, -z, y)。
 *
 * 3. Scale。FBX UnitScaleFactor=100，坐标被当作厘米导入，整机含桨仅约 1.46 单位宽。
 *    原始模型对照：SK_Drone_Scavenger 资产 bounds 半展为 (179.5, 151.5, 114.3)，即全宽 359cm；
 *    两个蓝图的 Mesh 组件相对缩放均为 0.5，故实际上屏全宽为 179.5 x 151.5 cm。
 *    新模型含桨的半展为 X 0.7301 / Y 0.7269 单位，即全宽 1.460 x 1.454 单位；
 *    取 115 倍后上屏为 168 x 167 cm，与原始模型的 179.5 x 151.5 cm 量级相当（比值 0.94）。
 *    新模型是薄型四旋翼，高度方向天然更矮（约 56cm 对 114cm），不强行拉伸。
 *
 *    注：本组件挂在 Capsule 上而非 Mesh 上，不继承 Mesh 的 0.5 缩放，故此处直接对照上屏尺寸。
 *
 * 4. YawOffset。前桨（FL/FR）位于 UE -Y 侧，即模型机头朝 -Y；+90° 偏航后对齐 UE 的 +X 前方。
 */

namespace
{
	/** 三个 StaticMesh 组装件的资产目录。 */
	const FString MilitaryDir = TEXT("/Game/DroneModel/free-military-drone-full-pack-in-description/source/");
	const FString PoliceDir   = TEXT("/Game/DroneModel/free-police-drone-full-pack-in-description/source/");
	const FString SciFiDir    = TEXT("/Game/DroneModel/free-sci-fi-drone-full-pack-in-description/source/");

	/** 由目录与资产名拼出带对象名的完整路径（/Game/Foo/Mesh.Mesh）。 */
	FString MakeMeshPath(const FString& Dir, const FString& AssetName)
	{
		return Dir + AssetName + TEXT(".") + AssetName;
	}

	// ---- 分件相对位置（未缩放模型单位，已由 FBX Y-up 转为 UE Z-up）----
	const FVector LocFrontLeftProp  (  0.46564f, -0.45022f,  0.04275f );
	const FVector LocFrontRightProp ( -0.46401f, -0.45022f,  0.04275f );
	const FVector LocBackLeftProp   (  0.46564f,  0.45331f,  0.04275f );
	const FVector LocBackRightProp  ( -0.46401f,  0.45331f,  0.04275f );
	const FVector LocCameraGimbal   ( -0.00152f, -0.01883f, -0.25178f );
	const FVector LocPoliceBadge    (  0.00082f, -0.00198f,  0.09404f );
	// Body 节点的 Lcl Translation 为 (0.00082, -0.0023, -0.00154)，转 UE 后各轴均 < 0.003 单位，
	// 即上屏不足 0.35cm。机身固定挂在挂载点原点，不再单独补这一位移。

	/** 对角同向、相邻反向。 */
	constexpr float SpinCcw =  1.0f;
	constexpr float SpinCw  = -1.0f;

	/**
	 * 三个模型共用同一套几何体（顶点数与 bounds 完全一致），只有材质实例与贴图不同。
	 * 因此分件装配完全共用，仅目录与是否含徽章有差别。
	 */
	FDroneModelDefinition MakeStaticAssemblyDefinition(const FString& InDisplayName, const FString& Dir, bool bHasBadge)
	{
		FDroneModelDefinition Def;
		Def.DisplayName   = InDisplayName;
		Def.bIsSkeletal   = false;
		Def.BodyMeshPath  = MakeMeshPath(Dir, TEXT("Mesh"));
		Def.Scale         = 115.0f;
		Def.ZOffset       = 0.0f;
		Def.YawOffset     = 90.0f;
		// 与原始模型一致：ABP_Drone 用的 Anim_Flight 里 blade_01..04 绕自身 Z 轴
		// 在 2.5s 内转 1800°（正好 5 圈），即 720°/s = 120 RPM。此值是从动画曲线
		// 逐帧采样测得的，不是估计值。
		Def.PropellerRpm  = 120.0f;

		const FString FrontLeft  = MakeMeshPath(Dir, TEXT("Mesh_ncl_3"));
		const FString FrontRight = MakeMeshPath(Dir, TEXT("Mesh_ncl_1"));
		const FString BackLeft   = MakeMeshPath(Dir, TEXT("Mesh_ncl_4"));
		const FString BackRight  = MakeMeshPath(Dir, TEXT("Mesh_ncl_5"));

		// 记录用，按 FL / FR / BL / BR 顺序
		Def.PropellerMeshPaths = { FrontLeft, FrontRight, BackLeft, BackRight };

		Def.Parts.Add(FDroneModelPart(FrontLeft,  LocFrontLeftProp,  SpinCcw));
		Def.Parts.Add(FDroneModelPart(FrontRight, LocFrontRightProp, SpinCw));
		Def.Parts.Add(FDroneModelPart(BackLeft,   LocBackLeftProp,   SpinCw));
		Def.Parts.Add(FDroneModelPart(BackRight,  LocBackRightProp,  SpinCcw));

		// 云台：静止分件。Mesh_ncl_2 对应 FBX 的 Camera 节点。
		Def.Parts.Add(FDroneModelPart(MakeMeshPath(Dir, TEXT("Mesh_ncl_2")), LocCameraGimbal));

		if (bHasBadge)
		{
			// police 独有的机身徽章，使用第二个材质实例 02_-_Default。
			Def.Parts.Add(FDroneModelPart(MakeMeshPath(Dir, TEXT("Mesh_ncl_6")), LocPoliceBadge));
		}

		return Def;
	}

	FDroneModelDefinition MakeOriginalDefinition()
	{
		FDroneModelDefinition Def;
		Def.DisplayName  = TEXT("原始模型");
		Def.bIsSkeletal  = true;
		// 仅作记录：Original 的 mesh 由蓝图在编辑器中指定，代码不重新加载。
		Def.BodyMeshPath = TEXT("/Game/Drone_Scavenger/SK/SK_Drone_Scavenger.SK_Drone_Scavenger");
		Def.Scale        = 1.0f;
		return Def;
	}

	/** 定义表。首次调用时构造，之后返回同一份常量引用。 */
	const TMap<EDroneModelType, FDroneModelDefinition>& GetDefinitionTable()
	{
		static const TMap<EDroneModelType, FDroneModelDefinition> Table =
		{
			{ EDroneModelType::Original, MakeOriginalDefinition() },
			{ EDroneModelType::Military, MakeStaticAssemblyDefinition(TEXT("军用机"), MilitaryDir, /*bHasBadge=*/false) },
			{ EDroneModelType::Police,   MakeStaticAssemblyDefinition(TEXT("警用机"), PoliceDir,   /*bHasBadge=*/true)  },
			{ EDroneModelType::SciFi,    MakeStaticAssemblyDefinition(TEXT("科幻机"), SciFiDir,    /*bHasBadge=*/false) },
		};
		return Table;
	}
}

const FDroneModelDefinition& GetDroneModelDefinition(EDroneModelType ModelType)
{
	const TMap<EDroneModelType, FDroneModelDefinition>& Table = GetDefinitionTable();
	if (const FDroneModelDefinition* Found = Table.Find(ModelType))
	{
		return *Found;
	}

	UE_LOG(LogTemp, Warning, TEXT("GetDroneModelDefinition: 未知模型类型 %d，回退到 Original"),
		static_cast<int32>(ModelType));
	return Table.FindChecked(EDroneModelType::Original);
}

EDroneModelType ResolveDroneModelType(EDroneModelType ModelType)
{
	return GetDefinitionTable().Contains(ModelType) ? ModelType : EDroneModelType::Original;
}

const TArray<EDroneModelType>& GetAllDroneModelTypes(){
	static const TArray<EDroneModelType> Types =
	{
		EDroneModelType::Original,
		EDroneModelType::Military,
		EDroneModelType::Police,
		EDroneModelType::SciFi,
	};
	return Types;
}

FString GetDroneModelDisplayName(EDroneModelType ModelType)
{
	return GetDroneModelDefinition(ModelType).DisplayName;
}

bool TryGetDroneModelTypeByDisplayName(const FString& DisplayName, EDroneModelType& OutType)
{
	for (const EDroneModelType Type : GetAllDroneModelTypes())
	{
		if (GetDroneModelDefinition(Type).DisplayName == DisplayName)
		{
			OutType = Type;
			return true;
		}
	}
	return false;
}
