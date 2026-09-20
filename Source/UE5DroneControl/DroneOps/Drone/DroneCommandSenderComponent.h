// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneOps/Core/DroneOpsTypes.h"
#include "Sockets.h"
#include "DroneCommandSenderComponent.generated.h"

/**
 * Component that builds and sends the 32-byte multi-drone control packet
 * to multi_ue_controller.py on port 8899.
 *
 * Attach ONE instance to the scene (e.g. on MultiDroneManager or a
 * dedicated Actor). DroneOpsPlayerController gets a reference via
 * DroneRegistrySubsystem.
 *
 * Wire format (little-endian, 32 bytes):
 *   double  Timestamp   (8)
 *   float   X           (4)   NED north  [m]
 *   float   Y           (4)   NED east   [m]
 *   float   Z           (4)   NED down   [m]
 *   int32   Mode        (4)   0=hover, 1=move
 *   int32   DroneMask   (4)   bit per drone (BitIndex)
 *   int32   Sequence    (4)   monotonic counter
 */
UCLASS(ClassGroup = "DroneOps", meta = (BlueprintSpawnableComponent))
class UE5DRONECONTROL_API UDroneCommandSenderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneCommandSenderComponent();

	// IP of multi_ue_controller.py (usually 127.0.0.1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommandSender")
	FString UnifiedControllerIP = TEXT("127.0.0.1");

	// Port of multi_ue_controller.py
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommandSender")
	int32 UnifiedControllerPort = 8899;

	// Minimum interval between sends (seconds). 0 = no throttle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CommandSender")
	float MinSendIntervalSec = 0.05f; // 20 Hz cap

	// ---- High-level send API ----

	/**
	 * Send command to a single drone.
	 * Looks up BitIndex from DroneRegistrySubsystem to build the mask.
	 * @param DroneId  Target drone id
	 * @param NedTarget  Target position in NED coordinates [meters]
	 * @param Mode  0=hover, 1=move
	 */
	UFUNCTION(BlueprintCallable, Category = "CommandSender")
	void SendSingleDroneCommand(int32 DroneId, FVector NedTarget, int32 Mode);

	/**
	 * Send command to multiple drones using an explicit bitmask.
	 * Used by FormationPlaybackSubsystem.
	 * @param DroneMask  Bitmask (bit N = drone with BitIndex N)
	 * @param NedTarget  Shared target in NED [meters]
	 * @param Mode  0=hover, 1=move
	 */
	UFUNCTION(BlueprintCallable, Category = "CommandSender")
	void SendMultiDroneCommand(int32 DroneMask, FVector NedTarget, int32 Mode);

	/**
	 * Send hover command to a single drone (Mode=0, zero target).
	 */
	UFUNCTION(BlueprintCallable, Category = "CommandSender")
	void SendHoverCommand(int32 DroneId);

	UFUNCTION(BlueprintPure, Category = "CommandSender")
	const FMultiDroneControlPacket& GetLastSentPacket() const { return LastPacket; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FSocket* SenderSocket = nullptr;
	TSharedPtr<class FInternetAddr> RemoteAddr;
	int32 Sequence = 0;
	double LastSendTime = 0.0;

	UPROPERTY()
	FMultiDroneControlPacket LastPacket;

	bool InitSocket();
	void CloseSocket();
	bool SendPacket(const FMultiDroneControlPacket& Packet);
};
