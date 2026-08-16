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

    /*
     * Stamps the facility's health verdict onto the rows the pass just produced (P8-D57). A SECOND step rather
     * than part of Collect, because the two halves come from different places: the rows are the ECS's, the
     * flags are the capture's, and the collector is forbidden to read JPH for either.
     *
     * A baked-static row inherits the UNION of its own bodies' flags — an actor with one broken body is a row
     * the user has to be able to find.
     */
    auto
    Apply_ProblemFlags(
        const TMap<uint64, ECk_Jolt_DebugDraw_ProblemFlags>& InProblemBodies) -> void;

    /// How many rows the last Apply_ProblemFlags left flagged. What the header badge and the chip both read.
    auto
    Get_NumProblemRows() const -> int32 { return _NumProblemRows; }

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
    int32                                _NumProblemRows = 0;
};

// --------------------------------------------------------------------------------------------------------------------
