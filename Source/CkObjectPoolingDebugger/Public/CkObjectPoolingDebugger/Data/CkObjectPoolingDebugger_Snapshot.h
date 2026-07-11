#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// ====================================================================================================================
// Pure-data snapshot of the CkCore ObjectPooling subsystem state for one world. Deliberately free of
// any Slate / CkCore-subsystem types in this header so the window only ever sees PODs. Rebuilt each
// gated tick (pool counts are small — no in-place-update machinery needed).
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// One (class, archetype) pool's live stats.
// --------------------------------------------------------------------------------------------------------------------

struct FCkObjectPoolingDebugger_PoolRow
{
    FString ClassName;
    FString ArchetypeName; // "CDO" when the pool keys on the class default object

    int32 NumFree            = 0;
    int32 NumInUse           = 0;
    int32 NumLiveInstances   = 0;
    int32 NumPrewarmRemaining = 0;
    int32 HighWaterMark      = 0;
    int32 NumHits            = 0;
    int32 NumMisses          = 0;
};

// --------------------------------------------------------------------------------------------------------------------
// Full snapshot for the selected world.
// --------------------------------------------------------------------------------------------------------------------

struct FCkObjectPoolingDebugger_Snapshot
{
    bool    HasSubsystem   = false;
    int32   NumPinnedUnique = 0; // DestroyOnRelease instances the subsystem pins (not in any pool)
    TArray<FCkObjectPoolingDebugger_PoolRow> Pools; // sorted by class name, then archetype

    // Gather from the world's ObjectPooling subsystem. Returns HasSubsystem=false when the world has
    // no game instance / the subsystem does not exist (e.g. not in PIE).
    static auto Gather(
        UWorld* InWorld) -> FCkObjectPoolingDebugger_Snapshot;
};

// ====================================================================================================================
