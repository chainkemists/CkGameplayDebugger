#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// ====================================================================================================================
// Pure-data snapshot of the CkCore ObjectPooling subsystem state for one world. Deliberately free of
// any Slate / CkCore-subsystem types in this header so the window only ever sees PODs (pool params
// are flattened to plain fields for the same reason). Rebuilt each gated tick (pool counts are
// small — no in-place-update machinery needed).
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// One (class, archetype) pool's live stats + creation params.
// --------------------------------------------------------------------------------------------------------------------

struct FCkObjectPoolingDebugger_PoolRow
{
    FString ClassName;        // raw (stable pool identity — part of the window's set-signature)
    FString DisplayClassName; // cleaned via ck::DebugNameClean
    FString ArchetypeName;    // "CDO" when the pool keys on the class default object
    bool    IsArchetypeCDO   = true;

    int32 NumFree            = 0;
    int32 NumInUse           = 0;
    int32 NumLiveInstances   = 0;
    int32 NumPrewarmRemaining = 0;
    int32 HighWaterMark      = 0;
    int32 NumHits            = 0;
    int32 NumMisses          = 0;

    // pool params, flattened (see header note)
    bool  IsRecyclePolicy     = true;  // false = DestroyOnRelease (pinned-unique)
    bool  IsBoundedCapacity   = false;
    int32 MaxSize             = 0;     // meaningful only when bounded
    bool  IsGrowOnExhaustion  = true;  // false = Fail
    int32 GrowBatchCount      = 1;
    int32 PrewarmCount        = 0;
    int32 PrewarmBudgetPerTick = 1;

    auto Get_PoolKeyString() const -> FString { return ClassName + TEXT("|") + ArchetypeName; }
    auto Get_TotalAcquires() const -> int32 { return NumHits + NumMisses; }
    auto Get_HitRate() const -> float
    {
        const auto Total = Get_TotalAcquires();
        return Total > 0 ? static_cast<float>(NumHits) / Total : 1.0f;
    }
};

// --------------------------------------------------------------------------------------------------------------------
// Full snapshot for the selected world.
// --------------------------------------------------------------------------------------------------------------------

struct FCkObjectPoolingDebugger_Snapshot
{
    FString WorldName;           // the gathered world's object name (empty when none)
    bool    HasSubsystem   = false;
    int32   NumPinnedUnique = 0; // DestroyOnRelease instances the subsystem pins (not in any pool)
    TArray<FCkObjectPoolingDebugger_PoolRow> Pools; // sorted by class name, then archetype

    // Gather from the world's ObjectPooling subsystem. Returns HasSubsystem=false when the world has
    // no game instance / the subsystem does not exist (e.g. not in PIE).
    static auto Gather(
        UWorld* InWorld) -> FCkObjectPoolingDebugger_Snapshot;

    // Pretty-printed JSON report: top-level metadata + aggregate totals + a per-pool array carrying
    // every raw counter/param plus derived totalAcquires/hitRate. Intended for offline pool tuning
    // (spotting high-miss pools or large pools with low peak usage). Pure — no Slate/subsystem access.
    auto ToJsonString() const -> FString;
};

// ====================================================================================================================
