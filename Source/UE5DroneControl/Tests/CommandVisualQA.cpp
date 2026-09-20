#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Command/CommandAlertStore.h"

// Explicit opt-in visual fixture. Never runs at startup or writes Backend state.
static FAutoConsoleCommand CommandAlertVisualQA(
    TEXT("CommandQA.AlertVisual"), TEXT("Inject clearly labelled QA visual state alerts into the active PIE session."),
    FConsoleCommandDelegate::CreateLambda([]
    {
        if (!GEngine) return;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (!World || World->WorldType != EWorldType::PIE || !World->GetGameInstance()) continue;
            if (auto* Store = World->GetGameInstance()->GetSubsystem<UCommandAlertStore>())
            {
                Store->AddAlert(1, TEXT("info"), TEXT("QA visual state / Information display"));
                Store->AddAlert(2, TEXT("low_battery"), TEXT("QA visual state / Battery warning"));
                Store->AddAlert(3, TEXT("lost_connection"), TEXT("QA visual state / Connection lost"));
            }
            break;
        }
    }));
#endif
