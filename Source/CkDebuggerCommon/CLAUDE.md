# CkDebuggerCommon — Authoring a CK Debugger

> **Read this file first** when creating a new debugger module under
> `Plugins/CkGameplayDebugger/Source/`, or when adding any user-facing surface
> (panels, graph nodes, list rows, search inputs) to an existing one. It is the
> canonical home for cross-debugger conventions, shared widgets, and safety
> rules. Per-module `CLAUDE.md` files only cover that module's own architecture.

## What lives here

`CkDebuggerCommon` is a shared library every debugger module depends on. It
provides:

- **Style tokens** (`CkStyle::`) — colours, spacing, font sizes. These live in
  CkFoundation's `CkEditorTools` module (`CkEditorTools/Style/CkStyle.h`, tunable
  under Editor Preferences → Ck → Style) so debuggers and CkFoundation editor
  tools share one look; CkDebuggerCommon re-exports them via its public
  dependency on `CkEditorTools`.
- **Brush / text style set** (`Styles/CkDebuggerStyle.h` — `FCkDebuggerStyle`) —
  THE registered brush set for the suite: `CkDebugger.Background.*`,
  `Panel.*`, `Row.*`, `Graph.*`, `Badge.Rounded`, the `CkDebugger.Text.*` text
  styles, the translucent `CkDebugger.TableView.Row` selection style, and the
  Brushes and text styles for the suite. ICONS moved to the CkFoundation typed
  registry (`FCkIconStyle::Get_Brush(ECk_Icon, size)` in CkEditorTools) — icons are
  referenced by the GENERATED, compile-checked `ECk_Icon` identifier, never by a
  string key or an SVG basename. `CkDebuggerCommon` owns this set's
  `Initialize`/`Shutdown`; a feature module must never call either. Adopt it
  instead of registering a module-local `FSlateStyleSet`.
- **Shared Slate widgets** (`Widgets/`) — composable building blocks for rows,
  pills, headers, status indicators, copy / selectable text.
- **Search bars** (`Search/`) — `SCkDebug_DualSearchBar` (side-by-side Filter +
  Highlight) and its light sibling `SCkDebug_SearchBar` (one debounced query +
  clear). Used by every debugger that has a list/tree the user searches.
- **Copy-menu helpers** (`Utils/CkDebug_CopyMenu_Utils.h`) — one canonical
  "Copy …" menu shape across schemas, lists, and right-click handlers.
- **Entity reference widget** (`Widgets/SCkDebug_EntityRef`) — single-line
  clickable pill for any `FCk_Handle` display surface. Click opens the entity
  in the CK ECS Debugger; right-click → Copy. See "Entity references" below.
- **Cross-debugger navigator** (`Navigation/CkDebug_Navigator.h`) — the
  one-way registration hook the EntityRef widget uses to invoke the ECS
  Debugger without taking a hard dependency on it.
- **Debugger tool registry** (`Launcher/CkDebuggerToolRegistry.h`) — the
  plain-data, generation-token catalog used by the dockable CK Debugger Launcher.
  Feature modules own registration; the launcher stores tab IDs rather than module callbacks.
- **Window-level plumbing** (`Window/CkDebuggerRefreshGate.h`) — refresh-rate
  gate honouring user settings (Use Global / OnlyWhenVisible / Hz cap).

If you find yourself reaching for `STextBlock`, `FMenuBuilder`, or
`SEditableTextBox` directly, check this file first — the right primitive
probably already exists.

## Severity iconography — one axis, one meaning

`ck::debug_axes::Get_ToneIcon(ECk_Tone)` is the single rule mapping a tone to its glyph (a typed `ECk_Icon`), and it lives beside the
colour ramps because tone → colour and tone → glyph are one axis wearing two hats. A tool that picks its own severity
pictures while binding the shared tone colour is a tool whose picture and colour can drift apart.

| Tone | Glyph | Shape |
|---|---|---|
| `Err` | `Severity_Error` | circle with an × |
| `Warn` | `Severity_Warning` | triangle with a ! |
| `Info` | `Severity_Info` | circle with an i |
| `Ok` | `Severity_Success` | circle with a check |
| `Neutral`, `Accent` | `NAME_None` | nothing — neither is a severity |

Four rules worth knowing before touching them:

- **They are never in the generated decorative pool** (`ck::icons::Get_GeneratedPool()`, the deterministic pool a
  feature without a bespoke glyph is assigned from at random). A severity picture handed out as somebody's arbitrary
  decoration would make the one thing on screen that must mean exactly one thing mean anything at all.
- **Severity reads from SHAPE, not colour.** Every icon in this suite is a monochrome stroke tinted by its
  `ColorAndOpacity`, so the four must stay distinguishable with the tint removed — hence triangle-vs-circle outlines
  rather than four circles with different marks.
- **They are authored here rather than taken from `FAppStyle`.** `FAppStyle::Get().GetBrush("Icons.Warning")` is free
  and is literally the engine's art, and it loses on two counts: an `FAppStyle` brush sits outside Style Lab, so it
  would be the one icon in a window a style revision cannot restyle; and it is an editor style set, while several of
  these modules also ship in packaged Development/DebugGame builds.
- **`Neutral` and `Accent` return `ECk_Icon::None` and callers must draw nothing.** `FCkIconStyle::Get_Brush`
  returns null for `None`, which `SCkDebug_Icon` and `SCkDebug_IconToggle` already handle.

`Ck.DebuggerCommon.Axes.ToneIcons` pins the mapping, the distinctness, that every glyph resolves to a real brush in
**both** style sets (the silent-nullptr class of bug: a typo'd filename draws nothing and reports nothing), and that
none of them is in the decorative pool.

**This replaced per-tool guesswork.** `Skull` previously meant "Critical severity" in CkOptimizationDebugger,
"Failed" in the gallery, "failed candidates" in CkEqsDebugger, "world trouble" in CkCrowdDebugger and "pause on plan
failed" in CkGoapDebugger. All five meant *failure*; all five now use `Severity_Error`.

## Copy & selectable text — the policy

**Default:** every piece of user-visible text that carries information (names,
IDs, paths, expressions, numbers, timestamps) should be copyable. Decorative
labels and short banners may stay as `STextBlock`. When in doubt, make it
selectable.

| Use case | Widget | Notes |
|---|---|---|
| A label or value previously written as `STextBlock` | `SCkDebug_SelectableLabel` | Drop-in. Has `.Text/.Font/.ColorAndOpacity` and an imperative `SetText()`. **Single-line only**, no `AutoWrapText`, no `TransformPolicy::ToUpper`. For uppercase headers, use `STextBlock` or apply `FString::ToUpper()` to the input. |
| An EntityScript class name or gameplay-tag path | `SCkDebug_NameLabel` | **The** name widget — every debugger renders class/tag names through it. Short form selectable, tooltip = full name, tiny »/« button expands/contracts when shortened. `.NameDepth` binds to the debugger's depth tuner; `SCkDebug_NameLabel::Get_ShortName()` is the one canonical shortener for composite strings the widget can't host (crumbs, log lines, graph measurement), `Get_SegmentCount()` feeds a cycler's MaxDepth. Don't use inside `SListView` rows (internal SButton — see click-traps below); use `Get_ShortName` + `STextBlock` there. |
| A toolbar name-verbosity control | `SCkDebug_NameDepthCycler` | The "Name ◀ value ▶" chrome control (canonical cycle: Full(0) ↔ 1 … MaxDepth). State stays with the owner (view model / graph / window member); widget reports via `OnDepthChanged`. Used by GOAP, SM, UI debuggers — add it to any debugger that renders class/tag names. |
| A glyph/icon anywhere in a debugger | `SCkDebug_Icon` | **Never drop a bare `SImage` icon into a slot** — that is how icons ship without tooltips. `.Meaning` (what the glyph stands for) becomes the hover tooltip; `.Brush` comes from the owning module's style registry. Click-passive — safe in `SListView` rows. Exception: an icon inside a control that already carries a richer tooltip (filter-chip SCheckBox, launcher tool button) keeps the wrapper's tooltip instead — one surface, one tooltip. |
| A simple boolean display/debug option | `SCkDebug_IconToggle` or `SCkDebug_IconToolbar` | Keep state and persistence with the feature; bind it through `FCkDebug_IconToggleAction`. Use the toolbar for window-wide settings: every action stays on one horizontal line and never collapses behind an overflow menu. The owning command lane scrolls horizontally at constrained widths; individual icons never wrap into a grid. Use the individual toggle only when the option is contextual to a local panel. Do not create a feature-local checkbox style or icon registry. |
| Composite widget that should support right-click → Copy as a unit (group, pill, custom row) | Wrap with `SCkDebug_CopyableContainer` | Pass `.CopyText(...)` with the multi-line clipboard payload. SButton inside the wrapped child still receives left-clicks; right-click bubbles through. |
| Inspector key/value rows | `SCkDebug_KeyValueRow` (via `FCkInspectorWidgetBuilder::AddRow`) | Values are already `SEditableText` — automatic. |
| An editable number anywhere (inspector row, settings drawer) | `SCkDebug_NumericEditor` | **The** numeric field. Commits on Enter / lost focus, never per keystroke; attribute-bound display frozen while typing; optional min/max; Float and Integer kinds; `OnEditStateChanged` brackets the focus window for the edit guard. Never hand-roll an `SEditableTextBox` for a number. |
| Standalone history row in a fixed-rebuild panel (plan history rail, transition log strip) | `SCkDebug_HistoryRow` with `.CopyText(...)` | Shares tone, accent, selection styling. **Click-trap warning — do NOT use inside `SListView`/`STreeView`/`STableRow`.** Its body is wrapped in an `SButton` that returns `FReply::Handled()` on every left click, so the parent `STableRow` never sees the selection click and the user cannot select rows. See "List / tree rows" section below for the correct pattern. |
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

## List / tree rows — `SListView` / `STreeView` contracts

Two contracts that are easy to miss and break selection silently. Both have been broken in past sessions; both produce the same end-user symptom ("rows flicker, I can't click them").

### 1. Don't put click-consuming widgets inside `STableRow`

`STableRow` detects selection via `OnMouseButtonDown` bubbling up from its content. Any child widget that returns `FReply::Handled()` on a left-mouse-down event traps the click before `STableRow` sees it — the row is rendered but **not selectable**.

The repeat offender is `SCkDebug_HistoryRow`: its body is wrapped in an internal `SButton` (used for the `OnClicked` event), and `SButton` always returns `Handled` on left-click. Despite the widget's name suggesting "row composite for lists", it is for **standalone, fixed-rebuild panels** (e.g. `SCkGoapDebug_HistoryRail` builds them in a `SVerticalBox`, not an `SListView`). Putting one inside an `STableRow` is the bug.

`SCkDebug_SelectableLabel` (built on `SEditableText`) is also a click-trap when sized to fill row width — it captures clicks for cursor-positioning. Use it for headers / value text *outside* of `SListView` rows.

**Safe widgets inside `STableRow`** — they don't consume left-click events, so selection bubbles cleanly:
- `STextBlock`
- `SImage`, `SBox`, `SBorder`, `SHorizontalBox`, `SVerticalBox`, `SSpacer`
- `SCkDebug_StatusPill` (visual-only; no internal button)

**Canonical reference**: [`SCkSchedulerDebugger_ProcessorTree::DoBuildRowContent`](../CkSchedulerDebugger/Public/CkSchedulerDebugger/Widgets/SCkSchedulerDebugger_ProcessorTree.cpp). Builds row content from `SHorizontalBox` + `STextBlock` + `SBox` + `SImage`. Selection styling is delegated to `STableRow.ShowSelection(true)`.

```cpp
// ✓ Correct — STableRow handles selection; row body is plain visual widgets.
return SNew(STableRow<TSharedPtr<FRowItem>>, InOwnerTable)
    .Padding(FMargin{0.0f, 1.0f})
    .ShowSelection(true)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth() [ /* status dot via SImage + SBox */ ]
        + SHorizontalBox::Slot().FillWidth(1.0f) [ SNew(STextBlock).Text(...) ]
    ];

// ✗ Wrong — SCkDebug_HistoryRow's internal SButton consumes the click; STableRow never selects.
return SNew(STableRow<TSharedPtr<FRowItem>>, InOwnerTable)
    [ SNew(SCkDebug_HistoryRow).TitleText(...).Tone(...) ];
```

Right-click "Copy ..." menus go on `SListView::OnContextMenuOpening` (not on the row body), so per-row right-click works without needing `SCkDebug_HistoryRow`'s `.CopyText(...)` argument.

### 2. Keep row-item `TSharedPtr` identity stable across refreshes

`SListView` / `STreeView` track selection by **pointer identity**. If your refresh handler does `_Items.Reset()` + `MakeShared<FRowItem>` per item every Tick, the user's selection is destroyed every tick — they click a row and the selection vanishes by the next paint. Visible symptoms: "rows flicker", "can't click rows", "selection won't stick".

The fix: index existing items by a stable key (entity handle, processor index, candidate index, ...) and **reuse the existing `TSharedPtr` when the key matches**. Allocate new shared pointers only for genuinely new items; drop ones for vanished items. Update displayed fields in place via `*Item = NewData`.

```cpp
// ✓ Correct — reuse existing pointers across refreshes; in-place field updates.
auto Existing = TMap<FStableKey, TSharedPtr<FRowItem>>{};
for (const auto& I : _Items) { if (I.IsValid()) Existing.Add(I->Key, I); }

auto NewItems = TArray<TSharedPtr<FRowItem>>{};
auto SetChanged = false;
for (const auto& Source : InSourceList)
{
    auto Item = TSharedPtr<FRowItem>{};
    if (auto* Found = Existing.Find(Source.Key))
    { Item = *Found; *Item = Source; Existing.Remove(Source.Key); }   // stable pointer
    else
    { Item = MakeShared<FRowItem>(Source); SetChanged = true; }       // new entry
    NewItems.Add(MoveTemp(Item));
}
if (Existing.Num() > 0) { SetChanged = true; }                        // vanished entries

_Items = MoveTemp(NewItems);
if (SetChanged) { _ListView->RequestListRefresh(); }                  // only on set change
```

Two consequences worth knowing:
- **`RequestListRefresh` only when the SET changes.** In-place field updates don't need a refresh — the existing row widgets stay valid. (The widgets won't auto-reflect the field change unless they bind via `TAttribute<FText>` lambdas reading from the item; the alternative is "rebuild structure on set change, accept stale fields between rebuilds." For most debug data — completed queries, completed plans — the latter is fine.)
- **Selection-restore on refresh** should compare against current selection before calling `SetItemSelection` to avoid spurious `OnSelectionChanged` firings:
  ```cpp
  const auto Cur = _ListView->GetSelectedItems();
  const auto AlreadySelected = (Cur.Num() == 1 && Cur[0] == NewSelection);
  if (NewSelection.IsValid() && NOT AlreadySelected)
  { _ListView->SetItemSelection(NewSelection, true, ESelectInfo::Direct); }
  ```
  And `OnSelectionChanged` should ignore `ESelectInfo::Direct` so the programmatic restore doesn't echo back into the ViewModel.

`SCkSchedulerDebugger_ProcessorTree` is the canonical reference for both of these patterns — it updates existing tree-node `TSharedPtr` instances in place (`InNode->IsVisible = ...`) and only calls `RequestTreeRefresh` when structure changes.

## Graph nodes — SGraphNode live-bind invariant

**SGraphNode visuals must bind via `TAttribute` lambdas, not construction-time reads.**

`UEdGraph` runtime state — flags like `IsInPlan`, selection state, role badges — changes between snapshots without the topology changing. If `SGraphNode` subclasses read these flags at construction time (e.g., `.BorderColor(Compute_BorderColor(_Node))`), the only way the visual updates is by recreating the node widget. That recreation is flicker.

The pattern: bind flag-driven visuals via `TAttribute<T>` lambdas with `TWeakObjectPtr` capture + null guard:

```cpp
.BorderBackgroundColor_Lambda([WeakNode = TWeakObjectPtr<UMyDebugNode>(_Node)]()
{
    auto Node = WeakNode.Get();
    if (Node == nullptr) { return DefaultColor; }
    return Compute_BorderColor(Node);
})
```

Corollary: `UEdGraph::NotifyGraphChanged` triggers `SGraphPanel::OnGraphChanged` which recreates node widgets. **Never emit `NotifyGraphChanged` from an in-place runtime-update path** — only from the topology rebuild. If your refresh splits into `topology_changed` + `runtime_changed` branches (it should), only the topology branch notifies.

## Domain ramps — never write a heat/score/category hex

Three colour scales live next to the style axes in
`Styles/CkDebuggerAxes.h` (`namespace ck::debug_axes`). Every stop is derived
from a `CkStyle::` role, so a palette edit moves all of them at once:

| Helper | Ramp | Replaces |
|---|---|---|
| `Get_HeatColor(Normalized)` | `Ok` → `Warn` (at 0.5) → `Err` | Scheduler's four-band `Get_TimingColor`, Insights' budget green/yellow/red |
| `Get_ScoreColor(Normalized)` | `Info` → `Ok` | Eqs's candidate score gradient |
| `Get_CategoricalColor(Index)` / `(FName)` | 8 distinct palette roles, wrapping | ECS inspector-filter fallback palette |

All three are **total**: ramps clamp and read NaN as 0, the categorical index
wraps in both directions. Pass a raw ratio and don't pre-clamp.

Semantic canvas colours that only mean something inside one visualization
(AStar grid cell states, Crowd navmesh paint, Map fog) stay local to their
module with a one-line justification comment.

## Time-series widgets

| Use case | Widget |
|---|---|
| Per-frame history the user scrubs / freezes (frame times, tick costs) | `SCkDebug_FrameStrip` — heat columns, budget or relative scale, scrub + pan + zoom, highlight bands, marker dots, LIVE/FROZEN gutter label |
| A run timeline of coloured spans with a cursor (state machine runs, plan history) | `SCkDebug_ScrubTimeline` — segments with optional subdivision cells, Live/Scrub modes, cut / flag / dot / tick marks, widget-owned pan+zoom window, ruler with a caller-supplied label formatter |
| A capped rolling list of "what just happened" | `SCkDebug_EventLog` — severity tones, optional category chip, timestamps, multi-select right-click copy |
| Lanes of time-positioned markers + activation spans | `SCkDebug_EventTimeline` |
| A value trend in a row or card | `SCkDebug_Sparkline` |

`FrameStrip` and `ScrubTimeline` are `SLeafWidget`s: they own no children, so
their tooltips are hover-tracked through the widget-level tooltip, and their
right-click copy is opt-in via `.CopyText(...)` (a right-*drag* still pans).
`EventLog` rows are built from `STextBlock` + the axis chip helper only, so
they are safe inside `SListView` — see the list-row contract above.

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

Debugger tabs persist across PIE, but registry-backed `FCk_Handle` values must
not. Every debugger model, collector, or widget that owns handle copies must
subscribe its owning window/controller to
`ck::DebugSessionLifecycle::Get_OnSessionInvalidated()` and synchronously clear
those copies. Common broadcasts this handle-free signal on BeginPIE and EndPIE;
feature modules remain responsible for their own state. The shared
`FCkDebuggerModel_WorldSelector` also clears its selection and broadcasts
`OnWorldChanged` from `FWorldDelegates::OnWorldCleanup` when its selected world
starts teardown. Route both that notification and explicit world switches
through the same feature-owned reset.
Validity guards are only defensive read boundaries and do not replace this
ownership reset.

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
2. **Window + view-model**: own a `SCkXxxDebuggerWindow`, wrap its specialized root in
   `SCkDebug_WindowChrome`, and own a view-model
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
   identity (see "PIE lifecycle" above). Poll the available-world list on a
   bounded tick gate, subscribe to the common session-invalidation signal, and
   route both session and explicit selected-world changes through the same
   feature-owned reset.

## Common window chrome and entity targeting

- Every plugin-owned standalone debugger tab uses `SCkDebug_WindowChrome`. The dock tab owns the
  debugger name; Chrome must not repeat a title or identity icon. Chrome owns the directly visible
  semantic command lanes, the right-anchored Tools switcher, body boundary, and trailing utilities.
  Primary and context lanes each remain one physical line and scroll horizontally when narrow;
  controls and icon actions never wrap into additional rows. `CkInsightsDebugger` follows the same
  contract while consuming UI-free trace analysis and report APIs from CkFoundation.
- Feature modules never construct `SCheckBox` directly. Use `SCkDebug_IconToggle` or
  `SCkDebug_IconToolbar` for a fixed icon-backed boolean, `SCkDebug_ToggleSurface` when a contextual
  card/chip owns rich content, and engine `SSegmentedControl` for short exclusive choices. The
  feature still owns state and behavior; Common owns checkbox presentation, accessibility, and
  interaction semantics.
- Entity-capable debuggers register one generation-token-protected
  `FCkDebug_EntityTargetRoute` after their tab spawner and unregister it before teardown. The
  predicate and open callback must resolve the same real target; an open-only route is invalid.
- **Viewport picker (shared).** `Picker/CkDebug_ViewportPicker.h` is the suite-wide
  "click-to-select in the viewport" mode; `Picker/SCkDebug_ViewportPickerControls.h` is its
  toolbar surface (Pick button + gear popover). A debugger window owns one picker, constructs it
  with `Get_TargetWorld` / `OnEntityPicked` / optional `TargetFilter`, ticks it UNGATED from its
  own `Tick`, and deactivates it in its world/session reset. `TargetFilter` restricts the preview
  to entities the debugger supports **plus their lifetime-owner chain up to the top-most
  non-transient, non-ActorRelay ancestor** (the conceptual NPC) — see
  `FCkDebug_EntityMarkers::FGatherParams::TargetMatch`. Keep the predicate as ONE public static on
  the window class and reuse it from the module's `FCkDebug_EntityTargetRoute`, so picker and
  route resolve the same real target; `OnEntityPicked` typically broadcasts SelectionSync and
  routes through `FCkDebug_EntityTargetRegistry::TryOpenAndTarget`. The overlay focus card +
  world tags come via the `ck::DebugPickerCards` factory slot that `CkEntityDebugOverlay` fills
  at module startup — consumers need no extra module deps. The ECS debugger is the one
  unfiltered consumer; GOAP / Crowd / AStar / SM / Intent are the specialized exemplars.
- Resolve parent/child selections with `DebugSelectionSync::Resolve_ClosestLineageMatch`. It
  checks exact, ancestor, and descendant entities, excludes sibling branches/transient-root
  cross-matches, and uses stable entity-id tie-breaking.
- `SCkDebug_WindowChrome` automatically exposes `Sync from ECS <id>` only when its tab has a registered
  route. `SCkDebug_EntityDebuggerLinks` discovers those same routes for ECS inspector `Open In`
  actions. Common pulls the primary ECS selection on demand and never retains a PIE handle.
- `ck.Debug.SelectionSync.OverlayFocus` is the opt-in continuous path for the on-screen overlay.
  It broadcasts only when the full focused handle changes; receivers update already-open compatible
  tabs and never open or foreground a debugger.
- If a debugger's collector populates after the tab opens, its feature window owns a short-lived
  pending target and applies it after the first refresh. Clear pending/cached handles before the
  PIE registry dies.

## Common 3D preview shell

- `Viewport/SCkDebug_3dPreviewViewport` owns the runtime-safe preview world, scene viewport, camera/navigation,
  camera presets and bookmarks, one icon-first common render/grid/frame/follow/isolate control strip, projection,
  and teardown. Fly speed is a shared per-user `UCkDebuggerWindowSettings` preference adjusted by perspective
  RMB + wheel and restored by every new Common preview client.
- `Viewport/CkDebug3dInteractionRouter` owns neutral click, additive selection, drag sequencing, plane shifts,
  hover throttling, and focus-loss cleanup. Feature adapters resolve opaque identities and execute specialized
  behavior; they do not create a second viewport client.
- A feature adapter declares capabilities and translates its own settings and commands. Jolt retains pause,
  step, authority-gated physics drag, contacts, probes, palettes, and labels; Crowd retains source selection,
  VoxelNav, Recast, PathNetwork, CVars, and details.
- Preview geometry belongs in the runtime `CkDebugScene` target. Never retain gameplay-world actors, ECS handles,
  navigation objects, or world pointers in the preview scene; collect first and publish copied values.

## Quick reference — file paths

```
CkDebuggerCommon/
├── Launcher/
│   └── CkDebuggerToolRegistry.h      (standalone debugger descriptor catalog + reload-safe token)
├── Navigation/
│   ├── CkDebug_EntityTarget.h        (feature-owned open-and-target route registry)
│   ├── CkDebug_Navigator.h           (Register/Goto_Entity for cross-debugger nav)
│   └── CkDebug_SelectionSync.h       (broadcast, ECS provider, and closest-lineage resolver)
├── Picker/
│   ├── CkDebug_ViewportPicker.h      (shared click-to-select viewport pick mode; host-configured world/pick/filter)
│   ├── CkDebug_ViewportPickerInputProcessor.h (Slate pre-input processor while pick mode is active)
│   ├── CkDebug_PickerOverlayCards.h  (focus-card/world-tag hook — CkEntityDebugOverlay fills the factory slot)
│   └── SCkDebug_ViewportPickerControls.h (the toolbar Pick button + settings popover)
├── Search/
│   ├── SCkDebug_DualSearchBar.h      (Filter + Highlight side-by-side)
│   └── SCkDebug_SearchBar.h          (single debounced query + clear)
├── Styles/
│   ├── CkDebuggerStyle.h             (THE suite brush/text set + SVG icon registry)
│   ├── CkDebuggerCommonStyle.h       (glow halos, flat button, icon-toggle checkbox)
│   ├── CkDebuggerAxes.h              (axis render/metric/predicate helpers + domain ramps)
│   └── CkDebuggerStyleSelection.h    (the axis enum catalog)
├── Utils/
│   ├── CkDebug_CopyMenu_Utils.h      (Handle_RightClickToCopy, AddCopyEntry, AddCopyEntryToToolMenu)
│   ├── CkDebug_InspectorEditGuard.h  (panel-scoped "edit in flight" registry + RAII scope; defers rebuilds)
│   └── CkDebug_RequestGate.h         (ck::DebugRequestGate — net-mode -> {enabled, reason} for debug write controls)
├── Viewport/
│   ├── CkDebug3dInteractionRouter.h  (neutral pick/drag/hover/command sequencing)
│   └── SCkDebug_3dPreviewViewport.h  (runtime-safe preview shell, camera, common controls, bookmarks)
├── Widgets/
│   ├── SCkDebug_EntityRef.h          (clickable FCk_Handle pill — navigates to ECS Debugger)
│   ├── SCkDebug_Icon.h               (THE icon widget — glyph + mandatory Meaning tooltip, click-passive)
│   ├── SCkDebug_IconToggle.h         (boolean icon action + always-visible direct toolbar)
│   ├── SCkDebug_NameDepthCycler.h    (toolbar "Name ◀ value ▶" verbosity control)
│   ├── SCkDebug_NameLabel.h          (THE class-name/tag label — depth-shortened, »/« expand; Get_ShortName/Get_SegmentCount statics)
│   ├── SCkDebug_SelectableLabel.h    (STextBlock-shape, copyable)
│   ├── SCkDebug_CopyableContainer.h  (wrap any widget with right-click → Copy)
│   ├── SCkDebug_KeyValueRow.h        (inspector row; values selectable)
│   ├── SCkDebug_NumericEditor.h      (THE numeric entry field — commit on enter/lost focus, float + int)
│   ├── SCkDebug_SectionHeader.h      (uppercase section header)
│   ├── SCkDebug_StatusPill.h         (toned status label)
│   ├── SCkDebug_HistoryRow.h         (history/timeline row + opt-in CopyText)
│   ├── SCkDebug_FrameStrip.h         (scrubbable per-frame heat columns)
│   ├── SCkDebug_ScrubTimeline.h      (segment track + scrub cursor + marks)
│   ├── SCkDebug_EventLog.h           (capped severity-toned event list)
│   ├── SCkDebug_NodePill.h           (graph / plan-step pill + opt-in CopyText)
│   ├── SCkDebug_InspectorPanel.h     (collapsible section)
│   └── SCkDebug_CountBadge.h
└── Window/
    ├── CkDebuggerRefreshGate.h       (per-window refresh-rate gate)
    └── SCkDebug_WindowChrome.h       (shared switcher/content/status frame)
```

Style tokens moved to CkFoundation:

```
CkFoundation/Source/CkEditorTools/Public/CkEditorTools/
├── Style/
│   └── CkStyle.h                     (CkStyle:: colour + spacing tokens, ECk_Tone)
└── Settings/
    └── CkStyleSettings.h             (UCk_Style_UserSettings_UE — Editor Preferences → Ck → Style)
```
