#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"

// ====================================================================================================================

class UWorld;

// ====================================================================================================================
// CkGoap Debugger — ViewModel.
//
// Holds the UI's mutable selection state (entity / ActionSet / Action) plus
// the latest snapshot batch pulled from FCkGoapDebugger_DataCollector. The
// debugger window owns one instance and ticks it from its own Tick.
//
// Live vs Scrub:
//   Live   — Tick re-pulls snapshots every frame; selection follows live data.
//   Scrub  — Tick freezes the displayed entity snapshot at _ScrubEventIndex's
//            FCkGoapDebugger_HistoryEvent::SnapshotAtEvent (rendered by UI).
//
// PIE lifetime:
//   FCk_Handle members hold a registry ref by value; clear them on world
//   teardown. SCkGoapDebuggerWindow (D2) will call Reset_ForWorldChange on
//   BeginPIE / EndPIE.
// ====================================================================================================================

class CKGOAPDEBUGGER_API FCkGoapDebugger_ViewModel
{
public:
    enum class EMode : uint8
    {
        Live,
        Scrub
    };

    // -----------------------------------------------------------------------------------------------------------------
    // Delegates
    // -----------------------------------------------------------------------------------------------------------------

    DECLARE_MULTICAST_DELEGATE(FOnViewModelChanged);
    FOnViewModelChanged OnChanged;

    // -----------------------------------------------------------------------------------------------------------------
    // Tick — re-pull snapshots, validate selection, broadcast OnChanged if
    // anything observable from the UI changed.
    // -----------------------------------------------------------------------------------------------------------------

    auto
    Tick(
        UWorld* InWorld) -> void;

    // -----------------------------------------------------------------------------------------------------------------
    // Lifecycle — drop cached snapshots + selection handles so the next PIE
    // session repopulates from a clean slate.
    // -----------------------------------------------------------------------------------------------------------------

    auto
    Reset_ForWorldChange() -> void;

    // -----------------------------------------------------------------------------------------------------------------
    // Selection
    // -----------------------------------------------------------------------------------------------------------------

    auto SetSelectedEntity(FCk_Handle InHandle) -> void;
    auto GetSelectedEntity() const -> FCk_Handle;

    auto SetSelectedActionSet(FCk_Handle_Goap_Planner InHandle) -> void;
    auto GetSelectedActionSet() const -> FCk_Handle_Goap_Planner;

    auto SetSelectedAction(FCk_Handle_Goap_Action InHandle) -> void;
    auto GetSelectedAction() const -> FCk_Handle_Goap_Action;

    // -----------------------------------------------------------------------------------------------------------------
    // Snapshot accessors — read-only. Returns nullptr when no snapshot exists
    // or the selection is stale.
    // -----------------------------------------------------------------------------------------------------------------

    auto GetCurrentEntitySnapshot() const -> const FCkGoapDebugger_EntitySnapshot*;
    auto GetSelectedActionSetInfo() const -> const FCkGoapDebugger_ActionSetInfo*;
    auto GetSelectedActionInfo()    const -> const FCkGoapDebugger_ActionInfo*;

    auto GetAllEntitySnapshots() const -> const TArray<FCkGoapDebugger_EntitySnapshot>&;

    // -----------------------------------------------------------------------------------------------------------------
    // Mode + scrub
    // -----------------------------------------------------------------------------------------------------------------

    auto SetMode(EMode InMode) -> void;
    auto GetMode() const -> EMode;

    auto SetScrubEventIndex(int32 InIndex) -> void;
    auto GetScrubEventIndex() const -> int32;

    // -----------------------------------------------------------------------------------------------------------------
    // Commands — UI buttons that need to mutate ECS state.
    // -----------------------------------------------------------------------------------------------------------------

    // Issues Request_Plan on the currently selected Action. No-op if selection
    // is invalid.
    auto ForceReplanOnSelected() -> void;

private:
    auto BroadcastIfChanged() -> void;

private:
    TArray<FCkGoapDebugger_EntitySnapshot> _AllSnapshots;

    FCk_Handle                _SelectedEntity;
    FCk_Handle_Goap_Planner _SelectedActionSet;
    FCk_Handle_Goap_Action    _SelectedAction;

    EMode _Mode = EMode::Live;
    int32 _ScrubEventIndex = INDEX_NONE;

    // Hash of the last broadcast state — used to suppress no-op OnChanged.
    uint32 _LastBroadcastHash = 0;
    bool   _HasBroadcast      = false;
};

// ====================================================================================================================
