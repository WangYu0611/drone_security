// Copyright Epic Games, Inc. All Rights Reserved.

#include "UE5DroneControl.h"
#include "Modules/ModuleManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "CesiumSunSky.h"
#include "Command/CommandScreenManager.h"
#include "Command/CommandMapInteractionService.h"

class FDroneControlModule final : public FDefaultGameModuleImpl
{
    FDelegateHandle WorldInitializedHandle;
public:
    virtual void StartupModule() override
    {
        FDefaultGameModuleImpl::StartupModule();
        WorldInitializedHandle = FWorldDelegates::OnPostWorldInitialization.AddLambda([](UWorld* World, const UWorld::InitializationValues)
        {
            if (World && World->IsGameWorld() && UCommandScreenManager::ResolveClientRole() == EDroneClientRole::Command)
            {
                TArray<AActor*> MapActors;
                for (TActorIterator<AActor> It(World); It; ++It)
                    if (It->IsA<ACesium3DTileset>() || It->IsA<ACesiumGeoreference>() || It->IsA<ACesiumSunSky>()) MapActors.Add(*It);
                for (AActor* Actor : MapActors) World->DestroyActor(Actor, true);
                UE_LOG(LogTemp, Log, TEXT("[Command] removed %d map source actors before construction; no map service"), MapActors.Num());
            }
            // Uncooked -game runs Cesium construction scripts before GameMode::PreInitializeComponents.
            // Prepare the same world-owned service before those scripts can load heavy 3D sources.
            if (World && World->IsGameWorld() && World->GetMapName().EndsWith(TEXT("CesiumWorld"))
                && UCommandScreenManager::ResolveClientRole() == EDroneClientRole::Map)
                UCommandMapInteractionService::GetOrCreateForWorld(World);
        });
    }
    virtual void ShutdownModule() override
    {
        FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitializedHandle);
        FDefaultGameModuleImpl::ShutdownModule();
    }
};

IMPLEMENT_PRIMARY_GAME_MODULE(FDroneControlModule, UE5DroneControl, "UE5DroneControl");

DEFINE_LOG_CATEGORY(LogUE5DroneControl)
