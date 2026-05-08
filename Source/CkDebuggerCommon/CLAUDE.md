# CkDebuggerCommon — Authoring a CK Debugger

> **Read this file first** when creating a new debugger module under
> `Plugins/CkGameplayDebugger/Source/`, or when adding any user-facing surface
> (panels, graph nodes, list rows, search inputs) to an existing one. It is the
> canonical home for cross-debugger conventions, shared widgets, and safety
> rules. Per-module `CLAUDE.md` files only cover that module's own architecture.

## What lives here

`CkDebuggerCommon` is a shared library every debugger module depends on. It
provides:

- **Style tokens** (`CkDebugStyle::`) — colours, spacing, font sizes. Tunable
  under Project Settings → CkGameplayDebugger.
- **Shared Slate widgets** (`Widgets/`) — composable building blocks for rows,
  pills, headers, status indicators, copy / selectable text.
- **Dual search bar** (`Search/SCkDebug_DualSearchBar`) — side-by-side Filter
  + Highlight inputs. Used by every debugger that has a list/tree the user
  searches.
- **Copy-menu helpers** (`Utils/CkDebug_CopyMenu_Utils.h`) — one canonical
  "Copy …" menu shape across schemas, lists, and right-click handlers.
- **Entity reference widget** (`Widgets/SCkDebug_EntityRef`) — single-line
  clickable pill for any `FCk_Handle` display surface. Click opens the entity
  in the CK ECS Debugger; right-click → Copy. See "Entity references" below.
- **Cross-debugger navigator** (`Navigation/CkDebug_Navigator.h`) — the
  one-way registration hook the EntityRef widget uses to invoke the ECS
  Debugger without taking a hard dependency on it.
- **Window-level plumbing** (`Window/CkDebuggerRefreshGate.h`) — refresh-rate
  gate honouring user settings (Use Global / OnlyWhenVisible / Hz cap).

If you find yourself reaching for `STextBlock`, `FMenuBuilder`, or
`SEditableTextBox` directly, check this file first — the right primitive
probably already exists.

## Copy & selectable text — the policy

**Default:** every piece of user-visible text that carries information (names,
IDs, paths, expressions, numbers, timestamps) should be copyable. Decorative
labels and short banners may stay as `STextBlock`. When in doubt, make it
selectable.

| Use case | Widget | Notes |
|---|---|---|
| A label or value previously written as `STextBlock` | `SCkDebug_SelectableLabel` | Drop-in. Has `.Text/.Font/.ColorAndOpacity` and an imperative `SetText()`. **Single-line only**, no `AutoWrapText`, no `TransformPolicy::ToUpper`. For uppercase headers, use `STextBlock` or apply `FString::ToUpper()` to the input. |
| Composite widget that should support right-click → Copy as a unit (group, pill, custom row) | Wrap with `SCkDebug_CopyableContainer` | Pass `.CopyText(...)` with the multi-line clipboard payload. SButton inside the wrapped child still receives left-clicks; right-click bubbles through. |
| Inspector key/value rows | `SCkDebug_KeyValueRow` (via `FCkInspectorWidgetBuilder::AddRow`) | Values are already `SEditableText` — automatic. |
| Tree / list row composite (history, plan history, transition log) | `SCkDebug_HistoryRow` with `.CopyText(...)` | Preferred over hand-rolled rows because it shares tone, accent, selection styling. |
| Graph-style step / node pill | `SCkDebug_NodePill` with `.CopyText(...)` | Same opt-in pattern. |

Compose multi-field clipboard payloads with newlines so the user can paste a
"give me everything about this thing" summary into a bug report. See
`SCkGoapDebug_HistoryRail.cpp` for the canonical example.

## Entity references — always use `SCkDebug_EntityRef`

Any time a debugger renders an `FCk_Handle` to the user, use
`SCkDebug_EntityRef` instead of formatting it into an `STextBlock`. Click on
the pill opens the entity in the CK ECS Debugger (tab is invoked / focused,
selection model gets the handle); right-click → Copy works automatically.

```cpp
SNew(SCkDebug_EntityRef)
    .Entity(SomeHandle)              // FCk_Handle, plain or attribute
    .ShowName(true)                  // optional: prefixes with DebugName
```

Use `Entity_Lambda([this](){ ... return Handle; })` when the displayed entity
changes over time (e.g. the selection in your debugger's view-model).

### `ShowName` — when to set true vs false

- **`ShowName(false)` (default)** — when an adjacent widget (like a combo box
  or section header) already shows the DebugName. The pill renders just the
  canonical `ID|Version(Raw)`, avoiding visual duplication. This is the
  pattern used in the SM / GOAP / AStar toolbars.
- **`ShowName(true)`** — when the pill is the *only* identifier of the entity
  on that surface (inspector rows like "Context Owner:" / "Parent:" / variable
  rows; badge boxes; the Crowd AgentDetail panel header).

### How click-to-navigate is wired up

`SCkDebug_EntityRef` calls `ck::DebugNav::Goto_Entity(Handle)`. The ECS
Debugger module registers a navigator function in its `StartupModule` that
opens the tab and sets the selection model. If `CkEcsDebugger` isn't loaded
(e.g. cooked client), the click is a no-op and the pill still renders +
right-click → Copy still works.

### Where the widget already lives

| Site | `ShowName` | Notes |
|---|---|---|
| ECS Inspector — `Entity Info → ID:` row | false | Sibling `Name:` row already shows the name. |
| ECS Inspector — `Relationships`, `SceneNode`, `DynamicFragments`, `Variables` | true | Pill is the only entity identifier in the row. |
| ECS Inspector — `MakeBadgeBox` / `PopulateBadgeBox` (used by Probes, ProbeTraces, InteractionResolver, InteractTarget, SceneNode siblings) | true | Each handle in the wrap-box is a pill. |
| SM / GOAP / AStar toolbars | false | Adjacent combo already shows the name. |
| Crowd `AgentDetailPanel` header | true | No combo on this panel; pill is the only identifier. |

When adding a new debugger that displays entities, follow the same matrix.

## Right-click "Copy …" menus

Three call shapes, all in `ck::DebugCopyMenu::` (`Utils/CkDebug_CopyMenu_Utils.h`):

```cpp
// 1) Right-click on any Slate widget (e.g. a selector button wrapper):
.OnMouseButtonDown_Lambda([this, Text](const FGeometry&, const FPointerEvent& Evt)
{
    return ck::DebugCopyMenu::Handle_RightClickToCopy(SharedThis(this), Evt, Text);
})

// 2) Inside SListView/STreeView::OnContextMenuOpening:
ck::DebugCopyMenu::AddCopyEntry(MenuBuilder, Label, Tooltip, TextToCopy);

// 3) Inside UEdGraphSchema::GetContextMenuActions:
ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu, EntryName, Label, Tooltip, TextToCopy);
```

For graph schemas: cast `InContext->Node` to your concrete node type and add
**multiple** entries — the visible display name, the underlying class / id, key
fields (cost, timing, conditions, …), and a "Copy All" multi-line summary.
`CkGoapDebugGraphSchema.cpp` and `CkSchedulerDebugGraphSchema.cpp` are the
canonical examples.

For tree / list `OnContextMenuOpening`: prefer multi-select aware behaviour —
join names / ids with `\n` so users can copy several rows at once. See
`CkDebuggerWidget_EntityTree.cpp::OnContextMenuOpening`.

For `SEditableText`-based widgets (`SelectableLabel`, `KeyValueRow` value): no
extra wiring needed — they ship with a built-in right-click context menu and
Ctrl+C / drag-select for free.

## Search bars

Every debugger search input should be a `SCkDebug_DualSearchBar`, giving the
user two independent text queries:

- **Filter** — narrows the visible entries (hide non-matches).
- **Highlight** — among entries that survive the filter, dim non-matches so
  matches stand out without losing surrounding context.

The two compose: type into Filter to narrow the list, then type into Highlight
to spot specific items inside the narrowed set. Either can be empty.

```cpp
SNew(SCkDebug_DualSearchBar)
    .FilterHintText(FText::FromString(TEXT("Filter processors…")))
    .HighlightHintText(FText::FromString(TEXT("Highlight…")))
    .OnFilterTextChanged_Lambda([this](const FString& InText)
    {
        if (_FilterString == InText) { return; }
        _FilterString = InText;
        ApplyFilterPipeline();   // your filter pass — affects visibility
    })
    .OnHighlightTextChanged_Lambda([this](const FString& InText)
    {
        if (_HighlightString == InText) { return; }
        _HighlightString = InText;
        ApplyFilterPipeline();   // your highlight pass — affects dim colour
    })
```

State lives per-widget (resets on debugger close). Two patterns for the
pipeline:

- **Tree/list with persistent rows** (ECS entity tree, scheduler processor
  tree): keep both `IsVisible` and `IsSearchMatch` flags on the node struct.
  Pass 1 sets `IsVisible` from `_FilterString`. Pass 2 sets `IsSearchMatch`
  from `_HighlightString` (true if `_HighlightString.IsEmpty()`). Row text
  colour binds to a lambda that returns the muted colour when `NOT IsSearchMatch`.
- **Rebuild-on-change panels** (GOAP world state, scheduler frame breakdown):
  inside the row-build loop, `continue` past rows that don't match the Filter
  input, then pick the muted colour for rows that don't match the Highlight
  input. The row name's `ColorAndOpacity_Lambda` can read `_HighlightString`
  directly if rebuild-per-keystroke is too expensive.

See `CkDebuggerWidget_EntityTree.cpp::ApplyFilterToNodes` for the persistent
pattern, `SCkGoapDebugger_WorldStatePanel.cpp::RebuildWorldState` for the
rebuild pattern.

## PIE lifecycle — the world identity gotcha

Worlds appear in `GEngine->GetWorldContexts()` **before** `HasBegunPlay()` is
true; calling `UWorld::GetSubsystem()` on a not-yet-begun world crashes. Always
guard:

```cpp
if (ck::Is_NOT_Valid(World)) { return; }
if (NOT World->HasBegunPlay()) { return; }
```

When polling `GEngine->GetWorldContexts()` to react to PIE start/stop, **never
gate on count alone**. A quick stop→restart can produce the same count with a
different `UWorld*`, and a `TWeakObjectPtr<UWorld>` cached from the previous
session silently goes invalid. Track the worlds list by **identity**:

```cpp
// Detect changes by IDENTITY, not just count.
auto WorldsChanged = AvailableWorlds.Num() != LastKnownWorlds.Num();
if (NOT WorldsChanged)
{
    for (auto Index = 0; Index < AvailableWorlds.Num(); ++Index)
    {
        if (LastKnownWorlds[Index].Get() != AvailableWorlds[Index])
        { WorldsChanged = true; break; }
    }
}
```

See `CkDebuggerPanel_EntityList.cpp::Tick` for the canonical implementation.

## In-world overlays via PMG

Debugger overlays that attach geometry to a selected runtime entity should
use `CkPmg`'s typed-shape API (filled procmesh + auto-wireframe) for any
volume-shaped marker — capsule, sphere, box. Reach for the line-only
`ck::pmg::Append_Debug*_World` API only for inherently line-shaped content
(paths, polylines, drop indicators).

| Use case | API |
|---|---|
| Filled capsule/sphere/box on the selected entity (live-tracking) | `UCk_Utils_Pmg_BasicShapes::Add_Capsule` / `Add_Sphere` / `Add_Box` etc. |
| Filled marker shapes that should cascade-destroy with a parent overlay entity | `UCk_Utils_Pmg_BasicShapes::Create_*` with the parent overlay entity as owner |
| Path lines, drop indicators, custom polylines | `ck::pmg::Append_DebugLine_World` (wireframe by design) |

Two gotchas the nav-debugger session burned — both fully covered in
`CkPmg/CLAUDE.md`, summarised here:

- **`InDuration = 0.0f` is a one-tick destroy**, not "persistent". The
  `CheckDuration` processor early-outs only on negative values. For live
  overlays use `-1.0f`.
- **`Append_Debug*_World` produces wireframes, not filled meshes.** If
  the overlay was supposed to look like a real volume, use the typed
  `Add_*` / `Create_*` calls — those build the procmesh and emit the
  wireframe automatically when `InDrawLines=true`.

Live-tracking pattern:

```cpp
// Once on selection:
GState.Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity_TransientOwner(World);
UCk_Utils_Pmg_BasicShapes::Add_Capsule(
    GState.Entity, FTransform{Center}, Radius, HalfHeight, Segments, Rings,
    Axis, Color, /*InDrawLines=*/true, Thickness, /*InDuration=*/-1.0f);
GState.Transform = UCk_Utils_Transform_UE::Cast(GState.Entity);

// Each tick:
UCk_Utils_Transform_UE::Request_SetTransform(
    GState.Transform, FCk_Request_Transform_SetTransform{FTransform{NewLocation}});
```

When the debugger window closes, selection changes, or the world resets,
call `UCk_Utils_EntityLifetime_UE::Request_DestroyEntity` on the parent
overlay entity — child shapes spawned via `Create_*(ParentEntity, ...)`
cascade-destroy with it. Don't poke ECS internals (`InHandle.AddOrGet<>`,
`InHandle.Add<>`) from debugger client code; that belongs inside the
PMG / Transform Utils. If a Util's behaviour seems wrong, fix the Util.

Render gating reminder: keep `DrawAll` cheap when nothing's selected and
the window is closed. The nav debugger gates on
`(WindowOpen || OverlayAlwaysOn cvar) && SelectedId >= 0`, then matches
`SelectedId` against `int32(GetTypeHash(Agent.EntityHandle))`. The same
hash lookup must be done identically on both write (selection) and read
(overlay) paths or the overlay silently no-ops on a live agent.

## Critical safety rules

These apply across **every** debugger module.

### 1. NEVER capture raw `UObject*` in delegates or lambdas

```cpp
// Wrong — World can be GC'd between bind-time and call-time
auto* World = GetWorld();
Button->OnClicked(this, &MyClass::OnClicked, World);

// Correct — store a weak ptr
TWeakObjectPtr<UWorld> WorldWeak(GetWorld());
Button->OnClicked(this, &MyClass::OnClicked, WorldWeak);
// In handler: auto* World = WorldWeak.Get(); if (NOT World) { return FReply::Handled(); }
```

Applies to all UObject-derived types — UWorld, AActor, UActorComponent,
UGameInstance, etc. Slate widgets outlive PIE; worlds and actors do not.

### 2. ALWAYS null-check `TSharedPtr` widgets before dereferencing

```cpp
if (NodeCanvas.IsValid()) { NodeCanvas->ClearChildren(); }
```

Methods like `RebuildFromModel()` / `RefreshTree()` may run during construction
or in response to external events before child widgets are ready.

### 3. ALWAYS validate array indices before access

```cpp
if (Pages.IsValidIndex(ActivePageIndex) && Pages[ActivePageIndex].IsValid())
{ Pages[ActivePageIndex]->Set_IsActive(true); }
```

### 4. `FCk_Handle` validity (ECS-using debuggers)

Always check `ck::IsValid(Handle)` before passing to CkFoundation API. Handles
become invalid when PIE ends, the entity is killed, or the world switches.

### 5. No deprecated Slate APIs

Use explicit `ToPaintGeometry(FVector2f Size, FSlateLayoutTransform)` instead
of the parameterless `ToPaintGeometry()` (deprecated in UE 5.5+).

### 6. Brush allocation

NEVER allocate brushes (`new FSlateColorBrush(...)`) in hot paths (Tick,
OnPaint, build methods called per-frame). Register them in your debugger's
style class and reference via the style set. Bare `new` brushes leak.

### 7. Editor-only types in `UncookedOnly` modules

Most debugger modules are `Type: "UncookedOnly"` in the .uplugin, but some
deps (`ToolMenus`, `UnrealEd`) are still gated on `bBuildEditor` in the
`Build.cs`. When using `UToolMenu*` or similar editor-only types in
`CkDebuggerCommon`, wrap the body with `#if WITH_EDITOR` so non-editor uncooked
targets still link. The forward declaration in the header is fine
unconditionally. See `CkDebug_CopyMenu_Utils.cpp::AddCopyEntryToToolMenu`.

## Module conventions

Follow CkFoundation conventions (see `CkFoundation/CLAUDE.md`):

- **Trailing return types:** `auto Foo() -> ReturnType`
- **Validity checks:** `ck::IsValid()` / `ck::Is_NOT_Valid()` — never raw null checks
- **Boolean negation:** `NOT` macro instead of `!`
- **String formatting:** `ck::Format_UE(TEXT("{}"), Value)` — never `FString::Printf` with `%s`. Uses `{}` libfmt-style placeholders.
- **`auto` everywhere:** prefer `auto` for local variables
- **`MoveTemp`:** UE's `MoveTemp` instead of `std::move`
- **Member variable prefix:** `_` (e.g. `_Config`, `_ProbeHandleToToggle`)
- **Section separators:** `// ====...` between major sections in `.cpp` files
- **Include order:** Standard → Unreal Engine → CkCore/CkEcs → Module-specific

### Build.cs

Most debugger modules already depend on `CkDebuggerCommon`, which transitively
brings `Slate`, `SlateCore`, `EditorStyle`, `AppFramework`, `ApplicationCore`
(for `FPlatformApplicationMisc::ClipboardCopy`), and editor-gated `ToolMenus`.
For the schema's `GetContextMenuActions`, you don't need to add `ToolMenus`
yourself — call `ck::DebugCopyMenu::AddCopyEntryToToolMenu` and the helper
handles it.

If your module renders graph nodes via `SGraphEditor`, also add `GraphEditor`.

## Creating a new debugger module — checklist

1. **Set up the module**: `Plugins/CkGameplayDebugger/Source/CkXxxDebugger/`
   with `CkXxxDebugger.Build.cs` (`PublicDependencyModuleNames` includes
   `CkDebuggerCommon`), an `.uplugin` entry with `"Type": "UncookedOnly"`, and
   a `_Module.h/.cpp` that registers any visual node factory / spawns the tab.
2. **Window + view-model**: own a `SCkXxxDebuggerWindow` and a view-model
   that observes runtime data. Honour `FCkDebuggerRefreshGate::Should_RefreshNow`
   in your `Tick` so the user's per-window refresh setting (Use Global / Hz cap
   / OnlyWhenVisible) is respected.
3. **Inspector / detail panels**: build rows from `SCkDebug_KeyValueRow`,
   headers from `SCkDebug_SectionHeader`, status pills from
   `SCkDebug_StatusPill`. Use `SCkDebug_SelectableLabel` for any custom text
   that isn't covered by these primitives.
4. **List / tree rows**: prefer `SCkDebug_HistoryRow` with `.CopyText(...)`.
   For custom row composites, wrap with `SCkDebug_CopyableContainer`. Wire
   `OnContextMenuOpening` and use `ck::DebugCopyMenu::AddCopyEntry` for any
   "copy this row's data" entries (multi-select aware — join with `\n`).
5. **Search input**: use `SCkDebug_DualSearchBar`, store `_FilterString` and
   `_HighlightString` per-widget, and run the two-pass pipeline (see "Search
   bars" above).
6. **Graph (if any)**: subclass `UEdGraphSchema`. Override
   `GetContextMenuActions` and call `ck::DebugCopyMenu::AddCopyEntryToToolMenu`
   for each meaningful field (display name, class name, key data, "Copy All").
   For node display widgets, `SCkDebug_NodePill` with `.CopyText(...)` is the
   default. Register your `FGraphPanelNodeFactory` in module startup.
7. **PIE world handling**: if you select between worlds, track them by
   identity (see "PIE lifecycle" above). Subscribe to nothing — poll on a 1Hz
   tick gate and react to identity changes.

## Quick reference — file paths

```
CkDebuggerCommon/
├── Navigation/
│   └── CkDebug_Navigator.h           (Register/Goto_Entity for cross-debugger nav)
├── Search/
│   └── SCkDebug_DualSearchBar.h      (Filter + Highlight side-by-side)
├── Utils/
│   └── CkDebug_CopyMenu_Utils.h      (Handle_RightClickToCopy, AddCopyEntry, AddCopyEntryToToolMenu)
├── Widgets/
│   ├── SCkDebug_EntityRef.h          (clickable FCk_Handle pill — navigates to ECS Debugger)
│   ├── SCkDebug_SelectableLabel.h    (STextBlock-shape, copyable)
│   ├── SCkDebug_CopyableContainer.h  (wrap any widget with right-click → Copy)
│   ├── SCkDebug_KeyValueRow.h        (inspector row; values selectable)
│   ├── SCkDebug_SectionHeader.h      (uppercase section header)
│   ├── SCkDebug_StatusPill.h         (toned status label)
│   ├── SCkDebug_HistoryRow.h         (history/timeline row + opt-in CopyText)
│   ├── SCkDebug_NodePill.h           (graph / plan-step pill + opt-in CopyText)
│   ├── SCkDebug_InspectorPanel.h     (collapsible section)
│   └── SCkDebug_CountBadge.h
├── Style/
│   └── CkDebugStyle.h                (colour + spacing tokens)
└── Window/
    └── CkDebuggerRefreshGate.h       (per-window refresh-rate gate)
```
