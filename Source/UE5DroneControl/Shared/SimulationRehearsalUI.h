#pragma once

#include "Shared/PlanWidgetSupport.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"

namespace SimulationUI
{
inline FText Label(const TCHAR* English, const TCHAR* Chinese)
{
    const bool bChinese = FInternationalization::Get().GetCurrentLanguage()->GetTwoLetterISOLanguageName() == TEXT("zh");
    return FText::FromString(bChinese ? Chinese : English);
}

inline bool Supported(const TSharedPtr<FJsonObject>& State)
{
    return PlanUI::Object(State, TEXT("simulation_uavs")).IsValid();
}

inline bool Aircraft(const TSharedPtr<FJsonObject>& State, const FString& Id)
{
    return PlanUI::Find(State, TEXT("simulation_uavs"), Id).IsValid();
}

inline FText AircraftCaption(const TSharedPtr<FJsonObject>& State, const FString& Id)
{
    const auto AircraftRecord = PlanUI::Find(State, TEXT("simulation_uavs"), Id);
    if (!AircraftRecord) return PlanUI::User(Id);
    const auto Name = PlanUI::Field(AircraftRecord, TEXT("name"));
    return FText::Format(Label(TEXT("{0} · Simulated"), TEXT("{0} · 模拟")), PlanUI::User(Name.IsEmpty() ? Id : Name));
}

struct FRegisteredAircraft
{
    FString UavId;
    FString Name;
    int32 RegistrationId = 0;
    bool bOnline = false;
};

inline TArray<FRegisteredAircraft> RegisteredAircraft(const UDroneRegistrySubsystem& Registry,
    const TSharedPtr<FJsonObject>& State)
{
    TArray<FRegisteredAircraft> Result;
    for (const auto& Drone : Registry.GetFriendlyDroneDescriptors())
    {
        if (Drone.Slot <= 0 || Drone.BackendIdString.IsEmpty()) continue;
        const FString Id = FString::Printf(TEXT("UAV-%02d"), Drone.Slot);
        FDroneTelemetrySnapshot Telemetry;
        const bool bObserved = Registry.GetTelemetry(Drone.DroneId, Telemetry);
        if (Aircraft(State, Id) || (bObserved && Telemetry.bSimulation)) continue;
        Result.Add({Id, Drone.Name, Drone.DroneId, bObserved && Telemetry.Availability == EDroneAvailability::Online});
    }
    Result.Sort([](const auto& A, const auto& B) { return A.UavId < B.UavId; });
    return Result;
}
}
