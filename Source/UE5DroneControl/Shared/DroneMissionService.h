#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DroneMissionService.generated.h"

/** Per-process mission requests. No world actors, camera or map dependencies. */
UCLASS()
class UE5DRONECONTROL_API UDroneMissionService : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    bool CanPauseSelected() const;
    bool SetSelectedPaused(bool bPaused);
};
