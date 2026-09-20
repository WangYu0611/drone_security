// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiDroneManager.h"
#include "MultiDroneCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "RealTimeDroneReceiver.h"
#include "UE5DroneControlCharacter.h"
#include "UE5DroneControl.h"
#include "DroneOps/Drone/DroneCommandSenderComponent.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Core/DroneOpsTypes.h"
#include "Engine/GameInstance.h"

AMultiDroneManager::AMultiDroneManager()
{
	PrimaryActorTick.bCanEverTick = false;
	ActiveDroneIndex = 0;

	CommandSenderComponent = CreateDefaultSubobject<UDroneCommandSenderComponent>(TEXT("CommandSenderComponent"));
}

void AMultiDroneManager::BeginPlay()
{
	Super::BeginPlay();
	RefreshDroneLists();

	if (DroneReceivers.Num() == 0)
	{
		UE_LOG(LogUE5DroneControl, Warning, TEXT("MultiDroneManager: No RealTimeDroneReceiver found in level"));
		ActiveDroneIndex = -1;
		return;
	}

	if (ActiveDroneIndex < 0 || ActiveDroneIndex >= DroneReceivers.Num())
	{
		ActiveDroneIndex = 0;
	}

	// Fallback registration: ensure all MultiDroneCharacter and
	// RealTimeDroneReceiver actors are registered (handles cases where
	// their own BeginPlay ran before the GameMode set up the registry).
	UDroneRegistrySubsystem* Registry = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		Registry = GI->GetSubsystem<UDroneRegistrySubsystem>();
	}

	if (!Registry)
	{
		return;
	}

	// Register MultiDroneCharacter senders
	TArray<AActor*> CharacterActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMultiDroneCharacter::StaticClass(), CharacterActors);
	for (AActor* Actor : CharacterActors)
	{
		if (AMultiDroneCharacter* Char = Cast<AMultiDroneCharacter>(Actor))
		{
			if (!Registry->IsDroneRegistered(Char->DroneId))
			{
				FDroneDescriptor Desc;
				Desc.Name            = Char->DroneName;
				Desc.DroneId         = Char->DroneId;
				Desc.MavlinkSystemId = Char->MavlinkSystemId;
				Desc.BitIndex        = Char->BitIndex;
				Desc.ThemeColor      = Char->ThemeColor;
				Desc.UEReceivePort   = Char->UEReceivePort;
				Desc.TopicPrefix     = Char->TopicPrefix;
				Registry->RegisterDrone(Desc);
			}
			Registry->RegisterSenderPawn(Char->DroneId, Char);
		}
	}

	// Register RealTimeDroneReceiver receivers
	for (ARealTimeDroneReceiver* Receiver : DroneReceivers)
	{
		if (!Receiver)
		{
			continue;
		}
		if (!Registry->IsDroneRegistered(Receiver->DroneId))
		{
			FDroneDescriptor Desc;
			Desc.Name          = Receiver->DroneName;
			Desc.DroneId       = Receiver->DroneId;
			Desc.UEReceivePort = Receiver->ListenPort;
			Registry->RegisterDrone(Desc);
		}
		Registry->RegisterReceiverActor(Receiver->DroneId, Receiver);
	}

	UE_LOG(LogUE5DroneControl, Log, TEXT("MultiDroneManager: Registry has %d drones"), Registry->GetAllDroneDescriptors().Num());
}

void AMultiDroneManager::RefreshDroneLists()
{
	DroneReceivers.Empty();
	DroneSenders.Empty();

	TArray<AActor*> ReceiverActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARealTimeDroneReceiver::StaticClass(), ReceiverActors);
	for (AActor* Actor : ReceiverActors)
	{
		if (ARealTimeDroneReceiver* Receiver = Cast<ARealTimeDroneReceiver>(Actor))
		{
			DroneReceivers.Add(Receiver);
		}
	}

	DroneReceivers.Sort([](const ARealTimeDroneReceiver& A, const ARealTimeDroneReceiver& B)
	{
		return A.ListenPort < B.ListenPort;
	});

	TArray<AActor*> SenderActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMultiDroneCharacter::StaticClass(), SenderActors);
	for (AActor* Actor : SenderActors)
	{
		AMultiDroneCharacter* Character = Cast<AMultiDroneCharacter>(Actor);
		if (!Character)
		{
			continue;
		}

		DroneSenders.Add(Character);
	}
}

void AMultiDroneManager::SwitchToDrone(int32 Index)
{
	if (DroneReceivers.Num() == 0)
	{
		UE_LOG(LogUE5DroneControl, Warning, TEXT("MultiDroneManager: No receivers available"));
		return;
	}

	if (Index < 0 || Index >= DroneReceivers.Num())
	{
		UE_LOG(LogUE5DroneControl, Warning, TEXT("MultiDroneManager: Invalid drone index %d"), Index);
		return;
	}

	ARealTimeDroneReceiver* Target = DroneReceivers[Index];
	if (!Target)
	{
		UE_LOG(LogUE5DroneControl, Warning, TEXT("MultiDroneManager: Receiver at index %d is null"), Index);
		return;
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		PC->SetViewTargetWithBlend(Target, 0.5f);
	}

	ActiveDroneIndex = Index;
}

AUE5DroneControlCharacter* AMultiDroneManager::GetActiveSender() const
{
	if (ActiveDroneIndex < 0 || ActiveDroneIndex >= DroneSenders.Num())
	{
		return nullptr;
	}
	return DroneSenders[ActiveDroneIndex];
}

ARealTimeDroneReceiver* AMultiDroneManager::GetActiveReceiver() const
{
	if (ActiveDroneIndex < 0 || ActiveDroneIndex >= DroneReceivers.Num())
	{
		return nullptr;
	}
	return DroneReceivers[ActiveDroneIndex];
}
