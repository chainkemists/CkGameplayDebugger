#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

class FCkGoapDebugger_ViewModel;
class SCkGoapDebugger_PlanStrip;
class SVerticalBox;
class STextBlock;
class SBox;

// ====================================================================================================================
// SCkGoapDebugger_PrimaryPane — Mockup B (dashboard) layout
//
// Inspector for the currently selected Planner (top-level or sub-Planner —
// selection is driven by the Sidebar tree). Layout:
//
//   ┌──────────────────────────────────────────────────────────────────────┐
//   │  Title  [PLANNER]  [ACTION?]  [OPT-OUT?]              <tag mono>     │  ← compact header
//   ├──────────────────────────────────────────────────────────────────────┤
//   │  ╔════════════════ PLAN  (full-width strip) ════════════════════════╗ │
//   │  ║  [1 ACTION ACTIVE  Standby]  >  [2 PLANNER  GoToWaypoint] ...   ║ │
//   │  ╚══════════════════════════════════════════════════════════════════╝ │
//   │                                                                      │
//   │  ┌──── IDENTITY & STATUS ────┬── GOAL ──────┬── DIAGNOSTICS & WIRING ┐│
//   │  │ ●PlanFound $999 1att 3ch  │ ✗ Goal=true  │ KV grid                ││
//   │  └───────────────────────────┴──────────────┴────────────────────────┘│
//   └──────────────────────────────────────────────────────────────────────┘
//
// Drilldown is integrated through the PlanStrip — clicking a plan card that
// represents a Planner-role step calls SetSelectedActionSet(handle), which
// updates this pane in place via the normal OnChanged broadcast.
//
// Refresh discipline (CkDebuggerCommon CLAUDE.md):
//   - Cached SBox host slots populated at Construct; never reassigned by
//     RefreshFromViewModel. Refresh swaps content via SetContent only.
//   - PlanStrip is created once in Construct (its own hash gate handles
//     content debouncing) and lives inside _PlanTileHost.
//   - Structural-hash gate around the tile rebuilds (BuildXxxTile).
// ====================================================================================================================

class CKGOAPDEBUGGER_API SCkGoapDebugger_PrimaryPane : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGoapDebugger_PrimaryPane) {}
        SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    ~SCkGoapDebugger_PrimaryPane();

    // Called by the parent window on ViewModel::OnChanged.
    auto RefreshFromViewModel() -> void;

private:
    auto BuildEmptyState()                                                  -> TSharedRef<SWidget>;
    auto BuildHeader(const FCkGoapDebugger_PlannerInfo& InPlanner)          -> TSharedRef<SWidget>;
    auto BuildPlanTile(const FCkGoapDebugger_PlannerInfo& InPlanner)        -> TSharedRef<SWidget>;
    auto BuildIdentityStatusTile(const FCkGoapDebugger_PlannerInfo& InPlanner) -> TSharedRef<SWidget>;
    auto BuildGoalTile(const FCkGoapDebugger_PlannerInfo& InPlanner)        -> TSharedRef<SWidget>;
    auto BuildWiringTile(const FCkGoapDebugger_PlannerInfo& InPlanner)      -> TSharedRef<SWidget>;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

    // Host slots — content swapped by RefreshFromViewModel via SetContent.
    TSharedPtr<SBox> _HeaderHost;
    TSharedPtr<SBox> _PlanTileHost;
    TSharedPtr<SBox> _IdentityTileHost;
    TSharedPtr<SBox> _GoalTileHost;
    TSharedPtr<SBox> _WiringTileHost;

    // PlanStrip — owns its own hash gate. Created once in Construct so the
    // gate has stable state to debounce against.
    TSharedPtr<SCkGoapDebugger_PlanStrip> _PlanStrip;

    uint32 _LastContentHash = 0;
    bool   _HasMaterialized = false;
};

// ====================================================================================================================
