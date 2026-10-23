#pragma once
#include "CoreMinimal.h"

namespace MapMove {
// Frozen when a destination is previewed. A later snapshot must not silently
// retarget a confirmation to a new task or downgrade takeover to ordinary move.
struct FTakeoverIntent {
    FString ExecutionId,UavId;
    int64 ControlVersion=0;
    bool IsSet() const { return !ExecutionId.IsEmpty(); }
};
}
