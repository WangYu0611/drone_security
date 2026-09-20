#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CommandAlertStore.generated.h"

USTRUCT(BlueprintType)
struct FCommandAlert
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 Id = 0;
    UPROPERTY(BlueprintReadOnly) FString SharedId;
    UPROPERTY(BlueprintReadOnly) FDateTime ReceivedAt;
    UPROPERTY(BlueprintReadOnly) int32 DroneId = 0;
    UPROPERTY(BlueprintReadOnly) FString Type;
    UPROPERTY(BlueprintReadOnly) FString Message;
    UPROPERTY(BlueprintReadOnly) bool bHandled = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommandAlertChanged, int32, AlertId);

/** Session-local event history. Survives shell recreation; never invents telemetry or GPS. */
UCLASS()
class UE5DRONECONTROL_API UCommandAlertStore : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    UFUNCTION(BlueprintCallable) int32 AddAlert(int32 DroneId, const FString& Type, const FString& Message);
    void AddSharedAlert(const FString& SharedId, int32 DroneId, const FString& Type, const FString& Message);
    UFUNCTION(BlueprintPure) TArray<FCommandAlert> GetAlerts() const { return Alerts; }
    UFUNCTION(BlueprintCallable) bool ClearAlert(int32 AlertId);
    UFUNCTION(BlueprintCallable) bool MarkHandled(int32 AlertId);
    UFUNCTION(BlueprintPure) int32 GetUnhandledCount() const;
    UPROPERTY(BlueprintAssignable) FCommandAlertChanged OnAlertAdded;
    UPROPERTY(BlueprintAssignable) FCommandAlertChanged OnAlertUpdated;
    static constexpr int32 MaxAlerts = 200;
private:
    UPROPERTY() TArray<FCommandAlert> Alerts;
    TWeakObjectPtr<class UDroneNetworkManager> Network;
    FDelegateHandle AlertHandle;
    int32 NextId = 1;
    void HandleNetworkAlert(int32 DroneId, const FString& Type, int32 Value);
};
