#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Network/DroneNetworkManager.h"
#include "Command/CommandAlertStore.h"

// Explicit opt-in fixture. Coordinates are labelled synthetic, never uploaded as real telemetry.
static FAutoConsoleCommand P2VisualQA(TEXT("P2.QA"),TEXT("Create labelled P2 synthetic GPS fixture; keep real P1 context channel."),
    FConsoleCommandDelegate::CreateLambda([] {
        if(!GEngine)return;
        for(const auto& C:GEngine->GetWorldContexts()) {
            auto* W=C.World();if(!W || !W->IsGameWorld() || !W->GetGameInstance())continue;
            auto* GI=W->GetGameInstance();GI->GetSubsystem<UDroneNetworkManager>()->SetStrictLocalPreviewIsolation(true);
            FTimerHandle Handle;
            W->GetTimerManager().SetTimer(Handle,FTimerDelegate::CreateWeakLambda(W,[W] {
            auto* GI=W->GetGameInstance();
            auto* R=GI->GetSubsystem<UDroneRegistrySubsystem>();
            for(int32 Id:{1,2,3}) {
                FDroneDescriptor D;D.DroneId=Id;D.Slot=Id;D.Name=FString::Printf(TEXT("P2 QA UAV-%02d / SYNTHETIC"),Id);R->RegisterDrone(D);
                FDroneTelemetrySnapshot T;T.DroneId=Id;T.bGpsFix=true;T.GpsLatitude=39.981+(Id-2)*.003;T.GpsLongitude=116.347+(Id-2)*.006;
                T.GpsAltitude=60;T.LastUpdateTime=FPlatformTime::Seconds();T.BatteryPercent=65+Id;T.Availability=EDroneAvailability::Online;
                R->UpdateTelemetry(Id,T);
            }
            GI->GetSubsystem<UCommandAlertStore>()->AddAlert(2,TEXT("low_battery"),TEXT("P2 QA synthetic alert / not a real UAV"));
            UE_LOG(LogTemp,Log,TEXT("[P2 QA] synthetic GPS fixture ready; real context transport retained"));
            }),3.0f,false);break;
        }
    }));
#endif
