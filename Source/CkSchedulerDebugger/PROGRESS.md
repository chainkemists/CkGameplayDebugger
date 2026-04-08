# CkSchedulerDebugger — Implementation Progress

## Status: Core Structure Complete, Wiring In Progress

## Completed

### Phase 1: Scheduler Instrumentation (CkFoundation)
- [x] `CkEcsWorld_Subsystem.h`: Added `CK_PROPERTY_GET(_Scheduler)` on `ACk_EcsWorld_Actor_UE`, `CK_PROPERTY_GET(_WorldActors)` on subsystem
- [x] `CkProcessorScheduler.h`: Added `CK_PROPERTY_GET(_Partition)`, `_DebugFrameHistory`, `_LastGraphBuildTimeMs` getter
- [x] `CkProcessorScheduler.cpp`: Instrumented `Tick()` and `DoPump()` with `FPlatformTime::Seconds()` per-processor timing, guarded by `#if !UE_BUILD_SHIPPING`
- [x] `CkProcessor.h`: Added `CK_PROPERTY_GET(_TotalTicks)` on `TProcessorBase`
- [x] `CkSchedulerDebugData.h`: New file with `FSchedulerDebug_ProcessorTiming` and `FSchedulerDebug_FrameSnapshot`
- [ ] `FScopeCycleCounter` integration for Unreal Insights (planned but not yet added alongside FPlatformTime)

### Phase 2: Module Skeleton
- [x] `CkSchedulerDebugger.Build.cs`: Dependencies configured
- [x] `CkSchedulerDebugger_Module.h/.cpp`: Module class with tab spawner, console command `ck.SchedulerDebugger`
- [x] `CkSchedulerDebuggerStyle.h`: Inline namespace with group colors, timing heat, layout constants
- [x] `CkDebugger.uplugin`: Module entry added

### Phase 3: Data Layer
- [x] `CkSchedulerDebugger_Types.h`: ProcessorInfo, GroupInfo, TreeNode structs
- [x] `CkSchedulerDebugger_DataCollector.h/.cpp`: Reads scheduler data from world subsystem each frame
- [x] `CkSchedulerDebugger_ViewModel.h/.cpp`: Selection, freeze, delegates

### Phase 4: Tree View Tab
- [x] `ICkSchedulerDebuggerPage.h`: Page interface
- [x] `SCkSchedulerDebuggerWindow.h/.cpp`: Main window with top bar, stats bar, tab bar, content area
- [x] `SCkSchedulerDebugger_ProcessorTree.h/.cpp`: Tree widget with search, sort, STreeView, badges
- [x] `SCkSchedulerDebugger_Inspector.h/.cpp`: Detail panel with sections, clickable dependencies
- [x] `CkSchedulerDebuggerPage_TreeView.h/.cpp`: 3-pane layout with splitters

### Phase 5: Graph View
- [x] `CkSchedulerDebugNode_Processor.h/.cpp`: UEdGraphNode subclass
- [x] `CkSchedulerDebugGraphSchema.h/.cpp`: Read-only schema
- [x] `CkSchedulerDebugGraph.h/.cpp`: UEdGraph with rebuild/update/layout, topology hash
- [x] `SGraphNode_SchedulerProcessor.h/.cpp`: Custom node rendering with accent strip, badges
- [x] `CkSchedulerDebugConnectionPolicy.h/.cpp`: Straight-line edge drawing
- [x] `CkSchedulerDebugGraphFactory.h/.cpp`: Node factory

### Phase 6: Timeline Tab
- [x] `SCkSchedulerDebugger_Timeline.h/.cpp`: SLeafWidget with OnPaint, zoom, pan, hit testing
- [x] `CkSchedulerDebuggerPage_Timeline.h/.cpp`: Page with pump breakdown panel

### Phase 7: Combined Tab
- [x] `CkSchedulerDebuggerPage_Combined.h/.cpp`: Split layout with toggle buttons

## In Progress

### Wiring & Integration Issues
- [x] Fixed: `PC_Exec` → `TEXT("Transition")` for pin categories
- [x] Fixed: Forward declaration `class` vs `struct` for `ck::FProcessorScheduler`
- [x] Fixed: Field name mismatches (ProcessorNodeIndex→NodeIndex, LastFrameTimeMs→MainPassTimeMs, ProcessorIndices→MemberIndices)
- [x] Fixed: `GT_Simple` → `GT_StateMachine`
- [x] Fixed: `INDEX_NONE` auto deduction with `static_cast<int32>`
- [x] Fixed: Replaced `FPlaceholderPage` for Timeline and Combined with real page classes
- [x] Wired `SGraphEditor` into TreeView's "Full Graph" sub-tab
- [x] Switched ProcessorTree from `SMultiColumnTableRow` to `STableRow` with `.Content()` for proper tree indentation + selection
- [x] Removed `HeaderRow` that was breaking tree layout
- [x] Wired real Timeline and Combined pages (removed FPlaceholderPage)
- [ ] **CURRENT FOCUS**: Perfect the Tree View + Detail Graph sub-tab
- [ ] Wire detail graph (mini neighbor view) into TreeView's "Detail Graph" sub-tab
- [ ] Verify all Tick() refresh paths actually trigger widget rebuilds

## Not Started

### Phase 8: Polish
- [ ] Sparkline widget (`SCkSchedulerDebugger_Sparkline.h/.cpp`)
- [ ] Heat map toggle on graph nodes
- [ ] Freeze pulsing "FROZEN" visual indicator
- [ ] Sort options (Name / Exec Order / Timing) in tree toolbar
- [ ] Keyboard navigation (arrows, enter, F for focus)
- [ ] `FScopeCycleCounter` alongside `FPlatformTime` for Insights

### Known Issues to Investigate
- DataCollector.cpp line 203: May need validation for edge cases when no PIE world is active
- Timeline OnPaint: Verify that bars render correctly with real data (only tested with mock expectations)
- Graph layout: Basic single-row-per-group; no Sugiyama crossing minimization yet
- The `UPROPERTY()` on `_DebugGraph` in the TreeView page header won't compile — it's not a UCLASS; need to use AddToRoot/RemoveFromRoot pattern instead

## File Inventory (36 source files in module + 5 modified in CkFoundation)

```
CkSchedulerDebugger/
├── CkSchedulerDebugger.Build.cs
├── CkSchedulerDebugger_Module.h
├── CkSchedulerDebugger_Module.cpp
├── PLAN.md
├── PROGRESS.md
└── Public/CkSchedulerDebugger/
    ├── Data/
    │   ├── CkSchedulerDebugger_Types.h
    │   ├── CkSchedulerDebugger_DataCollector.h
    │   └── CkSchedulerDebugger_DataCollector.cpp
    ├── Graph/
    │   ├── CkSchedulerDebugNode_Processor.h/.cpp
    │   ├── CkSchedulerDebugGraphSchema.h/.cpp
    │   ├── CkSchedulerDebugGraph.h/.cpp
    │   ├── SGraphNode_SchedulerProcessor.h/.cpp
    │   ├── CkSchedulerDebugConnectionPolicy.h/.cpp
    │   └── CkSchedulerDebugGraphFactory.h/.cpp
    ├── Pages/
    │   ├── ICkSchedulerDebuggerPage.h
    │   ├── CkSchedulerDebuggerPage_TreeView.h/.cpp
    │   ├── CkSchedulerDebuggerPage_Timeline.h/.cpp
    │   └── CkSchedulerDebuggerPage_Combined.h/.cpp
    ├── Styles/
    │   └── CkSchedulerDebuggerStyle.h
    ├── ViewModel/
    │   ├── CkSchedulerDebugger_ViewModel.h
    │   └── CkSchedulerDebugger_ViewModel.cpp
    ├── Widgets/
    │   ├── SCkSchedulerDebugger_ProcessorTree.h/.cpp
    │   ├── SCkSchedulerDebugger_Inspector.h/.cpp
    │   └── SCkSchedulerDebugger_Timeline.h/.cpp
    └── Window/
        ├── SCkSchedulerDebuggerWindow.h
        └── SCkSchedulerDebuggerWindow.cpp
```

## Reference Mockup
Interactive mockup at: `CkSchedulerDebugger/Mockups/mockup_unified.html`
(Serve with `npx serve` and open index.html)

## Key Patterns Used
- Module: follows `CkSmDebugger_Module` pattern exactly
- Widgets: follows `CkEcsDebugger` STreeView/SSplitter/SScrollBox patterns
- Graph: follows `CkSmDebugger` UEdGraph/SGraphNode/Schema/Factory patterns
- Data: follows `CkSmDebugger` DataCollector/ViewModel MVVM-light pattern
- Style: inline namespace pattern from `FCkSmDebuggerStyle`
