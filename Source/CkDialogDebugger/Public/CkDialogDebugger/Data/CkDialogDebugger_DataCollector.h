#pragma once

#include "CkDialogDebugger/Data/CkDialogDebugger_Types.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// --------------------------------------------------------------------------------------------------------------------

// Read-only collector: each Collect() rebuilds a snapshot of the world's Dialog registry (lines + banks) and every
// dialog emitter (tags, active cooldowns, query-history ring). Never mutates gameplay state.
class FCkDialogDebugger_DataCollector
{
public:
    auto
    Collect(
        UWorld* InWorld) -> void;

    // Drop the cached snapshot (its handles) — call on PIE teardown so the next session sees no stale handles.
    auto
    Reset() -> void;

    auto
    Get_Snapshot() const -> const FCkDialogDebugger_RegistrySnapshot&;

private:
    FCkDialogDebugger_RegistrySnapshot _Snapshot;
};

// --------------------------------------------------------------------------------------------------------------------
