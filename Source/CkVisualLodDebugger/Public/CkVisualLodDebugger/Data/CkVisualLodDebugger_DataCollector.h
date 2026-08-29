#pragma once

#include "CkVisualLodDebugger/Data/CkVisualLodDebugger_Types.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// --------------------------------------------------------------------------------------------------------------------

// Rebuilds a read-only snapshot of every VisualLod arbiter (and the members bound to it) in a world,
// once per gated refresh. Never mutates ECS state.
//
// The snapshot retains FCk_Handles, so Reset() must run on session/world invalidation before the PIE
// registry dies — see CkVisualLodDebugger_Types.h.
class FCkVisualLodDebugger_DataCollector
{
public:
    auto Collect(UWorld* InWorld) -> void;
    auto Reset() -> void;

    auto Get_Snapshot() const -> const FCkVisualLodDebugger_Snapshot&;

private:
    FCkVisualLodDebugger_Snapshot _Snapshot;
};

// --------------------------------------------------------------------------------------------------------------------
