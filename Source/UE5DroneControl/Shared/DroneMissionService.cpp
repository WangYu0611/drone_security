#include "Shared/DroneMissionService.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Network/DroneNetworkManager.h"
#include "DroneOps/Network/DroneWebSocketClient.h"
#include "Engine/GameInstance.h"

bool UDroneMissionService::CanPauseSelected() const
{
    const auto* Registry = GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    const auto* Network = GetGameInstance()->GetSubsystem<UDroneNetworkManager>();
    if (!Registry || Registry->GetMultiSelectedDrones().IsEmpty() || !Network || !Network->CanSendToBackend()
        || !Network->GetWebSocketClient() || !Network->GetWebSocketClient()->IsConnected()) return false;
    for (int32 Id : Registry->GetMultiSelectedDrones())
    {
        FDroneTelemetrySnapshot Telemetry;
        if (!Registry->GetTelemetry(Id, Telemetry) || Telemetry.Availability != EDroneAvailability::Online) return false;
    }
    return true;
}
bool UDroneMissionService::SetSelectedPaused(bool bPaused)
{
    if (!CanPauseSelected()) return false;
    auto* Registry = GetGameInstance()->GetSubsystem<UDroneRegistrySubsystem>();
    GetGameInstance()->GetSubsystem<UDroneNetworkManager>()->SendPauseCommand(Registry->GetMultiSelectedDrones(), bPaused);
    return true; // Submitted request; result remains authoritative in Registry task state.
}
