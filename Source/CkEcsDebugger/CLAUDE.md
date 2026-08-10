# CkEcsDebugger — Development Guidelines

> **Read [`../CkDebuggerCommon/CLAUDE.md`](../CkDebuggerCommon/CLAUDE.md) first.**
> It covers cross-debugger conventions: shared widgets, copy / selectable text
> policy, the search-mode toggle, PIE world handling, and the safety rules
> (lambda capture, TSharedPtr null-checks, deprecated Slate APIs, brush
> allocation, etc.). This file only covers what's specific to the ECS debugger.

## Module Overview

Slate-based ECS debugger for the CkFoundation plugin. Displays entity trees, component inspectors, and a graph view of entity relationships. Inspired by Flecs Explorer. Runs as a standalone editor tab that persists across PIE sessions.

## Architecture

```
Window (SCkDebuggerWindow_Main)
├── EntityList Panel (left sidebar)
│   ├── World Selector (buttons per PIE world)
│   ├── Search Bar
│   ├── Entity Tree (hierarchical by lifetime owner)
│   └── Status Bar
├── Content Area (center)
│   └── Pages (Overview with graph view, future pages)
└── Inspector Panel (right sidebar)
    └── Component Inspectors (auto-registered, sorted by priority)
```

### Key Models (shared state)
- `FCkDebuggerModel_EntitySelection` — selected entities + history for back/forward navigation
- `FCkDebuggerModel_WorldContext` — selected world, entity cache, world change broadcast

World/session changes are atomic ownership boundaries. The main window routes
both the common debugger-session invalidation signal and
`WorldContext::OnWorldChanged` through `Reset_ForWorldChange`: current
selection, Back/Forward history, picker state, entity-cache handles, tree nodes,
pins, and inspector state are released before the new world can be
refreshed. Never use `Clear_Selection()` for this boundary because it records
the old-world selection in navigation history.

### Cross-debugger entity navigation hook

`FCkEcsDebuggerModule::StartupModule` registers an
`ck::DebugNav::Register_EntityNavigator` callback that opens this debugger's
tab and sets its selection model when any other debugger's
`SCkDebug_EntityRef` widget is clicked. `ShutdownModule` clears the slot
first so a stale click can't re-enter a torn-down module. The navigator
itself lives in `CkDebuggerCommon/Navigation/CkDebug_Navigator.h`. See the
"Entity references" section in `../CkDebuggerCommon/CLAUDE.md` for the full
pattern.

The ECS module also registers an on-demand primary-selection provider for the common window
chrome. Explicit navigator selections broadcast once with source `EcsDebugger`, so already-open
feature debuggers adopt overlay quick-selects without an echo loop. The single-entity inspector
uses `SCkDebug_EntityDebuggerLinks`; only feature routes whose lineage-aware predicate resolves
the selected entity are shown.

The toolbar's On-Screen Overlay popover owns the shared session controls for continuous focused-
entity sync and full-depth focus expansion. Continuous sync is sourced only by
`CkEntityDebugOverlay`; ECS viewport-picking retains its explicit click-to-select behavior. Both
the on-screen overlay and viewport picker pass call-scoped focus roots to the shared marker gather,
which never retains those handles.

### Live hierarchy refresh

`SCkDebuggerWidget_EntityTree` polls the world model's structural revision. A revision can advance
without an added/removed membership diff, especially when `FFragment_LifetimeOwner` is replaced.
After applying membership changes, refresh therefore clears only node parent/children/root links
and relinks every surviving node. Do not replace the `TSharedPtr` nodes or fall back to periodic
`ForceFullRefresh`; selection, expansion, and cached labels depend on stable node identity.

### Inspector System
- `ICkDebuggerComponentInspector_Base` — interface with lifecycle: `CanInspect`, `Build_Inspector`, `Tick`, `OnDeactivated`
- `FCkDebuggerInspectorRegistry` — auto-registration via `CK_REGISTER_DEBUGGER_INSPECTOR` macro
- `FCkInspectorWidgetBuilder` — fluent API for building label-value grids with filtering

### Graph System
- `FCkEcsGraphModel` — pure data: nodes + edges from entity relationships
- `ICkEcsGraphLayoutStrategy` — layout algorithm (currently `FCkDirectionalGraphLayout`)
- `SCkDebuggerWidget_GraphView` — SCanvas + OnPaint rendering with pan/zoom/drag

## ECS-Specific Safety Rule

### Inspector lifecycle: use `OnDeactivated` for cleanup

When an inspector allocates per-entity state (debug draw, registered delegates, etc.), clean it up in `OnDeactivated()` — NOT in the destructor alone. `OnDeactivated` is called when:
- The inspected entity changes
- The inspector panel rebuilds
- The panel is destroyed

## ECS-Specific Conventions

- Use `SCkDebug_IconToggle` only for fixed-icon booleans and place debugger-wide actions in
  `SCkDebug_WindowChrome::MenuActionsContent`. Use `SCkDebug_ToggleSurface` for contextual,
  content-bearing filters, cards, and dynamic feature chips; use `SSegmentedControl` for
  mutually exclusive choices. Do not promote contextual entity filters into the window chrome.
- Inspector priority determines sort order (lower = higher in panel): EntityInfo=10, Transform=20, TagSet=25, Network=30, Relationships=40, etc.
- Inspectors that need per-inspector search set `IsFilterable() -> true`
- `FCkDebuggerStyle` (Slate brushes, text styles, padding + graph-node size constants, SVG icon registry) **moved to `CkDebuggerCommon/Styles/CkDebuggerStyle.h`** in the 2026-08-09 common-widget consolidation — it is now the whole suite's style set. Include the common path; do NOT call `Initialize`/`Shutdown` from this module (CkDebuggerCommon owns its lifetime). Cross-debugger colour tokens still live in `CkStyle::`.
- `SCkDebuggerWidget_SearchBar` is now a compatibility alias for the promoted `SCkDebug_SearchBar` (`CkDebuggerCommon/Search/`). New code uses the common spelling; the alias header is deleted in a later unit of that campaign.
- `FCkInspectorWidgetBuilder` composes rows out of `SCkDebug_KeyValueRow`. `AddHeader` emits `SCkDebug_SectionHeader`.
- Graph model is pure data with no rendering. Layout strategy is swappable.

## Adding a New Inspector

1. Create `CkInspector_Foo.h` / `.cpp` in `Inspectors/`
2. Inherit from `ICkDebuggerComponentInspector_Base`
3. Add `CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Foo)` in the .cpp
4. Implement `Get_ComponentName`, `CanInspect`, `Build_Inspector`, `Get_SortPriority`
5. If filterable: override `IsFilterable() -> true` and implement `Build_Inspector(Entity, Filter)`
6. If cleanup needed: override `OnDeactivated()`
7. Use `SCkDebug_KeyValueRow` (via `FCkInspectorWidgetBuilder::AddRow`) for value rows — values are automatically copyable. For custom text, use `SCkDebug_SelectableLabel` (see `CkDebuggerCommon/CLAUDE.md`).
8. Add module dependency to `CkEcsDebugger.Build.cs` if needed

## Adding a Graph Relationship

1. Add enum value to `ECkGraphEdgeType`
2. Add label in `GetEdgeTypeLabel()`
3. Add `Gather_Foo()` method to `FCkEcsGraphModel`
4. Call it from `Rebuild()`
5. Add direction mapping in `FCkDirectionalGraphLayout::ComputeLayout()`
