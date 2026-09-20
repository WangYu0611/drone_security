#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Network/DroneNetworkManager.h"
#include "Command/CommandAlertStore.h"

// Explicit local development fixture; never automatic, never sends mission/telemetry to Backend.
static FAutoConsoleCommand M1VisualQA(TEXT("M1.QA"), TEXT("Create labelled local QA descriptors, telemetry and alerts for physical acceptance."),
    FConsoleCommandDelegate::CreateLambda([]
    {
        if (!GEngine) return;
        for (const auto& C : GEngine->GetWorldContexts())
        {
            auto* W = C.World(); if (!W || !W->IsGameWorld() || !W->GetGameInstance()) continue;
            auto* GI = W->GetGameInstance();
            GI->GetSubsystem<UDroneNetworkManager>()->SetStrictLocalPreviewIsolation(true);
            auto* R = GI->GetSubsystem<UDroneRegistrySubsystem>();
            for (int32 Id : {1, 2, 3})
            {
                FDroneDescriptor D; D.DroneId = Id; D.Name = FString::Printf(TEXT("M1 QA UAV %02d"), Id); D.Slot = Id;
                R->RegisterDrone(D);
                FDroneTelemetrySnapshot T; T.DroneId = Id; T.BatteryPercent = 60 + Id;
                T.Availability = EDroneAvailability::Offline; R->UpdateTelemetry(Id, T);
                FDroneTaskStateSnapshot M; M.ArrayId = TEXT("M1 LOCAL QA"); R->UpdateTaskState(Id, M);
            }
            GI->GetSubsystem<UCommandAlertStore>()->AddAlert(2, TEXT("low_battery"), TEXT("M1 QA visual state / simulated battery alert"));
            UE_LOG(LogTemp, Log, TEXT("[M1 QA] local fixture ready; Backend isolated; no real UAV acceptance"));
            break;
        }
    }));
#endif
