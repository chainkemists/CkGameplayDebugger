#pragma once

#include "CkAggroDebugger/Data/CkAggroDebugger_Types.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// --------------------------------------------------------------------------------------------------------------------

// Rebuilds a read-only snapshot of every Aggro owner in a world each refresh. Never mutates ECS state.
//
// Aggro is authority-only, so on a client this legitimately collects nothing — that is not an error state and the
// window says so rather than looking broken.
class FCkAggroDebugger_DataCollector
{
public:
    auto Collect(UWorld* InWorld) -> void;
    auto Reset() -> void;

    auto Get_Snapshot() const -> const FCkAggroDebugger_Snapshot&;

private:
    FCkAggroDebugger_Snapshot _Snapshot;
};

// --------------------------------------------------------------------------------------------------------------------
