#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"

// ====================================================================================================================

class UWorld;

// ====================================================================================================================
// CkGoap Debugger — Data Collector.
//
// Two collection tiers (see CkGoapDebugger_Types.h for the why):
//   - CollectRoster : flat rows for EVERY Goap agent, cheap enough to run each
//                     gated tick at ~150 agents. Maintains the per-entity ring
//                     buffer of FCkGoapDebugger_HistoryEvent from frame-to-frame
//                     roster diffs (chain mutations, plan-status transitions,
//                     replans, WS change-log entries, enable-toggle flips).
//   - CollectFull   : the deep FCkGoapDebugger_EntitySnapshot for ONE entity.
//
// Lifetime:
//   - One process-global instance (singleton, all members are file-static).
//   - Initialize/Shutdown are called from the module's StartupModule/ShutdownModule.
//   - World/session invalidation clears the prev-roster map, the
//     prev-full-selected snapshot, and the history maps so handles that were
//     valid in the prior registry don't leak forward.
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

    static auto
    Reset_ForWorldChange() -> void;

    // -----------------------------------------------------------------------------------------------------------------
    // Snapshot collection — two tiers.
    //
    // Roster : cheap, ALL agents, every gated tick. Flat per-top-level-Planner
    //          rows read straight off fragments — no recursion, no world-state
    //          key scan, no catalog. This is ALSO the single event producer:
    //          it diffs against the previous roster and pushes history events
    //          for every agent.
    // Full   : the deep FCkGoapDebugger_EntitySnapshot for ONE entity — the
    //          Inspector / Catalog / WS-rail / Graph tier.
    // -----------------------------------------------------------------------------------------------------------------

    // InSelectedFull may be null; when an event's entity == the selected entity
    // it is used to attach rich SnapshotAtEvent copies (scrub fidelity for the
    // agent the user is watching). All other events carry a null SnapshotAtEvent.
    static auto
    CollectRoster(
        UWorld* InWorld,
        const FCkGoapDebugger_EntitySnapshot* InSelectedFull) -> TArray<FCkGoapDebugger_RosterEntry>;

    // Full deep snapshot for ONE entity. Unset when the entity carries no Goap
    // role (or the world/entity is invalid).
    static auto
    CollectFull(
        UWorld* InWorld,
        const FCk_Handle& InEntity) -> TOptional<FCkGoapDebugger_EntitySnapshot>;

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
