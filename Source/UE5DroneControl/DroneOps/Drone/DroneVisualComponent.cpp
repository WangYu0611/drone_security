// Copyright Epic Games, Inc. All Rights Reserved.

#include "DroneVisualComponent.h"
#include "DroneOps/Drone/DroneSelectionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

UDroneVisualComponent::UDroneVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 只有存在自转分件时才需要 Tick；Original 模式下保持关闭。
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UDroneVisualComponent::ApplyModelType(EDroneModelType NewType)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 收敛非法枚举值，避免把它记进 CurrentModelType。
	NewType = ResolveDroneModelType(NewType);
	const FDroneModelDefinition& Def = GetDroneModelDefinition(NewType);

	if (Def.bIsSkeletal)
	{
		// 切回原始模型：销毁全部 StaticMesh 分件并恢复 SkeletalMesh。
		DestroySpawnedParts();
		SetSkeletalMeshActive(true);
		SetRelativeLocation(FVector::ZeroVector);
		SetRelativeRotation(FRotator::ZeroRotator);
		ActivePropellerRpm = 0.0f;
		CurrentModelType = NewType;
		UpdateTickEnabled();
		RefreshOwnerHighlight();
		return;
	}

	// 机身是命中体，加载失败则整体放弃切换，保持当前外观不变。
	UStaticMesh* BodyMesh = LoadObject<UStaticMesh>(nullptr, *Def.BodyMeshPath);
	if (!BodyMesh)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("DroneVisualComponent: 模型 '%s' 的机身资产加载失败，保持当前外观。路径: %s"),
			*Def.DisplayName, *Def.BodyMeshPath);
		return;
	}

	// 先创建新组件再销毁旧组件，避免出现空白帧。
	TArray<FDroneModelSpawnedPart> NewParts;

	// 镜像机在关卡里给 SkeletalMesh 挂了蓝色覆盖材质（MI_DefaultColorway，
	// Base Color = 0, 0.092, 0.231）。运行时新建的组件不会自动继承它，
	// 不处理的话换模型后镜像机就会丢掉蓝色、与影子机混淆。
	UMaterialInterface* const InheritedOverride = FindInheritedOverrideMaterial();

	auto SpawnPart = [this, Owner, &Def, &NewParts, InheritedOverride](UStaticMesh* Mesh, const FVector& UnscaledLocation, float SpinDirection, bool bIsBody)
	{
		const FName ComponentName = MakeUniqueObjectName(
			Owner, UStaticMeshComponent::StaticClass(), TEXT("DroneModelPart"));

		UStaticMeshComponent* PartComponent =
			NewObject<UStaticMeshComponent>(Owner, ComponentName, RF_Transient);
		if (!PartComponent)
		{
			return;
		}

		PartComponent->SetStaticMesh(Mesh);

		// 继承 SkeletalMesh 的覆盖材质，保持镜像机 / 影子机的配色区分。
		if (InheritedOverride)
		{
			const int32 SlotCount = PartComponent->GetNumMaterials();
			for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
			{
				PartComponent->SetMaterial(SlotIndex, InheritedOverride);
			}
		}

		PartComponent->SetupAttachment(this);
		PartComponent->SetRelativeLocation(UnscaledLocation * Def.Scale);
		PartComponent->SetRelativeScale3D(FVector(Def.Scale));

		// 碰撞：转移原 SkeletalMesh 的 ECC_Visibility 命中职责。
		// 机身与分件都参与命中——命中判定是 Actor 级的（ResolveDroneIdFromActor），
		// 不区分组件，让桨叶区域也可点选能明显改善操作手感。
		PartComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		PartComponent->SetCollisionObjectType(ECC_WorldDynamic);
		PartComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		PartComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		PartComponent->SetGenerateOverlapEvents(false);
		PartComponent->SetSimulatePhysics(false);

		PartComponent->RegisterComponent();

		FDroneModelSpawnedPart& Entry = NewParts.AddDefaulted_GetRef();
		Entry.Component = PartComponent;
		Entry.SpinDirection = bIsBody ? 0.0f : SpinDirection;
	};

	SpawnPart(BodyMesh, FVector::ZeroVector, 0.0f, /*bIsBody=*/true);

	for (const FDroneModelPart& Part : Def.Parts)
	{
		UStaticMesh* PartMesh = LoadObject<UStaticMesh>(nullptr, *Part.MeshPath);
		if (!PartMesh)
		{
			// 分件缺失不影响机身与其余分件，跳过即可。
			UE_LOG(LogTemp, Warning,
				TEXT("DroneVisualComponent: 模型 '%s' 的分件加载失败，已跳过。路径: %s"),
				*Def.DisplayName, *Part.MeshPath);
			continue;
		}
		SpawnPart(PartMesh, Part.RelativeLocation, Part.SpinDirection, /*bIsBody=*/false);
	}

	DestroySpawnedParts();
	SpawnedParts = MoveTemp(NewParts);

	// 挂载点自身承担整机的视觉中心与机头朝向校正。
	SetRelativeLocation(FVector(0.0f, 0.0f, Def.ZOffset));
	SetRelativeRotation(FRotator(0.0f, Def.YawOffset, 0.0f));

	SetSkeletalMeshActive(false);

	ActivePropellerRpm = Def.PropellerRpm;
	AccumulatedSpinDegrees = 0.0f;
	CurrentModelType = NewType;

	UpdateTickEnabled();
	RefreshOwnerHighlight();

	UE_LOG(LogTemp, Log, TEXT("DroneVisualComponent: %s 已切换到 '%s'（%d 个分件）"),
		*Owner->GetName(), *Def.DisplayName, SpawnedParts.Num());
}

void UDroneVisualComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ActivePropellerRpm <= 0.0f)
	{
		return;
	}

	// RPM → 度/秒：每分钟 N 转 = N * 360 / 60 度/秒。
	// 取模保持在一圈内，避免长时间运行后浮点精度下降。
	AccumulatedSpinDegrees = FMath::Fmod(
		AccumulatedSpinDegrees + ActivePropellerRpm * 6.0f * DeltaTime, 360.0f);

	for (const FDroneModelSpawnedPart& Part : SpawnedParts)
	{
		if (Part.SpinDirection != 0.0f && Part.Component)
		{
			Part.Component->SetRelativeRotation(
				FRotator(0.0f, AccumulatedSpinDegrees * Part.SpinDirection, 0.0f));
		}
	}
}

void UDroneVisualComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	DestroySpawnedParts();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

UStaticMeshComponent* UDroneVisualComponent::GetBodyComponent() const
{
	// 机身总是第一个创建的分件。
	return SpawnedParts.IsEmpty() ? nullptr : SpawnedParts[0].Component.Get();
}

void UDroneVisualComponent::DestroySpawnedParts()
{
	for (const FDroneModelSpawnedPart& Part : SpawnedParts)
	{
		if (Part.Component)
		{
			Part.Component->DestroyComponent();
		}
	}
	SpawnedParts.Empty();
}

USkeletalMeshComponent* UDroneVisualComponent::FindOwnerSkeletalMesh() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

UMaterialInterface* UDroneVisualComponent::FindInheritedOverrideMaterial() const
{
	const USkeletalMeshComponent* SkeletalMesh = FindOwnerSkeletalMesh();
	if (!SkeletalMesh)
	{
		return nullptr;
	}

	const TArray<TObjectPtr<UMaterialInterface>>& Overrides = SkeletalMesh->OverrideMaterials;
	if (Overrides.Num() == 0)
	{
		// 影子机：没有覆盖，各分件用自己的材质实例（沙漠迷彩 / 警用涂装 / 科幻）。
		return nullptr;
	}

	// 只在覆盖"全槽位一致"时继承。新模型的槽位数与 SkeletalMesh 不同，
	// 逐槽位对应没有意义；而镜像机的两个槽位用的是同一个蓝色材质，
	// 这种情形取其一即可正确复现配色。若各槽位不同则放弃，避免猜错。
	UMaterialInterface* const First = Overrides[0];
	if (!First)
	{
		return nullptr;
	}
	for (const TObjectPtr<UMaterialInterface>& Override : Overrides)
	{
		if (Override != First)
		{
			return nullptr;
		}
	}
	return First;
}

void UDroneVisualComponent::SetSkeletalMeshActive(bool bActive)
{
	USkeletalMeshComponent* SkeletalMesh = FindOwnerSkeletalMesh();
	if (!SkeletalMesh)
	{
		return;
	}

	SkeletalMesh->SetVisibility(bActive, /*bPropagateToChildren=*/true);

	if (bActive)
	{
		// 恢复原有设置：Mesh 重新承接 ECC_Visibility 命中。
		SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		SkeletalMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
	else
	{
		SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void UDroneVisualComponent::UpdateTickEnabled()
{
	bool bNeedsTick = false;
	if (ActivePropellerRpm > 0.0f)
	{
		for (const FDroneModelSpawnedPart& Part : SpawnedParts)
		{
			if (Part.SpinDirection != 0.0f && Part.Component)
			{
				bNeedsTick = true;
				break;
			}
		}
	}
	SetComponentTickEnabled(bNeedsTick);
}

void UDroneVisualComponent::RefreshOwnerHighlight()
{
	// 新建的分件组件不在上一次 ApplyVisualState() 的遍历范围内，
	// 若无人机当前已处于选中/悬停状态，需要补一次高亮。
	// 找不到组件时跳过，保持本组件对 Pawn 结构的弱依赖。
	if (const AActor* Owner = GetOwner())
	{
		if (UDroneSelectionComponent* Selection = Owner->FindComponentByClass<UDroneSelectionComponent>())
		{
			Selection->RefreshHighlight();
		}
	}
}
