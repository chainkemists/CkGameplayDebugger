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
// SCkGoapDebugger_PrimaryPane (U11.7-C)
//
// Inspector pane for the currently selected Planner (which may be top-level or
// any sub-Planner — selection is driven by the Sidebar tree). Layout matches
// mockup G:
//
//   ┌────────────────────────────────────────────────────────┬──────────────┐
//   │  Title (DisplayName)  [PLANNER] [ACTION?]              │  Right rail  │
//   │  Tier label · parent name                              │              │
//   │                                                        │              │
//   │  ▸ Status pill · Cost · Attempts · Last replan         │              │
//   │                                                        │              │
//   │  PLAN  ── horizontal strip ──                          │              │
//   │  ┌────┐  ▸  ┌────┐  ▸  ┌────┐                          │              │
//   │  │ #1 │     │ #2 │     │ #3 │                          │              │
//   │  └────┘     └────┘     └────┘                          │              │
//   │  (Plan[0] highlighted ACTIVE)                          │              │
//   │                                                        │              │
//   │  GOAL  ── per-tier conditions                          │              │
//   │  ✓/✗  Key = Value                                      │              │
//   └────────────────────────────────────────────────────────┴──────────────┘
//
// Drilldown is integrated through the PlanStrip — clicking a plan card that
// represents a Planner-role step calls SetSelectedActionSet(handle), which
// updates this pane in place via the normal OnChanged broadcast.
//
// Refresh discipline (CkDebuggerCommon CLAUDE.md):
//   - Cached SBox / SVerticalBox host slots populated at Construct.
//   - SetContent / ClearChildren only in refresh paths — never destructive
//     ChildSlot reassignment.
//   - Structural-hash gate before rebuild.
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
    auto BuildStatusRow(const FCkGoapDebugger_PlannerInfo& InPlanner)       -> TSharedRef<SWidget>;
    auto BuildGoalSection(const FCkGoapDebugger_PlannerInfo& InPlanner)     -> TSharedRef<SWidget>;
    auto BuildRightRail(const FCkGoapDebugger_PlannerInfo& InPlanner)       -> TSharedRef<SWidget>;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

    // Host slots — refreshed by RefreshFromViewModel.
    TSharedPtr<SBox>          _HeaderHost;
    TSharedPtr<SVerticalBox>  _LeftBody;
    TSharedPtr<SBox>          _RightRailHost;

    // Plan strip — owns its own hash gate. Always forward refresh, even when
    // our outer hash didn't change, so live Plan flips still propagate.
    TSharedPtr<SCkGoapDebugger_PlanStrip> _PlanStrip;

    uint32 _LastContentHash = 0;
    bool   _HasMaterialized = false;
};

// ====================================================================================================================
