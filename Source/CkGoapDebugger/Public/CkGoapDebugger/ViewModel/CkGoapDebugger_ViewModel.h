#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"

// ====================================================================================================================

class UWorld;

// ====================================================================================================================
// CkGoap Debugger — ViewModel.
//
// Holds the UI's mutable selection state (entity / Planner / Action) plus
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

    // U11.7-C: per-Planner snapshot accessor. The Sidebar drives selection via
    // SetSelectedActionSet(PlannerHandle) for both top-level Planners and
    // every nested sub-Planner. This walker descends the TopLevelPlanners
    // forest recursively, so an arbitrary sub-Planner selection resolves
    // without going through the legacy ActionSets[] shim (which only carries
    // top-level entries).
    auto GetSelectedPlannerInfo() const -> const FCkGoapDebugger_PlannerInfo*;
    auto GetPlannerInfoByHandle(FCk_Handle_Goap_Planner InHandle) const -> const FCkGoapDebugger_PlannerInfo*;

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

    // Force an OnChanged broadcast — used when a non-snapshot piece of UI
    // state (e.g., name-depth verbosity) changes and every subscribed pane
    // needs to re-render.
    auto Broadcast_Changed() -> void { OnChanged.Broadcast(); }

    // -----------------------------------------------------------------------------------------------------------------
    // UI presentation state — shared across panes that render class names.
    // The graph pane's name-depth toolbar mutates this through Set_NameDepth
    // then calls Broadcast_Changed so the sidebar / plan strip / primary pane
    // / breadcrumb / history rail all re-render with the new verbosity.
    //
    // Depth semantics (mirrors SCkDebug_NameLabel::Get_ShortName):
    //   0 = full joined name (every segment), 1 = leaf, 2 = last two, ...
    // -----------------------------------------------------------------------------------------------------------------

    auto Get_NameDepth() const -> int32 { return _NameDepth; }
    auto Set_NameDepth(int32 InDepth) -> void { _NameDepth = FMath::Max(0, InDepth); }

    // -----------------------------------------------------------------------------------------------------------------
    // Key trace — cross-pane highlight of one WS key. The WS rail sets it on
    // row click; decision cards / plan chips / graph nodes highlight every
    // condition chip referencing the traced key. Empty tag = no trace.
    // -----------------------------------------------------------------------------------------------------------------

    auto Get_TracedWsKey() const -> FGameplayTag { return _TracedWsKey; }
    auto Set_TracedWsKey(FGameplayTag InKey) -> void
    {
        if (_TracedWsKey == InKey) { return; }
        _TracedWsKey = InKey;
        Broadcast_Changed();
    }

private:
    auto BroadcastIfChanged() -> void;

private:
    TArray<FCkGoapDebugger_EntitySnapshot> _AllSnapshots;

    FCk_Handle                _SelectedEntity;
    FCk_Handle_Goap_Planner _SelectedActionSet;
    FCk_Handle_Goap_Action    _SelectedAction;

    EMode _Mode = EMode::Live;
    int32 _ScrubEventIndex = INDEX_NONE;

    // Cross-pane WS key trace (see Get_TracedWsKey). Plain tag — no handle,
    // so no EndPIE clearing needed.
    FGameplayTag _TracedWsKey;

    // Hash of the last broadcast state — used to suppress no-op OnChanged.
    uint32 _LastBroadcastHash = 0;
    bool   _HasBroadcast      = false;

    // UI presentation state — class-name verbosity. Owned here so every pane
    // that renders class names converges on one source of truth, and the
    // graph pane's toolbar control becomes the single editor for it.
    int32 _NameDepth = 1;
};

// ====================================================================================================================
