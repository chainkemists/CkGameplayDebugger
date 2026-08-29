---
name: ck-slate-tools
description: "Use when writing or modifying Ck Slate UI, including debugger widgets, list/tree rows, styling, input, viewport interaction, refresh, or teardown behavior."
---

# Ck Slate Tools — pitfall catalog + contracts

Every entry below is a defect that actually shipped and was fixed in this plugin (provenance:
`PROGRESS.md` polish passes + feedback rounds, 2026-07-10/11). Read the matching row BEFORE
writing the pattern it covers. Paths are relative to `Plugins/CkGameplayDebugger/` unless noted.

**Companion docs (don't restate them, read them when relevant):**

| Doc | Owns |
|---|---|
| `Source/CkDebuggerCommon/CLAUDE.md` | Shared widget catalog (`SCkDebug_EntityRef`, search bars, world selector), copy/selectable-text policy, **§"List / tree rows"** (the two click-traps), new-debugger-module checklist |
| `Source/CkEcsDebugger/CLAUDE.md` | Inspector system, panel architecture, priority ladder |
| `.claude/skills/ck-gameplaydebugger-extension/SKILL.md` | Runbooks for ADDING overlay providers / inspectors / whole debugger modules — structure, not pitfalls |

## 1. Rendering & layout pitfalls

| Symptom | Root cause | Fix |
|---|---|---|
| Checkboxes/toggles render as **grey squares** | Referenced an engine style name that doesn't exist in THIS engine (`FAppStyle::Get(), "ToggleButton"` — absent in UnrealEngine-Angelscript 5.7.4). Slate falls back to a **default-constructed style, silently** — no warning, no ensure. | Never trust an engine style name from memory/tutorials — verify it exists in this engine's style set, or register your own (that's why `CkDebugger.ToggleChip` exists in `Source/CkEcsDebugger/Public/CkEcsDebugger/Styles/CkDebuggerStyle.cpp`). |
| Chips / SVG icons **stretch to the column width** | `SVerticalBox`/`SHorizontalBox` slots default to `HAlign_Fill`/`VAlign_Fill`; an `AutoWidth` column is sized by its WIDEST child (e.g. a group label), so every narrower sibling — and the SVG inside it — stretches to match. | Set `HAlign_Left` (or the intent) explicitly on slots inside auto-sized columns. Exemplar + in-code note: `CkDebuggerPanel_EntityList.cpp` `Build_FeatureRail` chip slots. |
| Panel/cards **flicker on every refresh** | `ClearChildren()` + recreate per refresh tick — Slate loses widget state, hover, and paint continuity. Bonus trap: rebuilding from a sort with **unstable ordering** (`TArray::Sort` on equal keys) reorders visually every tick even with a cache. | Stable-identity contract: key→widget cache, mutate the existing widgets in place (`SetText`, attribute lambdas), re-slot ONLY when the key set or order actually changes, and make sort tie-breaks deterministic. Exemplar: Archetypes page card cache (`Pages/CkDebuggerPage_Archetypes.cpp`). |
| Rows/values **lag or never update** | Static `FText` captured at construction. | Bind attributes (`.Text_Lambda`) reading live state; the row is built once, the lambdas track. Inspector rows: `FCkInspectorWidgetBuilder` already does this — call `RequestRebuild()` only on STRUCTURAL change. |
| Debug draws (gizmos, boxes, strings) **blink** | One-frame `DrawDebug*` (Duration 0) re-issued from a **refresh-GATED Slate tick** — when `FCkDebuggerRefreshGate` caps below frame rate, the draw is absent on uncapped frames. | Persistent selection visuals = retained PMG entities moved per tick (`Source/CkDebuggerCommon/Public/CkDebuggerCommon/Markers/CkDebug_PmgGizmoSet.{h,cpp}`). Transient hover affordances = immediate-mode draw from an **ungated** tick (picker hover box). Never one-frame draws from a gated path. |
| PMG shape outlines vanish / stay behind when the shape moves | Assumed outlines re-draw per tick. They don't — `InDrawLines=true` outlines are **baked as a second procmesh section** (`FProcessor_Pmg_DebugShape_BakeLines`, alpha forced 1), which is exactly why they're flicker-free AND move with the shape. | Use `InDrawLines=true` freely on moving debug solids. Do NOT use PMG `Create_Pivot` for live gizmos — Composite children bake world transforms at setup; moving the parent moves nothing. |
| SVG icons render but can't be tinted / tint looks wrong | Colored source SVGs fight `ColorAndOpacity`. | Author glyphs **monochrome white**; tint via `SImage::ColorAndOpacity`. Icons auto-register from `Resources/Icons/**` as `CkDebugger.Icon.<BaseName>` (`CkDebuggerStyle.cpp` `CreateIconBrushes`); resolve via `FCkDebuggerStyle::Get_IconBrush(FName)` — unknown id = no-brush, check for nullptr. |

## 2. Lifetime & teardown contracts (crash-grade)

| Contract | Why / where |
|---|---|
| **Clear every cached `FCk_Handle` on `FEditorDelegates::EndPIE`** — including handles inside static/pending state, not just widget members. | Handles hold the registry by value; one outliving its PIE registry AVs on destruct at the NEXT PIE start (`~FCk_Handle → ReleaseSharedReference`). Full audit list: `Source/CkSmDebugger/CLAUDE.md`. Recent example: the queued-focus static in `CkDebug_Focus.cpp` registers an EndPIE clear for its pending handle. |
| **Tear down handle-holding Slate on `FCoreDelegates::OnEnginePreExit`, NOT `ShutdownModule`.** | By ShutdownModule the registry's shared state is already freed. Pattern + rationale: `Source/CkEcsDebugger/CkEcsDebugger_Module.cpp:60-66,109-121`. |
| Capture `TWeakPtr` in attribute lambdas; capture feature handles **by value** and `ck::IsValid`-check on every read. | Widgets outlive entities and panels. And name the weak local **`WeakPanel`, never `WeakThis`** — `WeakThis` shadows `TSharedFromThis::WeakThis` → C4458 **as error**. |
| Per-entity inspector state (draws, gizmos, markers) is released in `OnDeactivated()` — the entity may ALREADY be invalid when it fires. | `Inspectors/CkDebuggerInspector_Base.h:31`; gizmo sets: `Reset()` there and on EndPIE. |

## 3. Input & interaction pitfalls

| Symptom | Root cause | Fix |
|---|---|---|
| Rows select but clicks "randomly" dead | A click-consuming widget (button-like, `SSpacer` with visibility Visible…) filling the row swallows the hit before the `STableRow`. | The two click-traps + row identity contract: `Source/CkDebuggerCommon/CLAUDE.md` §"List / tree rows". Also: `SListView` items need **stable `TSharedPtr` identity** across refreshes or selection/scroll state resets. |
| Selection echo loops between debugger tabs | Re-broadcasting a selection you just APPLIED from the sync bus. | `ck::DebugSelectionSync` receive-side uses `FApplyGuard`; applies arrive as `ESelectInfo::Direct` and must never re-broadcast. Inverse trap: `Direct` applies also never AUTO-broadcast — programmatic/picker selections that SHOULD sync must call `Broadcast` explicitly (that gap was the "world-pick doesn't show in Crowd/GOAP" defect). |
| Editor CRASHES (stack-death AV) clicking a list row | `OnSelectionChanged` half-honored the Direct rule: it skipped the re-broadcast but still ran the apply path, whose selection-restore calls `SetItemSelection` — which signals SYNCHRONOUSLY, before the list's state settles, so the restore's compare-guard misses and the handler↔restore cycle recurses until the stack dies. | Ignore `ESelectInfo::Direct` with an outright early-return — not by branching only the broadcast. Programmatic paths already own the state they set (`SCkVisualLodDebuggerWindow` roster incident, 2026-08-29). |
| Editor camera-look wedges after adding a global `IInputProcessor` | Consuming RMB down/up starves the level viewport's own RMB tracking — capture never releases. | Pre-processors observing viewport input must be **passive** (return false) + drag-threshold discrimination. And: the editor's context menu OWNS the ejected RMB click — an in-world RMB command can't win that fight (dropped by maintainer decision; only a modifier chord could revive it — ask first). |
| `F`-key/context-menu actions never fire | Widget doesn't take keyboard focus. | Override `SupportsKeyboardFocus() = true` + `OnKeyDown`; keep the context-menu entry as the discoverable twin (see agent list panels). |
| A streaming list steals the game's keyboard input in PIE — every new entry re-steals (WASD dies while walking) | Auto-scroll-to-tail used `SListView::RequestNavigateToItem`, which scrolls AND moves keyboard focus onto the list. | `RequestScrollIntoView` for follow-the-tail scrolling; navigate is only for user-driven selection moves. Fixed in `SCkDebug_EventLog` (VisualLod "Recent Activity" incident, 2026-08-29). Related: never override `SupportsKeyboardFocus` on a whole WINDOW — every click on it then defocuses the viewport; scope it to the one panel that needs it (key events bubble up the focus path anyway). |

## 4. Editor-viewport interaction (PIE)

- **Ejected-vs-possessed detection, deprojection, view camera:** use `ck::DebugViewportView`
  (`Source/CkDebuggerCommon/Public/CkDebuggerCommon/Navigation/CkDebug_ViewportView.{h,cpp}`) —
  never hand-roll. The LEVC path builds view/proj matrices manually because `CalcSceneView`
  returns stale matrices outside a Draw. F8-eject does NOT swap to ADebugCameraController in
  this project; the discriminator is `GEditor->GetActiveViewport() == GCurrentLevelEditingViewportClient->Viewport`.
- **Framing an entity:** `ck::DebugFocus::Focus_Entity` — animated `TransitionToLocation`
  glide (the editor's own F feel), auto-eject while possessed via
  `RequestToggleBetweenPIEandSIE` + ticker completion, distance via `ck.Debug.Focus.DistanceScale`.
  Don't reintroduce raw `SetViewLocation` teleports or engine `FocusViewportOnBox` (version-sensitive).
- **`TSharedPtr<SEditorViewport>` → `TWeakPtr<SWidget>` refuses to convert (C2664/C2440):**
  the derived type is only forward-declared in your TU — smart-pointer conversions need the
  **complete type**. Include `SEditorViewport.h` (see `CkDebug_Focus.cpp`).
- Mesh/world picking must NOT rely on physics traces alone — renderer proxies commonly run
  `NoCollision`; use analytic ray-vs-bounds against gathered snapshots
  (`CkDebuggerModel_ViewportPicker.cpp` `_MeshPickBounds`). Check BOTH `IsmProxy` AND
  `IskmProxy` in any mesh gate — they are separate modules/fragments.

## 5. Module & build hygiene

- Honor the per-window refresh gate in `Tick`: `FCkDebuggerRefreshGate::Should_RefreshNow(WindowId)`
  (`Source/CkDebuggerCommon/Public/CkDebuggerCommon/Window/CkDebuggerRefreshGate.h:29-33`).
  Everything data-refresh goes behind it; only transient immediate-mode visuals stay ungated.
- Unity builds merge anonymous namespaces across `.cpp` files — same-named file-local helpers
  collide (C2374/C2086). Prefix per-file or use a named `ck_<file>` namespace. (Exception:
  `CkEntityDebugOverlay` sets `bUseUnity=false` specifically so its providers can share helper
  names — that license does not travel.)
- New debugger modules: follow the 7-step checklist in `Source/CkDebuggerCommon/CLAUDE.md`
  §"Creating a new debugger module" (UncookedOnly, tab spawner, console toggle, gate, EndPIE,
  OnEnginePreExit, nav/sync registration). Direct CkFoundation usage (e.g. `CkPmg` shapes)
  needs its own `.Build.cs` dependency — transitive links are luck, not policy.
- Gate = toolbox only (`CkAuto/UnrealToolbox.exe --build … --test --test-pattern EcsDebugger`),
  verdict from the log's `=== Test summary ===` block. Visual behavior is `[EDITOR-VERIFY]` —
  say so instead of claiming PIE outcomes.

## When NOT this skill

| You actually want to… | Load instead |
|---|---|
| Add an overlay provider / ECS inspector / whole debugger module (structure, registration, runbooks) | `ck-gameplaydebugger-extension` |
| Author the fragments/Utils the UI displays | `ckecs-architecture-contract` (CkFoundation) |
| Diagnose a framework bug the UI merely surfaced | `ck-debugging-playbook` (CkFoundation) |
