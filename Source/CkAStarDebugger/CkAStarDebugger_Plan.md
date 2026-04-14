# CkAStarDebugger — Implementation Plan

## Context

Visual debugger for A* searches, registered in the CkGameplayDebugger plugin alongside CkSmDebugger, CkEcsDebugger, etc. The key differentiator from the SM debugger is a **live grid/graph visualization** showing the A* search state — open set, closed set, current path, blocked cells — all color-coded and updating in real-time.

Console command: `ck.AStarDebugger [0/1]`

---

## Architecture (mirrors CkSmDebugger exactly)

```
Layer 1: Data (DataCollector)
  ├─ Queries ECS for all entities with FFragment_AStar_Debug
  ├─ Reads search state, result, params, debug fragments
  ├─ Builds FCkAStarDebugger_SearchInfo per entity
  └─ Tracks search history (start/complete/fail events over time)

Layer 2: ViewModel (shared state hub)
  ├─ Owns DataCollector
  ├─ Selection state: which entity, which search, view mode
  ├─ Broadcasts delegates: OnSearchListChanged, OnSearchDataRefreshed,
  │   OnSelectedEntityChanged, OnViewModeChanged
  └─ Ticks DataCollector each frame

Layer 3: UI (SCkAStarDebuggerWindow + sub-widgets)
  ├─ Entity selector (dropdown)
  ├─ Grid/Graph view (THE MAIN VISUALIZATION — custom Slate paint)
  ├─ Stats panel (iterations, budget, open/closed sizes, timing)
  ├─ Search history (past searches with results)
  └─ Toolbar (controls, view options)
```

---

## File Structure (absolute paths)

```
D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkAStarDebugger\
├── CkAStarDebugger.Build.cs
├── CkAStarDebugger_Module.h
├── CkAStarDebugger_Module.cpp
├── CkAStarDebugger_Plan.md                                          (this file)
└── Public/CkAStarDebugger/
    ├── CkAStarDebuggerStyle.h                                        (color + dimension constants)
    │
    ├── Data/
    │   ├── CkAStarDebugger_Types.h                                   (FCkAStarDebugger_SearchInfo, etc.)
    │   ├── CkAStarDebugger_DataCollector.h                           (data aggregator)
    │   └── CkAStarDebugger_DataCollector.cpp
    │
    ├── GridView/
    │   ├── SCkAStarDebugger_GridView.h                               (custom Slate widget — THE grid)
    │   └── SCkAStarDebugger_GridView.cpp
    │
    ├── ViewModel/
    │   ├── CkAStarDebugger_ViewModel.h                               (MVVM hub)
    │   └── CkAStarDebugger_ViewModel.cpp
    │
    └── Window/
        ├── SCkAStarDebuggerWindow.h                                  (top-level container)
        ├── SCkAStarDebuggerWindow.cpp
        ├── SCkAStarDebugger_StatsPanel.h                             (stats sidebar)
        ├── SCkAStarDebugger_StatsPanel.cpp
        ├── SCkAStarDebugger_SearchHistory.h                          (history list)
        └── SCkAStarDebugger_SearchHistory.cpp
```

---

## Detailed File Specifications

### CkAStarDebugger.Build.cs
**Path:** `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkAStarDebugger\CkAStarDebugger.Build.cs`

Dependencies: Core, CoreUObject, Engine, InputCore, Slate, SlateCore, WorkspaceMenuStructure, EditorStyle, AppFramework, ToolMenus, CkCore, CkEcs, CkAStar, CkDebuggerCommon. Conditional: UnrealEd.

**Already created.**

### CkAStarDebugger_Module.h/.cpp
**Path:** `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkAStarDebugger\CkAStarDebugger_Module.h`
**Path:** `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkAStarDebugger\CkAStarDebugger_Module.cpp`

Exact mirror of CkSmDebugger_Module:
- `StartupModule()`: Register NomadTabSpawner "CkAStarDebugger" in Developer Tools > Debug
- `ShutdownModule()`: Unregister tab spawner, reset window/tab
- Console command: `ck.AStarDebugger [0/1]` — open/close/toggle
- `OnSpawnDebuggerTab()`: Creates SCkAStarDebuggerWindow in SDockTab

**Stub already created.** Needs full implementation.

### CkAStarDebuggerStyle.h
**Path:** `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkAStarDebugger\Public\CkAStarDebugger\CkAStarDebuggerStyle.h`

Static inline constants in `namespace FCkAStarDebuggerStyle`:

**Grid Cell Colors:**
- `EmptyCell` — dark gray (#1A1A2E)
- `BlockedCell` — dark red (#3D1111)
- `OpenSetCell` — blue (#1E4D8C)
- `ClosedSetCell` — slate (#2D3748)
- `PathCell` — bright green (#22C55E)
- `StartCell` — cyan (#06B6D4)
- `GoalCell` — gold (#F59E0B)
- `CurrentExpandNode` — bright amber (#FBBF24)

**Grid Rendering:**
- `GridLineColor` — subtle (#2A2A3E)
- `GridBorderColor` — medium (#4A5568)
- `CellSize` = 16 pixels (default)
- `CellGap` = 1 pixel
- `MinCellSize` = 4 pixels (zoomed out)
- `MaxCellSize` = 48 pixels (zoomed in)

**Stats & UI:**
- Match CkSmDebuggerStyle text/background patterns
- `BudgetBarFull` — green, `BudgetBarOver` — red
- `CacheHit` — green, `CacheMiss` — amber

### CkAStarDebugger_Types.h
**Path:** `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkAStarDebugger\Public\CkAStarDebugger\Data\CkAStarDebugger_Types.h`

**FCkAStarDebugger_SearchInfo** — per-entity collected data:
- `FCk_Handle EntityHandle` — the ECS entity
- `FString DebugName` — entity debug name
- `ECk_AStarSearchStatus SearchStatus`
- `int32 GridWidth, GridHeight` — if grid graph (0 if unknown graph type)
- `TArray<int32> Path` — current result path
- `float TotalCost`
- `int32 TotalIterations`
- `int64 TotalTimeMicroseconds`
- `int32 OpenSetSize, ClosedSetSize`
- `int32 IterationsThisFrame`
- `int64 TimeThisFrameMicroseconds`
- `float BudgetUsagePercent`
- `int64 BudgetMicroseconds`
- `float CostThreshold`
- `TSet<int32> BlockedCells` — for grid rendering
- `TSet<int32> OpenSetCells` — approximation for visualization (if debug data supports it)
- `TSet<int32> ClosedSetCells` — approximation for visualization

**FCkAStarDebugger_HistoryEntry** — search event:
- `double WallTime`
- `int32 FrameNumber`
- `ECk_AStarSearchStatus FinalStatus`
- `int32 TotalIterations`
- `int64 TotalTimeMicroseconds`
- `float TotalCost`
- `int32 PathLength`

### CkAStarDebugger_DataCollector.h/.cpp
**Path:** `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkAStarDebugger\Public\CkAStarDebugger\Data\CkAStarDebugger_DataCollector.h`

**FCkAStarDebugger_DataCollector:**
- `Collect(UWorld*)` — main entry. Iterates all entities with `FFragment_AStar_Debug`. Reads debug fragment, result fragment, params fragment. For test entities, also reads `FFragment_AStarTest_GridGraph` and `FFragment_AStarTest_SearchState` for open/closed set visualization.
- `Get_AllSearchEntities()` — returns collected array
- `CollectSearchEntity(FCk_Handle)` — per-entity collection
- Tracks search completion events → appends to history

**Key Design:** The data collector needs to read the open/closed set from `TSearchState` for visualization. Since `TSearchState` is templated, the collector can only read it for known concrete types (e.g., the test grid graph). For unknown consumer types, it falls back to just `FFragment_AStar_Debug` stats (no cell-level visualization). This is acceptable — consumers can register their own data providers later.

### CkAStarDebugger_ViewModel.h/.cpp
**Path:** `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkAStarDebugger\Public\CkAStarDebugger\ViewModel\CkAStarDebugger_ViewModel.h`

**FCkAStarDebugger_ViewModel:**

Delegates:
- `OnSearchListChanged` — when entity list changes
- `OnSearchDataRefreshed` — when selected entity's data updates
- `OnSelectedEntityChanged` — entity selection changed
- `OnViewModeChanged` — live vs paused

State:
- `_SelectedEntityHandle` — which entity to debug
- `_DataCollector` — owned
- `_SearchHistory` — per-entity history (TMap<FCk_Handle, TArray<FCkAStarDebugger_HistoryEntry>>)

API:
- `Tick(UWorld*, float)` — calls DataCollector.Collect(), broadcasts
- `Set/Get_SelectedEntityHandle()`
- `Get_AllSearchEntities()`
- `Get_CurrentSearchInfo()`
- `Get_SearchHistory()`

### SCkAStarDebugger_GridView.h/.cpp
**Path:** `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkAStarDebugger\Public\CkAStarDebugger\GridView\SCkAStarDebugger_GridView.h`

**SCkAStarDebugger_GridView : SLeafWidget** — THE core visual widget.

Custom low-level Slate renderer (OnPaint). Renders:
1. **Grid cells** as colored rectangles (empty, blocked, open set, closed set, path, start, goal)
2. **Path overlay** as connected line segments through cell centers
3. **Cell hover tooltip** showing node index, g-score, f-score, parent
4. **Zoom** via mouse wheel (CellSize scales between MinCellSize and MaxCellSize)
5. **Pan** via right-click drag
6. **Legend** at bottom showing color meanings

Input handling:
- Mouse wheel → zoom
- Right-click drag → pan
- Left-click → select cell (shows detail in stats panel)
- Hover → tooltip

Data input (set by Window each frame):
- `SetSearchInfo(const FCkAStarDebugger_SearchInfo&)` — updates grid data
- Grid dimensions, blocked cells, open/closed sets, path, start/goal

### SCkAStarDebuggerWindow.h/.cpp
**Path:** `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkAStarDebugger\Public\CkAStarDebugger\Window\SCkAStarDebuggerWindow.h`

**SCkAStarDebuggerWindow : SCompoundWidget** — top-level container.

Layout (responsive):
```
┌─────────────────────────────────────────────────────┐
│ [Entity Selector ▼]  [Pause] [Step] [Reset]  [Zoom] │  ← Toolbar
├───────────────────────────────┬─────────────────────┤
│                               │  Status: InProgress │
│                               │  Iterations: 847    │
│      GRID / GRAPH VIEW        │  Open Set: 123      │
│    (SCkAStarDebugger_         │  Closed Set: 724    │
│         GridView)             │  Budget: 50us (87%) │
│                               │  Cost: 18.0         │
│                               │  Time: 43us         │
│                               ├─────────────────────┤
│                               │  Search History     │
│                               │  ─────────────────  │
│                               │  #1 Complete 18.0   │
│                               │  #2 InProgress...   │
└───────────────────────────────┴─────────────────────┘
```

Composes:
- Entity selector dropdown (mirrors SM debugger's SM selector)
- Toolbar with controls
- GridView (left, 70% width)
- StatsPanel (right-top, 30% width)
- SearchHistory (right-bottom, 30% width)
- ViewModel (shared with sub-widgets)

### SCkAStarDebugger_StatsPanel.h/.cpp
**Path:** `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkAStarDebugger\Public\CkAStarDebugger\Window\SCkAStarDebugger_StatsPanel.h`

**SCkAStarDebugger_StatsPanel : SCompoundWidget**

Displays:
- Search status (with color indicator)
- Iterations (total + this frame)
- Open/Closed set sizes
- Budget usage (bar + percentage)
- Cost threshold
- Total time
- Path length
- Grid dimensions
- Selected cell detail (if a cell is clicked in GridView)

### SCkAStarDebugger_SearchHistory.h/.cpp
**Path:** `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkAStarDebugger\Public\CkAStarDebugger\Window\SCkAStarDebugger_SearchHistory.h`

**SCkAStarDebugger_SearchHistory : SCompoundWidget**

Scrollable list of past search events:
- Frame number, wall time
- Final status (Complete/Failed/CostThreshold) with color
- Iterations, time, cost, path length
- Click to inspect (future: scrub to historical state)

---

## Registration

**uplugin:** `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\CkDebugger.uplugin`
Module entry already added: `CkAStarDebugger`, Type: `UncookedOnly`.

---

## Implementation Sequence

1. **Module bootstrap**: Implement CkAStarDebugger_Module.cpp (tab spawner, console command)
2. **Style**: Fill CkAStarDebuggerStyle.h with color/dimension constants
3. **Types**: Fill CkAStarDebugger_Types.h with data structs
4. **DataCollector**: Implement ECS data collection
5. **ViewModel**: Implement MVVM hub with delegates
6. **GridView**: Implement custom Slate OnPaint renderer (THE hardest part)
7. **StatsPanel**: Implement stats display
8. **SearchHistory**: Implement history list
9. **Window**: Compose all sub-widgets
10. **Test**: Open debugger, run A* gym, verify visualization

---

## Reference Files

| Component | Reference (SM Debugger) |
|-----------|------------------------|
| Module | `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkSmDebugger\CkSmDebugger_Module.cpp` |
| Build.cs | `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkSmDebugger\CkSmDebugger.Build.cs` |
| DataCollector | `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkSmDebugger\Public\CkSmDebugger\Data\CkSmDebugger_DataCollector.h` |
| ViewModel | `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkSmDebugger\Public\CkSmDebugger\ViewModel\CkSmDebugger_ViewModel.h` |
| Window | `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkSmDebugger\Public\CkSmDebugger\Window\SCkSmDebuggerWindow.h` |
| Style | `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkSmDebugger\Public\CkSmDebugger\CkSmDebuggerStyle.h` |
| Timeline (custom paint) | `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkSmDebugger\Public\CkSmDebugger\Window\SCkSmDebugger_Timeline.h` |
| Types | `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkSmDebugger\Public\CkSmDebugger\Data\CkSmDebugger_Types.h` |

---

## Mockups

See the 3 HTML mockup variations at:
- `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkAStarDebugger\Mockups\variation_a_grid_focus.html`
- `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkAStarDebugger\Mockups\variation_b_split_panels.html`
- `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkAStarDebugger\Mockups\variation_c_dashboard.html`
