# CkSchedulerDebugger — Implementation Plan

## Context

We're implementing a Slate-based debugger for the ECS Processor Scheduler. The approved mockup (`mockup_unified.html`) has three tabs sharing selection state: **Tree View** (tri-pane), **Timeline** (waterfall profiler), and **Combined** (both stacked). The debugger will be a new module at `CkGameplayDebugger/Source/CkSchedulerDebugger`, following patterns established by the existing ECS and SM debuggers.

## Shorthand

- `$SD` = `D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkSchedulerDebugger`
- `$CF` = `D:\Repos\CkPlugins\Plugins\CkFoundation\Source\CkEcs\Public\CkEcs`

---

## Phase 1: Scheduler Instrumentation (CkFoundation changes)

Expose scheduler internals for reading, and add timing instrumentation.

### 1A. Expose `_Scheduler` on the world actor

**Modify**: `$CF/Subsystem/CkEcsWorld_Subsystem.h`
- Add `CK_PROPERTY_GET(_Scheduler)` on `ACk_EcsWorld_Actor_UE` (line ~74)
- Access path then becomes: `Subsystem->Get_WorldActors()` → per-actor `Get_Scheduler()` → `const TOptional<FProcessorScheduler>&`
- `_WorldActors` already has `CK_PROPERTY_GET` (line 116)

### 1B. Expose `_Partition` on the scheduler

**Modify**: `$CF/Scheduler/CkProcessorScheduler.h`
- Add `CK_PROPERTY_GET(_Partition)` on `FProcessorScheduler`
- Graph node fields (`_ProcessorName`, `_InEdges`, etc.) are already public on the struct — no changes needed

### 1C. Expose `_TotalTicks` on processors

**Modify**: `$CF/Processor/CkProcessor.h`
- Add `CK_PROPERTY_GET(_TotalTicks)` on `TProcessorBase`

### 1D. Add timing instrumentation

**New file**: `$CF/Scheduler/CkSchedulerDebugData.h`

```cpp
struct FSchedulerDebug_ProcessorTiming
{
    FName ProcessorName;
    double MainPassTimeMs = 0.0;
    TArray<double> PumpPassTimesMs;  // one entry per pump pass this processor was invoked
    bool WasDirtyThisFrame = false;
    int32 PumpCountThisFrame = 0;
};

struct FSchedulerDebug_FrameSnapshot
{
    TArray<FSchedulerDebug_ProcessorTiming> ProcessorTimings;  // indexed by node index
    int32 PumpIterationCount = 0;
    double TotalFrameTimeMs = 0.0;
    uint64 FrameNumber = 0;
};
```

**Modify**: `$CF/Scheduler/CkProcessorScheduler.h`
- Add `TArray<FSchedulerDebug_FrameSnapshot> _DebugFrameHistory` (circular buffer, max 300) — guarded by `#if !UE_BUILD_SHIPPING`
- Add `CK_PROPERTY_GET(_DebugFrameHistory)`

**Modify**: `$CF/Scheduler/CkProcessorScheduler.cpp`
- In `Tick()`: wrap each processor's `Tick()` call with:
  1. `FPlatformTime::Seconds()` before/after → stored in debug snapshot (guarded by `#if !UE_BUILD_SHIPPING`)
  2. `FScopeCycleCounter` using per-processor stat IDs → integrates with Unreal Insights/stat system (always active in non-shipping)
- In pump loop: same dual instrumentation for pumped processors
- Push completed snapshot into `_DebugFrameHistory`
- `FPlatformTime` data is for the debugger's own display; `FScopeCycleCounter` is for Insights integration
- Both guarded by `#if !UE_BUILD_SHIPPING`

---

## Phase 2: Module Skeleton

### 2A. Build configuration

**New**: `$SD/CkSchedulerDebugger.Build.cs`
- Extends `CkModuleRules`
- Depends on: Core, CoreUObject, Engine, InputCore, Slate, SlateCore, GraphEditor, WorkspaceMenuStructure, EditorStyle, AppFramework, ToolMenus, GameplayTags, CkCore, CkEcs
- Conditional: UnrealEd (editor only)

### 2B. Module class

**New**: `$SD/CkSchedulerDebugger_Module.h` + `.cpp`
- `FCkSchedulerDebuggerModule : public IModuleInterface`
- Console command: `ck.SchedulerDebugger [0/1]`
- `StartupModule()`: register graph node factory + nomad tab spawner
- `ShutdownModule()`: unregister both
- Tab spawner creates `SDockTab` containing `SCkSchedulerDebuggerWindow`

### 2C. Style constants

**New**: `$SD/Public/CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h`
- Inline namespace pattern (like `FCkSmDebuggerStyle`)
- Group accent colors (9 groups), timing heat colors, node state colors

### 2D. Plugin manifest

**Modify**: `CkGameplayDebugger/CkDebugger.uplugin`
- Add `CkSchedulerDebugger` module entry: `Type: "UncookedOnly"`, `LoadingPhase: "Default"`

---

## Phase 3: Data Layer

### 3A. Types

**New**: `$SD/Public/CkSchedulerDebugger/Data/CkSchedulerDebugger_Types.h`
- `FCkSchedulerDebugger_ProcessorInfo`: name, display name, node index, edges, tick group, group, flags, timing history
- `FCkSchedulerDebugger_GroupInfo`: name, tick group, member indices, color
- `FCkSchedulerDebugger_TreeNode`: type enum (TickGroup/Group/Processor), display name, children, parent, visibility

### 3B. Data collector

**New**: `$SD/Public/CkSchedulerDebugger/Data/CkSchedulerDebugger_DataCollector.h` + `.cpp`
- Finds PIE world via `GEngine->GetWorldContexts()`
- Gets `UCk_EcsWorld_Subsystem_UE` → iterates `Get_WorldActors()`
- For each actor: `Get_Scheduler()` → `Get_Partition()` → reads nodes, execution order, timing
- Builds tree hierarchy: Root → TickGroup → Group → Processor
- Identifies groups by scanning `IsGroupStart`/`IsGroupEnd` nodes

### 3C. ViewModel

**New**: `$SD/Public/CkSchedulerDebugger/ViewModel/CkSchedulerDebugger_ViewModel.h` + `.cpp`
- Owns `DataCollector`
- Delegates: `OnDataRefreshed`, `OnSelectionChanged`
- Shared selection: `_SelectedProcessorIndex` (synced across all tabs)
- Freeze toggle: stops data collection when frozen
- `Tick(UWorld*, float)`: calls collector, broadcasts

---

## Phase 4: Tree View Tab

### 4A. Page interface

**New**: `$SD/Public/CkSchedulerDebugger/Pages/ICkSchedulerDebuggerPage.h`
- `Get_PageName()`, `Build_Content()`, `Tick()`, `IsActive()`, `Set_IsActive()`

### 4B. Main window

**New**: `$SD/Public/CkSchedulerDebugger/Window/SCkSchedulerDebuggerWindow.h` + `.cpp`
- Top bar: title, world selector, freeze button
- Stats bar: frame time, pump count, processor/ghost/dirty/parallel counts
- Tab buttons: Tree View / Timeline / Combined
- Content area: swaps page content (pages built once, shown/hidden)
- `Tick()`: finds PIE world, calls `ViewModel->Tick()`

### 4C. Processor tree widget

**New**: `$SD/Public/CkSchedulerDebugger/Widgets/SCkSchedulerDebugger_ProcessorTree.h` + `.cpp`
- Search bar with debounce
- Sort dropdown (Exec Order / Name / Timing)
- `STreeView<TSharedPtr<FCkSchedulerDebugger_TreeNode>>`
- Custom row: exec order #, name, timing badge (heat-colored), dirty/parallel/ghost badges
- Group rows: accent color, child count, aggregate timing
- Bottom section: "Pump This Frame" log
- Selection calls `ViewModel->Set_SelectedProcessorIndex()`

### 4D. Inspector panel

**New**: `$SD/Public/CkSchedulerDebugger/Widgets/SCkSchedulerDebugger_Inspector.h` + `.cpp`
- Sections: Processor Info, Timing, Dependencies (clickable), Fragments
- Sparkline placeholder (filled in Phase 8)
- Binds to ViewModel selection/data delegates

### 4E. Tree View page

**New**: `$SD/Public/CkSchedulerDebugger/Pages/CkSchedulerDebuggerPage_TreeView.h` + `.cpp`
- Horizontal `SSplitter`: tree (0.25) | center (0.5) | inspector (0.25)
- Center has sub-tabs: "Detail Graph" (mini SVG neighbor graph) and "Full Graph" (Phase 5)

---

## Phase 5: Graph View (Full DAG)

### 5A. Graph node (UEdGraphNode subclass)

**New**: `$SD/Public/CkSchedulerDebugger/Graph/CkSchedulerDebugNode_Processor.h` + `.cpp`
- Fields: ProcessorNodeIndex, ProcessorName, GroupName, IsGhost, timing, pump data
- `AllocateDefaultPins()`: one input, one output

### 5B. Graph schema (read-only)

**New**: `$SD/Public/CkSchedulerDebugger/Graph/CkSchedulerDebugGraphSchema.h` + `.cpp`
- `CanCreateConnection` → disallow, `TryCreateConnection` → false
- Empty context menus
- Returns custom connection drawing policy

### 5C. Graph (UEdGraph subclass)

**New**: `$SD/Public/CkSchedulerDebugger/Graph/CkSchedulerDebugGraph.h` + `.cpp`
- `RebuildFromData()`: creates nodes from snapshot, uses topology hash to avoid unnecessary rebuilds
- `UpdateTimingData()`: hot-path update for timing/dirty fields only
- Layout: Sugiyama-style layered approach with crossing minimization

### 5D. Custom SGraphNode

**New**: `$SD/Public/CkSchedulerDebugger/Graph/SGraphNode_SchedulerProcessor.h` + `.cpp`
- Rounded box with group-colored accent strip
- Name, timing badge, dirty/parallel/ghost indicators
- Selected: blue border, upstream/downstream edge highlighting

### 5E. Connection drawing policy

**New**: `$SD/Public/CkSchedulerDebugger/Graph/CkSchedulerDebugConnectionPolicy.h` + `.cpp`
- Straight arrows, default grey
- Selected node's edges highlighted (blue upstream, green downstream)

### 5F. Node factory

**New**: `$SD/Public/CkSchedulerDebugger/Graph/CkSchedulerDebugGraphFactory.h` + `.cpp`
- Maps `UCkSchedulerDebugNode_Processor` → `SGraphNode_SchedulerProcessor`

---

## Phase 6: Timeline Tab

### 6A. Timeline widget

**New**: `$SD/Public/CkSchedulerDebugger/Widgets/SCkSchedulerDebugger_Timeline.h` + `.cpp`
- `SLeafWidget` with custom `OnPaint`
- Y-axis: swim lanes per group (collapsible)
- X-axis: time (proportional), zoomable via mouse wheel
- Bars: group-colored with heat tint, proportional width, name/timing/badges
- Pump sections: tinted background, dashed separator
- Ghost bars: hatched pattern, dimmed
- Click to select, hover tooltip

### 6B. Timeline page

**New**: `$SD/Public/CkSchedulerDebugger/Pages/CkSchedulerDebuggerPage_Timeline.h` + `.cpp`
- Vertical split: timeline (0.65) | bottom panel (0.35)
- Bottom: pump breakdown (0.6) | inspector (0.4)

---

## Phase 7: Combined Tab

**New**: `$SD/Public/CkSchedulerDebugger/Pages/CkSchedulerDebuggerPage_Combined.h` + `.cpp`
- Vertical `SSplitter`: tree pane (top) | timeline (bottom)
- Layout toggle buttons: Split (50/50), Tree Focus (70/30), Timeline Focus (30/70)
- Shared inspector sidebar on right

---

## Phase 8: Polish (incremental)

- **Sparkline widget**: `SCkSchedulerDebugger_Sparkline.h/.cpp` — `SLeafWidget` mini line graph of last 300 frames
- **Heat map toggle**: tint graph nodes by timing
- **Freeze mode**: pulsing "FROZEN" indicator, stops data collection
- **Sort options**: dropdown in tree toolbar
- **Keyboard navigation**: arrows, enter, F (focus on selected in graph)

---

## File Summary

| Phase | New Files | Modified Files |
|-------|-----------|----------------|
| 1: Instrumentation | 1 | 4 (CkProcessorScheduler.h/.cpp, CkProcessorGraph — no change needed, CkProcessor.h, CkEcsWorld_Subsystem.h) |
| 2: Skeleton | 4 | 1 (.uplugin) |
| 3: Data Layer | 5 | 0 |
| 4: Tree View | 8 | 0 |
| 5: Graph View | 10 | 0 |
| 6: Timeline | 4 | 0 |
| 7: Combined | 2 | 0 |
| 8: Polish | 2 | existing |
| **Total** | **~36** | **~5** |

---

## Verification

After each phase:
1. **Phase 1**: Compile CkFoundation. Verify `Get_Scheduler()` and `Get_Partition()` return valid data in a test PIE session via log output.
2. **Phase 2**: Compile CkSchedulerDebugger. Verify `ck.SchedulerDebugger` console command opens an empty tab.
3. **Phase 3**: Verify DataCollector populates processor list and timing data in `Tick()` (log output or debugger breakpoint).
4. **Phase 4**: Open debugger, verify tree populates with correct groups/processors, selection highlights, inspector shows details.
5. **Phase 5**: Switch to Full Graph sub-tab, verify DAG renders with correct edges, pan/zoom works, click-to-select syncs.
6. **Phase 6**: Switch to Timeline tab, verify swim lanes render with proportional bars, pump sections visible, click-to-select works.
7. **Phase 7**: Switch to Combined tab, verify both views visible, splitter works, selection syncs between them.
8. **Phase 8**: Verify sparklines animate, heat map toggles, freeze captures state.

## Implementation Order

Phases 1→2→3 are strictly sequential (each depends on prior).
After Phase 3, Phases 4/5/6 can partially overlap (they share the data layer but build independent UI).
Phase 7 depends on 4+6.
Phase 8 is incremental polish on top of everything.
