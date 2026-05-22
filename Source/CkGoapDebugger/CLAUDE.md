# CkGoapDebugger — per-Planner debugger UI

> **Read `CkDebuggerCommon/CLAUDE.md` first.** It covers shared conventions: copy-selectable text, entity-ref pills, list/tree row contracts, search bars, PIE world lifecycle, in-world overlays, and safety rules. This file only covers CkGoapDebugger's own architecture.

---

## What this module does

`CkGoapDebugger` is the editor debugger window for the `CkGoap` module. It renders the full Planner/Action tree for any entity selected in the debugger, shows each Planner's live plan strip, world-state source, goal, plan history, and A* search statistics. It opens as a tab via `CK GOAP Debugger` (registered by `CkGoapDebugger_Module`).

---

## UI structure

### Window — `SCkGoapDebuggerWindow`

Top-level Slate widget (`SCkDebugger_WindowBase` subclass). Owns:

- `FCkGoapDebugger_ViewModel _ViewModel` — tick loop + data pipeline.
- `UCkGoapDebugGraph* _Graph` — UEdGraph managing action/goal node layout.
- `STextComboBox* _EntitySelector` — combo box for selecting the entity under inspection.
- `SCkGoapDebug_PlanStrip* _PlanStrip` — hero band below the graph showing the plan as card pills.
- `SCkGoapDebug_HistoryRail* _HistoryRail` — left-side rail of past plan attempts (each entry is an `SCkDebug_HistoryRow`; **not** inside an `SListView` — it lives in a `SVerticalBox` inside `SCkDebug_RailContainer`).
- `SCkGoapDebugger_WorldStatePanel`, `SCkGoapDebugger_StatsPanel`, `SCkGoapDebugger_FailureAnalysisPanel` — right inspector stack.
- `SCkGoapDebug_MacroNodesPanel` — macro-node (goal / action category) panel.
- `SWidgetSwitcher* _TopTabSwitcher` — switches between Graph view and Macro-nodes view.

`Tick` drives `_ViewModel->Tick(World, DeltaTime)` and then calls panel-specific refresh methods.

### ViewModel — `FCkGoapDebugger_ViewModel`

Orchestrates the data pipeline and selection model:

- `_DataCollector` — runs `Collect(World)` each tick (live mode) or returns from history (scrub mode).
- `_SelectedEntityHandle` — the entity currently selected in the combo box.
- `_IsPaused` — when true, `Collect` is skipped; panels read the last snapshot.
- `_ViewMode` (`Live` / `Scrub`) — in Scrub mode, `Get_CurrentGoapInfo()` returns the frozen snapshot at `_ScrubHistoryIndex`.

Multicast delegates: `OnGoapListChanged`, `OnGoapDataRefreshed`, `OnSelectedEntityChanged`, `OnPausedChanged`, `OnViewModeChanged`.

### DataCollector — `FCkGoapDebugger_DataCollector`

Walks all entities on the given `UWorld` each tick, collecting `FCkGoapDebugger_GoapInfo` per entity (plan status, world state entries, action infos, goal infos, A* stats, diagnostics, failure analysis). Maintains rolling plan-history (`_PlanHistory`) and per-frame search-progress log (`_SearchLog`, capped at 120 entries per entity).

---

## Graph — `UCkGoapDebugGraph`

`UEdGraph` subclass. Contains `TArray<TObjectPtr<UCkGoapDebugNode_Action>> _ActionNodes` and `TObjectPtr<UCkGoapDebugNode_Goal> _GoalNode`. Driven by `UpdateFromGoapInfo`:

1. **Topology gate**: `ComputeTopologyHash(InInfo)` compares against `_TopologyHash`. If unchanged, skip `RebuildTopology`; go straight to `UpdateRuntimeState`.
2. **Topology rebuild** (`RebuildTopology`): clears nodes, spawns a `UCkGoapDebugNode_Action` per action in `InInfo.Actions`, spawns the `UCkGoapDebugNode_Goal`, wires dependency edges (effect → precondition, solid pins) and tree edges (parent → child, dashed pins), calls `PerformLayout` to position nodes.
3. **Runtime-state update** (`UpdateRuntimeState`): updates cost tint, in-plan highlighting, status badge — no node reallocation. Returns `bool` (true = changed) for the caller to know whether to invalidate the graph view.

`SetSuppressNotifications(true)` wraps bulk operations to prevent `NotifyGraphChanged` storms. Always suppress during rebuild; re-enable after.

**`UEdGraph::Nodes` iteration rule**: `Nodes` is `TArray<TObjectPtr<UEdGraphNode>>`. Always use explicit `UEdGraphNode*` when iterating:

```cpp
for (UEdGraphNode* Node : Graph->Nodes)
{
    auto* ActionNode = Cast<UCkGoapDebugNode_Action>(Node);
    if (NOT ActionNode) { continue; }
    // ...
}
```

Do not iterate with `auto&` — `TObjectPtr<UEdGraphNode>` does not implicitly supply the underlying `UCkGoapDebugNode_*` cast.

---

## Plan strip — `SCkGoapDebug_PlanStrip`

Hero band rendered below the graph viewport. Subscribes to the ViewModel. Rebuilds only when `ComputeHash()` detects a plan-content change (`_LastHash` comparison).

Renders:
- `[Plan · N steps · cost C]` meta-text.
- A card pill per plan step (`BuildStepPill`), with `bActive = (StepIdx == 0)` visually distinct (highlighted border + `ACTIVE` status pill).
- An arrow widget (`BuildArrow`) between each step.
- A goal pill (`BuildGoalPill`) at the end showing the active goal name.

`OnStepClicked` delegate fires with the step's class name; `SCkGoapDebuggerWindow::OnPlanStepClicked` finds the corresponding graph node and sets selection.

---

## History rail — `SCkGoapDebug_HistoryRail`

Left-side plan history. Each past plan attempt is an `SCkDebug_HistoryRow` inside a `SVerticalBox` (not an `SListView`). `SCkDebug_HistoryRow` is correct here — it's a standalone fixed-rebuild panel, not inside a table row. Selecting a history entry calls `_ViewModel->Set_ViewMode(Scrub)` + `Set_ScrubHistoryIndex(Index)`.

`_LastHash` tracks the history entry count; `RebuildRows()` runs when the hash changes.

---

## Refresh discipline

**Hash-debounce everywhere.** Every panel computes a cheap hash of the data it renders and only rebuilds when the hash changes. No unconditional `ChildSlot[...]` replacements per tick.

**`FCkDebuggerRefreshGate::Should_RefreshNow`** must be honoured in the window `Tick`. If the gate returns false, skip `_ViewModel->Tick` and all panel refresh calls. This respects the user's per-window refresh settings (Use Global / Hz cap / OnlyWhenVisible).

**Topology gate before graph rebuild.** `UCkGoapDebugGraph::UpdateFromGoapInfo` applies a topology hash check before any node reallocation. Cost tint and highlighting update in-place via `UpdateRuntimeState` without touching node count or wiring.

**Stable `TSharedPtr` identity.** History rail row items and any `SListView`/`STreeView` used in future panels must follow the stable-pointer protocol from `CkDebuggerCommon/CLAUDE.md`: reuse existing `TSharedPtr` entries by a stable key, update in-place, only `RequestListRefresh` / `RequestTreeRefresh` when the set actually changes. Reallocating `TSharedPtr` per tick destroys selection state every frame.

---

## Common gotchas

### Anonymous-namespace constant collisions in .cpp files

Multiple `.cpp` files in the same module that each declare `namespace { constexpr auto Duration = ...}` or similar generic names will collide under unity build (C2374/C2086). Use a unique local name (`constexpr auto GraphRebuild_Delay = ...`) or move the constant inside the function body.

### `UEdGraph::Nodes` type

`Nodes` is `TArray<TObjectPtr<UEdGraphNode>>`. Iterating with `for (auto& Node : Nodes)` gives a `TObjectPtr<UEdGraphNode>&`, not a `UCkGoapDebugNode_Action*`. Always dereference explicitly or use `for (UEdGraphNode* Node : Nodes)` — the implicit conversion is present but `Cast<>` on `TObjectPtr` does not compile the same way on all tool builds.

### `SCkDebug_HistoryRow` inside `STableRow` is wrong

`SCkDebug_HistoryRow`'s internal `SButton` returns `FReply::Handled()` on left-click, trapping the click before `STableRow` can process selection. The history rail correctly uses it inside a `SVerticalBox`, not a list view. For any future list/tree panel, use plain `SHorizontalBox` + `STextBlock` inside `STableRow` (see `CkDebuggerCommon/CLAUDE.md` "List / tree rows").

### Refresh thrash — never use `ChildSlot[...]` in hot paths

Replacing `ChildSlot[...]` unconditionally on every tick rebuilds the entire widget subtree, destroying selection state and causing visual flicker. Gate all rebuilds behind a hash check. If the hash matches, return immediately.

---

## Data flow summary

```
UWorld tick
  └── FCkGoapDebugger_ViewModel::Tick
        ├── FCkGoapDebugger_DataCollector::Collect(World)
        │     └── CollectGoapEntity per entity → FCkGoapDebugger_GoapInfo
        │           → TrackPlanCompletion  → _PlanHistory
        │           → TrackSearchProgress → _SearchLog
        ├── Fire OnGoapListChanged / OnGoapDataRefreshed delegates
        └── SCkGoapDebuggerWindow::Tick
              ├── RefreshEntitySelector (if list changed)
              ├── UCkGoapDebugGraph::UpdateFromGoapInfo
              │     ├── ComputeTopologyHash → gate
              │     ├── RebuildTopology (if hash changed) + SuppressNotifications
              │     └── UpdateRuntimeState (always) → tint / highlight
              ├── SCkGoapDebug_PlanStrip::Tick → hash-debounced RebuildStrip
              ├── SCkGoapDebug_HistoryRail::Tick → hash-debounced RebuildRows
              ├── SCkGoapDebugger_WorldStatePanel::Tick → hash-debounced rebuild
              ├── SCkGoapDebugger_StatsPanel::Tick → hash-debounced rebuild
              └── SCkGoapDebugger_FailureAnalysisPanel::Tick → hash-debounced rebuild
```

---

## File layout (Public headers)

```
CkGoapDebugger/
├── Data/
│   ├── CkGoapDebugger_DataCollector.h   (FCkGoapDebugger_DataCollector)
│   └── CkGoapDebugger_Types.h           (FCkGoapDebugger_GoapInfo, HistoryEntry, SearchSnapshot, etc.)
├── Graph/
│   ├── CkGoapDebugGraph.h               (UCkGoapDebugGraph — UEdGraph)
│   ├── CkGoapDebugNode_Action.h         (UCkGoapDebugNode_Action)
│   ├── CkGoapDebugNode_Goal.h           (UCkGoapDebugNode_Goal)
│   ├── CkGoapDebugGraphSchema.h         (schema + right-click copy menus)
│   ├── CkGoapDebugGraphFactory.h        (FGraphPanelNodeFactory)
│   ├── CkGoapDebugConnectionPolicy.h    (connection rules)
│   ├── SGraphNode_GoapAction.h          (Slate node widget)
│   └── SGraphNode_GoapGoal.h            (Slate goal node widget)
├── ViewModel/
│   └── CkGoapDebugger_ViewModel.h       (FCkGoapDebugger_ViewModel)
└── Window/
    ├── SCkGoapDebuggerWindow.h          (top-level window widget)
    ├── SCkGoapDebug_PlanStrip.h         (plan card strip)
    ├── SCkGoapDebug_HistoryRail.h       (plan history left rail)
    ├── SCkGoapDebugger_WorldStatePanel.h
    ├── SCkGoapDebugger_StatsPanel.h
    ├── SCkGoapDebugger_FailureAnalysisPanel.h
    ├── SCkGoapDebugger_GoalPanel.h
    ├── SCkGoapDebugger_PlanView.h
    ├── SCkGoapDebugger_StyleTest.h
    └── MacroNodes/
        ├── SCkGoapDebug_MacroNodesPanel.h
        ├── SCkGoapDebug_ActionRow.h
        ├── SCkGoapDebug_GoalCard.h
        └── CkGoapDebug_ActionCategorizer.h
```

---

## See also

- `CkDebuggerCommon/CLAUDE.md` — cross-debugger conventions (copy text, entity refs, list rows, search bars, PIE lifecycle, PMG overlays, safety rules).
- `CkGoap/CLAUDE.md` (in `CkFoundation`) — the U11 Planner/Action model this debugger reflects.
- Design spec: `docs/superpowers/specs/2026-05-21-CkGoap-PlannerActionCollapse-design.md`.
