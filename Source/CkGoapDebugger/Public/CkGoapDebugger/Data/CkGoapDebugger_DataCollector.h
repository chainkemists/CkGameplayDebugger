#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"

// ====================================================================================================================

class UWorld;

// ====================================================================================================================
// CkGoap Debugger — Data Collector.
//
// Snapshots every entity with a Goap root into a TArray<FCkGoapDebugger_EntitySnapshot>
// on demand and maintains a per-entity ring buffer of FCkGoapDebugger_HistoryEvent
// derived from frame-to-frame diffs (chain mutations, plan-status transitions,
// ActionSet enable toggle flips).
//
// Lifetime:
//   - One process-global instance (singleton, all members are file-static).
//   - Initialize/Shutdown are called from the module's StartupModule/ShutdownModule.
//   - PIE BeginPIE/EndPIE clear the per-world prev-snapshot cache + history maps
//     so handles that were valid in the prior session don't leak forward.
//
// Threading: collection runs on the game thread (UI Tick), no async work.
// ====================================================================================================================

class CKGOAPDEBUGGER_API FCkGoapDebugger_DataCollector
{
public:
    // -----------------------------------------------------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------------------------------------------------

    static auto
    Initialize() -> void;

    static auto
    Shutdown() -> void;

    // -----------------------------------------------------------------------------------------------------------------
    // Snapshot collection — pull a fresh batch from the live ECS world.
    // -----------------------------------------------------------------------------------------------------------------

    // Returns a fresh snapshot of every entity that currently has a Goap root.
    // Internally also diffs against the previous tick's batch for the same
    // world to populate the per-entity history rings.
    static auto
    CollectSnapshots(
        UWorld* InWorld) -> TArray<FCkGoapDebugger_EntitySnapshot>;

    // -----------------------------------------------------------------------------------------------------------------
    // History — per-entity ring buffer of FCkGoapDebugger_HistoryEvent.
    // -----------------------------------------------------------------------------------------------------------------

    static auto
    GetHistory(
        const FCk_Handle& InEntityHandle) -> const TArray<FCkGoapDebugger_HistoryEvent>&;

    static auto
    ClearHistory() -> void;

    static auto
    ClearHistoryForEntity(
        const FCk_Handle& InEntityHandle) -> void;
};

// ====================================================================================================================
