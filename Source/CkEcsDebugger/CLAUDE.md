# CkEcsDebugger - Development Guidelines

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

### Inspector System
- `ICkDebuggerComponentInspector_Base` — interface with lifecycle: `CanInspect`, `Build_Inspector`, `Tick`, `OnDeactivated`
- `FCkDebuggerInspectorRegistry` — auto-registration via `CK_REGISTER_DEBUGGER_INSPECTOR` macro
- `FCkInspectorWidgetBuilder` — fluent API for building label-value grids with filtering

### Graph System
- `FCkEcsGraphModel` — pure data: nodes + edges from entity relationships
- `ICkEcsGraphLayoutStrategy` — layout algorithm (currently `FCkDirectionalGraphLayout`)
- `SCkDebuggerWidget_GraphView` — SCanvas + OnPaint rendering with pan/zoom/drag

## Critical Safety Rules

### 1. NEVER capture raw UObject* in delegates or lambdas

**Wrong:**
```cpp
auto* World = GetWorld();
Button->OnClicked(this, &MyClass::OnClicked, World); // World can be GC'd
```

**Correct:**
```cpp
TWeakObjectPtr<UWorld> WorldWeak(GetWorld());
Button->OnClicked(this, &MyClass::OnClicked, WorldWeak);
// In handler: auto* World = InWorldWeak.Get(); if (!World) return;
```

This applies to ALL UObject-derived types: UWorld, AActor, UActorComponent, UGameInstance, etc. Slate widgets outlive PIE sessions. Worlds and actors are garbage collected when PIE ends. A raw pointer captured in a button delegate becomes dangling.

### 2. ALWAYS null-check TSharedPtr widgets before dereferencing

**Wrong:**
```cpp
NodeCanvas->ClearChildren(); // Crashes if called before Construct()
```

**Correct:**
```cpp
if (NodeCanvas.IsValid())
{
    NodeCanvas->ClearChildren();
}
```

Every `TSharedPtr<SWidget>` member must be checked before use. Methods like `ClearGraph()`, `RebuildFromModel()`, `RebuildInspectors()` can be called during construction, teardown, or in response to external events before widgets are ready.

### 3. ALWAYS validate array indices before access

**Wrong:**
```cpp
Pages[ActivePageIndex]->Set_IsActive(true);
```

**Correct:**
```cpp
if (Pages.IsValidIndex(ActivePageIndex) && Pages[ActivePageIndex].IsValid())
{
    Pages[ActivePageIndex]->Set_IsActive(true);
}
```

### 4. Inspector lifecycle: use OnDeactivated for cleanup

When an inspector allocates per-entity state (debug draw, registered delegates, etc.), clean it up in `OnDeactivated()` — NOT in the destructor alone. `OnDeactivated` is called when:
- The inspected entity changes
- The inspector panel rebuilds
- The panel is destroyed

### 5. FCk_Handle validity

ALWAYS check `ck::IsValid(Handle)` before passing handles to CkFoundation API functions. Handles become invalid when:
- PIE ends (all entities destroyed)
- The entity is killed at runtime
- The world switches

### 6. World validity

Before calling `UWorld::GetSubsystem()` or any world API, verify:
```cpp
if (ck::Is_NOT_Valid(World)) { return; }
if (NOT World->HasBegunPlay()) { return; } // Subsystems not initialized yet
```

Worlds appear in `GEngine->GetWorldContexts()` before `HasBegunPlay()` is true. Calling `GetSubsystem()` on such a world crashes.

### 7. No deprecated Slate APIs

Use explicit `ToPaintGeometry(FVector2f Size, FSlateLayoutTransform)` instead of the parameterless `ToPaintGeometry()` which is deprecated in UE 5.5+.

### 8. Brush allocation

NEVER allocate brushes (`new FSlateColorBrush(...)`) in hot paths (Tick, OnPaint, Build methods called per-frame). Register brushes in `FCkDebuggerStyle` and reference them via the style set. Bare `new` brushes leak.

## Coding Conventions

Follow CkFoundation conventions (see CkFoundation/CLAUDE.md):

- **Trailing return types:** `auto Foo() -> ReturnType`
- **Validity checks:** `ck::IsValid()` / `ck::Is_NOT_Valid()` — never raw null checks
- **Boolean negation:** `NOT` macro instead of `!`
- **String formatting:** `ck::Format_UE(TEXT("{}"), Value)` — NEVER `FString::Printf` or `%s` specifiers. Uses `{}` libfmt-style placeholders.
- **`auto` everywhere:** Prefer `auto` for local variables
- **`MoveTemp`:** Use UE's `MoveTemp` instead of `std::move`
- **Member variable prefix:** `_` prefix (e.g., `_Config`, `_ProbeHandleToToggle`)
- **Section separators:** `// ====...` between major sections in .cpp files
- **Include order:** Standard → Unreal Engine → CkCore/CkEcs → Module-specific

### Debugger-Specific Conventions

- Inspector priority determines sort order (lower = higher in panel): EntityInfo=10, Transform=20, TagSet=25, Network=30, Relationships=40, etc.
- Inspectors that need per-inspector search set `IsFilterable() -> true`
- Colors live in `CkDebugStyle::` (CkDebuggerCommon) — tunable under Project Settings → CkGameplayDebugger → GOAP. `FCkDebuggerStyle` only owns ECS-specific Slate brushes, text styles, padding + graph-node size constants.
- Common Slate primitives live in `CkDebuggerCommon/Widgets/` — `SCkDebug_InspectorPanel` for collapsible sections, `SCkDebug_KeyValueRow` for label/value rows, `SCkDebug_SectionHeader` for subsection headers, `SCkDebug_StatusPill` for toned status labels. Prefer these over bespoke `SExpandableArea` / `STextBlock` constructions.
- `FCkInspectorWidgetBuilder` composes rows out of `SCkDebug_KeyValueRow`. `AddHeader` emits `SCkDebug_SectionHeader`.
- Graph model is pure data with no rendering. Layout strategy is swappable.

## Adding a New Inspector

1. Create `CkInspector_Foo.h` / `.cpp` in `Inspectors/`
2. Inherit from `ICkDebuggerComponentInspector_Base`
3. Add `CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Foo)` in the .cpp
4. Implement `Get_ComponentName`, `CanInspect`, `Build_Inspector`, `Get_SortPriority`
5. If filterable: override `IsFilterable() -> true` and implement `Build_Inspector(Entity, Filter)`
6. If cleanup needed: override `OnDeactivated()`
7. Add module dependency to `CkEcsDebugger.Build.cs` if needed

## Adding a Graph Relationship

1. Add enum value to `ECkGraphEdgeType`
2. Add label in `GetEdgeTypeLabel()`
3. Add `Gather_Foo()` method to `FCkEcsGraphModel`
4. Call it from `Rebuild()`
5. Add direction mapping in `FCkDirectionalGraphLayout::ComputeLayout()`
