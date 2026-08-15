#pragma once

#include "CkJoltDebugger/Data/CkJoltDebugger_Types.h"

class UWorld;

// --------------------------------------------------------------------------------------------------------------------

/*
 * Flat snapshot pass over the four body-backing features of the selected world. Plain C++ (not a UObject) —
 * the window owns one and pumps it from its refresh-gated Tick.
 *
 * Collect() replaces the whole array every pass; the outliner is what preserves row identity across passes,
 * because SListView keys selection by pointer and the snapshots themselves are values.
 */
class FCkJoltDebugger_DataCollector
{
public:
    auto
    Collect(
        UWorld* InWorld) -> void;

    auto
    Reset() -> void;

    auto
    Get_Bodies() const -> const TArray<FCkJoltDebugger_BodySnapshot>& { return _Bodies; }

    /*
     * Every baked body of every JoltStaticActor, mapped to the entity that owns it. A baked actor contributes
     * N bodies and ONE row, so a click on any body but the first has no row key to match — this is what turns
     * that pick into the actor's row.
     */
    auto
    Get_BakedBodyOwners() const -> const TMap<uint64, FCk_Handle>& { return _BakedBodyOwners; }

private:
    TArray<FCkJoltDebugger_BodySnapshot> _Bodies;
    TMap<uint64, FCk_Handle>             _BakedBodyOwners;
};

// --------------------------------------------------------------------------------------------------------------------
