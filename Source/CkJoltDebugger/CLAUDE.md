# CkJoltDebugger — the Jolt physics world, rendered somewhere you can actually look at it

> **Read [CkDebuggerCommon/CLAUDE.md](../CkDebuggerCommon/CLAUDE.md) first.** It covers the shared
> conventions this module obeys without restating them: window chrome and command lanes, the icon-toggle
> rule, list-row contracts, the PIE world lifecycle, and the safety rules. This file covers only
> CkJoltDebugger's own architecture.

## Identity (verified 2026-08-15)

**DeveloperTool** module — included in editor and packaged Development/DebugGame, excluded from
Test/Shipping. One nomad tab (`CkJoltDebugger`, label "CK Jolt Physics"), opened by the
`ck.JoltDebugger` console command (`[0/1]`, or bare to toggle) or from the shared launcher
(**Systems** category, slot 30, `Cube` glyph). The window is an **outliner** rail (0.22) / preview-world
**viewport** (0.53) / **stats + detail** rail (0.25).

**The tab id is defined exactly once**, as `SCkJoltDebuggerWindow::TabId`. The module reads it through
`FCkJoltDebuggerModule::Get_DebuggerTabName()` rather than declaring an `FName` of its own — two
`FName` constants in two translation units have no ordered initialisation between them, and the loser
would silently be `NAME_None`. The tab spawner, the launcher descriptor, the entity-target route and
the chrome's `Sync from ECS` action all name that one value.

**Depends on:** `CkCore`, `CkEcs`, `CkJolt`, `CkSpatialQuery` (the Probe fragment behind the sensor
population), `CkDebuggerCommon`, `CkEditorTools`, plus `RenderCore`, `RHI`, `InputCore` and `UMG` for
the viewport shell (`FSceneViewport` + `FUMGViewportClient`), and `UnrealEd` /
`WorkspaceMenuStructure` behind `Target.bBuildEditor`.

---

## Ownership boundary — the rule the whole module exists under

**`CkJolt` owns all debug data AND all rendering.** It ships `FCk_Jolt_DebugDrawTarget` (per-world
retained ISM buckets, materials, palette, class visibility, content bounds) and the capture processor
that pumps every registered, demanding target inside the physics pipeline's async-safe window. **This
module owns presentation only**: a preview world to render into, a camera, and the controls that flip
the facility's own switches.

Two hard consequences:

- **This module never touches `JPH::PhysicsSystem`.** Slate ticks whenever it likes; the physics step
  may be in flight on worker threads. Every Jolt read happens in the capture processor, on the game
  thread, at the one point in the frame where it is safe. A debugger that reached into the physics
  system directly would be reading a half-stepped world — and would do it intermittently, which is
  worse than doing it never.
- **This module never includes `CkJolt_DebugDrawTarget_Impl.h`.** That header is CkJolt-internal and
  leaks JPH types; inclusion *is* the privacy boundary there (CkJolt has no `Private/` dir). The public
  `CkJolt/Subsystem/CkJolt_DebugDrawTarget.h` is JPH-free by design and is the entire surface this
  module is entitled to.

Nothing here re-derives a colour, a bound, or a body count that the facility already computed.

---

## The viewport

`SCkJoltDebugger_3dViewport` (`Public/CkJoltDebugger/Viewport/`) is an `SViewport` hosting an
`FPreviewScene` (**no default lighting, no physics scene, non-editor**) behind an `FSceneViewport` and a
private `FCkJoltDebugger_3dViewportClient : FUMGViewportClient`. The shell is copied from
`SCkCrowdDebugger_3dViewport` — camera math, input handling and per-frame invalidate transfer verbatim.

**The widget draws NOTHING.** There is no `Draw` / PDI override and no grid. The facility's instanced
static meshes are registered into `Get_PreviewWorld()` and render because they are *in* that world. If
you find yourself adding a PDI call here, the thing you want to draw belongs in CkJolt.

The scene is unlit and physics-free on purpose: the debug-draw materials are unlit, so lighting would
change nothing, and a preview world that simulated would fight the world being inspected.

### Camera

| Preset | Projection | Notes |
|---|---|---|
| Perspective | Perspective | default; the only mode with flight |
| Top / Bottom / Left / Right / Front / Back | Orthographic | fixed rotation, then frames content |
| Frame All | unchanged | frames `Get_ContentBounds()`; **Home** |
| Frame Selection | unchanged | frames `Get_HighlightedBodyBounds()`; **F**; inert with no selection |

Orbit RMB · pan MMB · wheel zoom · RMB+wheel camera speed · WASD / QE / arrows flight (perspective
only) · **Home** = Frame All · **F** = Frame Selection · plain LMB = pick.

**Ortho framing backs the eye off along −view by the bounding-sphere radius plus a margin.** Sitting the
eye on the box centre — the shape inherited from the Crowd viewport — puts half the content behind the
near plane, which is invisible with wireframes and obvious the moment the bodies are solid.

**Frame Selection frames the highlighted body regardless of class visibility; Frame All does not.**
That asymmetry is deliberate. `Get_ContentBounds()` excludes hidden classes because a camera must never
frame what the viewer cannot see, but the Highlight overlay is its own colour class with no toggle — a
selection is always visible, so framing it is always meaningful. The selection bounds come from the
body's NORMAL instance, so a row click frames on THAT click without waiting for the next capture.

### Click-picking

A plain left click (no RMB/MMB held — those are camera drags) deprojects the cursor and asks the
facility for the nearest live instance: `TryPick_Body(Origin, Direction)`, an oriented-box test in
instance space. **The widget never raycasts the physics world** — the preview scene has no physics
scene, and the game world's belongs to a step this module must not touch. A click on empty space
clears the selection; a click that hits an instance no row can be found for leaves the selection
alone, because the click did hit something.

---

## The outliner

`SCkJoltDebugger_OutlinerPanel` lists one row per body-backing entity of the selected world, collected
by `FCkJoltDebugger_DataCollector` on the window's refresh-gated Tick. Four populations, one flat list:
`JoltBody`, `BakedStatic` (a JoltStaticActor attribution entity), `Sensor` (a CkSpatialQuery Probe),
`Character`.

- **Row identity is `(Handle, Population)`, not the handle alone.** One entity can back two populations
  at once — a JoltBody that also owns a Probe — and a handle-keyed map would collapse those into one
  row and churn the list every pass. Identity drives the `SListView` pointer-reuse contract
  (`CkDebuggerCommon/CLAUDE.md` §"List / tree rows"): items are mutated in place and
  `RequestListRefresh` only fires when the visible SET changes.
- **`BodyKey` is `TOptional<uint64>`.** A row with no drawn body behind it — a baked actor whose bodies
  were already removed — has none, and 0 is a VALID Jolt body key, so a sentinel would alias onto the
  first body ever created. Rows are looked up by handle wherever a lookup has the choice.
- **Dual search**: the Filter query hides rows; the Highlight query dims the survivors. Both match what
  the row renders (name, population pill, detail text) plus the body key.
- **An external selector reveals its row.** `SelectByHandle` / `SelectByEntity` search the UNFILTERED
  set; if the match is currently filtered out they clear the FILTER query and then select it. A
  selection the user cannot see is indistinguishable from no selection, which would read as a broken
  "Open In". The Highlight query is left alone — it dims, it never hides.
- Right-click copies the row text or the full entity handle (`SListView::OnContextMenuOpening`, never a
  click-trapping widget inside the row).

---

## Selection model

The window owns the whole model: `TOptional<FCkJoltDebugger_BodySnapshot> _Selection`, single-select.
`DoApplySelection` is the ONE writer; every source funnels through it and every sink is driven from it.

| Source | Path |
|---|---|
| Outliner row click | `OnSelectionChanged` (non-`Direct` only) |
| Viewport click | `TryPick_Body` → key → row |
| Global `DebugSelectionSync` | `Resolve_ClosestLineageMatch` with `Is_JoltDebuggerEntity` |
| `FCkDebug_EntityTargetRoute` ("Open In") | `TargetEntity` |
| Game-viewport picker | broadcast + route, so it re-enters through the two above |

| Sink | Path |
|---|---|
| Facility highlight | `Set_HighlightedBody(BodyKey)` — an unset key clears it |
| Viewport framing | `Set_SelectionBounds(Get_HighlightedBodyBounds())` |
| Outliner row | `SelectByHandle` under `FApplyGuard`, `ESelectInfo::Direct` |
| Detail panel | reads `_Selection` through its `GetSelection` attribute |
| `DebugSelectionSync::Broadcast` | **user-originated sources only** |

**Echo suppression is triple-guarded**: only Outliner/Viewport (the two user-driven sources)
re-broadcast; programmatic selects use `ESelectInfo::Direct`, which `OnSelectionChanged` ignores; and
`HandleGlobalSelectionSync` drops anything tagged with this tab's own id.

**Selection facts refresh by ENTITY, not body key** — a body key can be unset, and a re-baked static
actor keeps its entity while every one of its body ids changes. A selection whose row leaves the world
is dropped rather than left pointing at a released slot.

### Pending target

A route can arrive before the collector has ever run (the tab opens with the click). The window then
holds ONE pending target — the entity **plus the world it was resolved in** — and retries on the next
refresh. Three rules keep it from becoming a thief:

- It is applied only when its world IS the selector's current world. An entity id means nothing outside
  its own registry; matching it against another world's ids would select a stranger.
- It is dropped the moment any non-external selection happens, so it cannot land a refresh later and
  take the row the user just clicked.
- It has a one-pass retry budget: once a refresh HAS produced rows and none of them matched, the target
  is not in this world's collection and the pending target is cleared.

---

## Detail panel

`SCkJoltDebugger_DetailPanel` — `SCkDebug_KeyValueRow`s built once, every value a `TAttribute` lambda
reading the window's live selection. Rows that do not apply to the selected population read `--` rather
than disappearing, so the panel's height never jumps as the selection moves.

Rows: Entity (`SCkDebug_EntityRef`), Population, Motion Type, Sleep State, Body Key, Linear Velocity,
Source Actor, Baked Bodies.

**Linear velocity comes from the facility, never from `UCk_Utils_JoltBody_UE`.** The capture processor
samples the highlighted rigid body's velocity in the physics pipeline's async-safe window and publishes
it as `FCk_Jolt_DebugDrawTarget::Get_HighlightedBodyLinearVelocity()`; the window copies that into the
selection facts. A Slate-side `Get_LinearVelocity` call would lock a body interface on a world whose
step may be in flight — intermittently wrong is worse than never. Consequences to expect in the UI:
the row reads `--` for a character (a `CharacterVirtual` has no rigid-body velocity), and for a
sleeping or static body once the scene-revision pass that drew it has passed, because the sample
belongs to a capture rather than to the selection.

---

## Target lifecycle and demand

The **window**, not the viewport, owns one `FCk_Jolt_DebugDrawTarget`, constructed against the
viewport's preview world. Which *game* world it captures is the shared
`FCkDebuggerModel_WorldSelector` selection; the target is registered with that world's
`UCk_Jolt_Subsystem` and re-registered whenever the selection changes. The subsystem is held
**weakly** — the game world dies while this window lives.

```
demand = (selected world valid && HasBegunPlay) && tab is visible
```

**Demand is pushed, not only pulled.** `SDockingTabStack` detaches a backgrounded tab's content, so
`Tick` stops firing — a demand evaluated only from `Tick` would latch ON at its last-seen value and the
capture processor would keep filling an invisible preview world every frame, forever. The window
therefore subscribes `FGlobalTabmanager::OnTabForegrounded` and re-runs the same sync from there. The
tab well updates its foreground index *before* broadcasting, so the visibility answer inside that
handler is already the post-switch one, in both directions. The `Tick` sync stays as the steady-state
path (registration changes, a world that begins play later); it is a pointer compare and a bool, never
a Slate rebuild.

The destructor sets demand **off explicitly before unregistering** — dropping a target that is still
desired leaves its last instances standing.

### Route and picker — one predicate

`SCkJoltDebuggerWindow::Is_JoltDebuggerEntity` is the single answer to "can this window show that
entity" (`Has` any of the four `_Current` fragments). The module's `FCkDebug_EntityTargetRoute` and the
shared game-viewport picker's `TargetFilter` both call it, so a route that opens the tab and a pick
that previews an entity can never disagree. The route is registered AFTER the tab spawner and
unregistered BEFORE it; its predicate and its open callback resolve the same real target, so it is
never an open-only route.

The picker (`FCkDebug_ViewportPicker` + `SCkDebug_ViewportPickerControls`, in the `JoltTarget` lane)
ticks UNGATED from the window's Tick — it drives the GAME viewport and must keep up with the display
even when this window's refresh gate is throttled — and is deactivated on world change, session
invalidation and destruction. Its `OnEntityPicked` broadcasts SelectionSync and routes through
`FCkDebug_EntityTargetRegistry::TryOpenAndTarget`, so a pick lands on the same row an ECS "Open In"
would.

### Teardown

- **`ck::DebugSessionLifecycle::Get_OnSessionInvalidated()` is the single teardown path.** There is
  deliberately **no direct `FEditorDelegates::EndPIE` handler here**: `CkDebuggerCommon`'s module binds
  both BeginPIE and EndPIE and broadcasts session-invalidated from them, so a local EndPIE handler would
  be a second route to the same reset — one more thing to keep in agreement for no coverage.
- **`FCoreDelegates::OnEnginePreExit`** drops the module's window and tab refs so the target unregisters
  from a live subsystem while the world still exists. NOT `ShutdownModule` — too late.
- The world selector also clears itself from `FWorldDelegates::OnWorldCleanup`; that arrives as
  `OnWorldChanged` and routes through the same reset.

---

## Command lanes

**Primary = what acts on THIS viewport. Context = everything else.**

| Lane | Group | Contents |
|---|---|---|
| Primary | `JoltRender` | wireframe/solid toggle (`Grid`) — swaps the facility's materials, rebuilds no geometry |
| Context | `JoltTarget` | `SCkDebug_WorldSelector` |
| Context | `JoltCamera` | 8 icon buttons (7 presets + Frame All) |
| Context | `JoltInWorldDraw` | `ck.Jolt.DebugDraw.Enabled` / `.Velocity` |
| Context | `JoltPopulations` | four class-group toggles |
| Context | `JoltLegend` | one swatch + label per colour class, colours bound from the target's palette |

### ⚠ In-world draw is NOT this viewport

**The single most confusing thing about this window.** The two CVar toggles in `JoltInWorldDraw` gate
the *subsystem's* debug draw into the **game viewport**. They do nothing to the pane above them, and the
viewport renders whether they are on or off. That is why they are labelled "In-world …", why both
tooltips name the game viewport explicitly, and why they are not in the Primary lane — a lane position
next to the viewport reads as ownership of it.

### Population toggles → colour classes

Each toggle drives `Set_ClassVisibility` on every class in its group and reports the state of the
group's first class (they are only ever flipped together from here).

| Toggle | Icon | Colour classes |
|---|---|---|
| Jolt Bodies | `Jolt` | `Static`, `Kinematic`, `Dynamic_Awake`, `Dynamic_Sleeping` |
| Baked Static World | `World` | `BakedStatic` |
| Sensors | `Probe` | `Sensor` |
| Characters | `Person` | `Character` |

Hiding a class is **component visibility, not a capture skip** — the facility keeps capturing it, so
unhiding shows current poses rather than a frozen snapshot, and `Get_ContentBounds()` (the camera's
framing source) excludes hidden classes because framing invisible content is never what the user meant.

---

## Tests

`Private/Tests/*.spec.cpp`, whole file inside `#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS`,
`IMPLEMENT_SIMPLE_AUTOMATION_TEST`, `EditorContext | EngineFilter`.

| Spec | Asserts |
|---|---|
| `Ck.JoltDebugger.Window.ConstructsWithoutSlotAttributeEnsure` | the window builds and prepasses with no slot-attribute ensure |
| `Ck.JoltDebugger.Viewport.ConstructsWithoutEnsure` | the viewport builds ensure-free and owns a preview world |
| `Ck.JoltDebugger.Viewport.CameraPresets` | every preset sets its projection mode and points the camera down its expected axis (compared as forward vectors — rotator normalization must not make it flaky) |
| `Ck.JoltDebugger.Viewport.FrameAllWithoutContentIsInert` | framing invalid bounds leaves the camera untouched instead of snapping to the origin |
| `Ck.JoltDebugger.Outliner.ConstructsWithoutEnsure` | the panel builds, reconciles an EMPTY pass and a `Clear` without an ensure, and resolves an absent handle to nothing |
| `Ck.JoltDebugger.Outliner.RowsSelectFilterAndSurviveRefresh` | three rows on three real entities (a standalone `ck::FEcsWorld`): select-by-handle finds the right row, a refresh over the same entities KEEPS the selection (the pointer-reuse contract), the filter hides non-matches, and an external select reaches a filtered-out row and reveals every row again |
| `Ck.JoltDebugger.Detail.ConstructsWithoutEnsure` | every value lambda survives a completely unbound `GetSelection` and reads `--` |
| `Ck.JoltDebugger.Detail.RowsReflectTheSelection` | the built rows render the bound selection's own values (population / motion / sleep / body key), an unsampled velocity reads `--` and a sampled one reaches the row, an unset body key degrades to `--`, and clearing the selection empties every row |

Real entities for a row fixture come from `ck::FEcsWorld{}` + `UCk_Utils_EntityLifetime_UE::
Request_CreateEntity(World.Get_Registry())` — the same shape the ECS-debugger specs use. Rows are
addressed through the panel's own read surface (`Get_Selection`, `Get_NumVisibleRows`,
`Set_FilterQuery`) rather than by reaching into its members.

Also run `Ck.DebuggerLauncher` — its catalog spec asserts the exact tool census, so renaming this tab id
requires editing that spec in the same change. `Ck.Jolt.DebugDraw.*` (CkTests) covers the facility this
module consumes, including the highlight overlay, the pick math and the velocity sample this window
renders.

### What the specs cannot reach — `[EDITOR-VERIFY]` only

Headless specs construct widgets; they cannot render, and they cannot give a target real content
(content bounds come from captured ISM buckets, which need a live Jolt world). These remain manual:

- bodies actually render in the viewport, and match the in-world draw
- framing on real content — Frame All / the ortho presets landing on the bodies
- the ortho eye offset — solid bodies not clipped by the near plane in Top/Front/etc.
- demand goes off when the tab is backgrounded (click a sibling tab during PIE)
- the window survives PIE end without crashing, showing its last state
- the outliner lists all four populations with clean names, and the search filters them
- a row click highlights that body in the viewport; Frame Selection and `F` frame it
- a viewport click on a body selects its row — including a body PAST THE FIRST of a baked static
  actor, which resolves through the collector's owner index
- ECS debugger → a Jolt entity → "Open In → Jolt" lands on the row; "Sync from ECS" works; both reach
  a row the filter is currently hiding
- the game-viewport picker previews and picks only Jolt entities and their owner chain
- the detail panel's velocity tracks a moving dynamic body, and reads `--` for a character

---

## Known costs — unmeasured

None of these is known to hurt. They are recorded so the next profiling pass starts from a list rather
than from scratch, and so nobody "optimises" one of them on a hunch:

- **One selection change = one full inactive-body pass.** `Set_HighlightedBody` re-arms the facility's
  revision-keyed pass so a static or long-asleep body gains its overlay on the very next capture
  instead of waiting for the scene to change. That pass is O(all bodies), and a DESELECT pays it too.
  Accepted as the correct-by-construction shape; measure before optimising it into an incremental one.
- **Collection and filtering are O(all rows) per refresh**, with no virtualisation beyond `SListView`'s
  own. Never profiled at a 100k-body world — profile the collector walk, the filter pass and the row
  reconcile together before changing any of them.
- **`TryPick_Body` is O(live instances) per click.** A click handler, not a tick; only a problem if a
  pick becomes perceptibly slow at scale.

---

## Warnings / anti-patterns

- **Never include `CkJolt_DebugDrawTarget_Impl.h`.** Public header only.
- **Never read `JPH::PhysicsSystem` from Slate** — and that includes going through a `UCk_Utils_Jolt*`
  helper that reads it for you, which is how a "harmless" velocity read got in once. If a live value
  isn't on the target's public surface, add it in CkJolt, sample it in the capture processor, and
  expose it JPH-free.
- **Never key a row or a lookup on a raw body key where a handle is available.** 0 is a valid body key,
  a baked actor's row carries only its FIRST body's key, and a re-bake changes every key it has.
- **Never declare a second `FName` for the tab id.** `SCkJoltDebuggerWindow::TabId` is the definition;
  cross-TU static init order would decide which one wins.
- **Never rebuild Slate structure in `Tick`.** The tree is built once in `Construct`; every value is a
  `TAttribute` lambda. See the rendering policy in `SCkDebugger_WindowBase`.
- **Never construct `SCheckBox`.** `SCkDebug_IconToggle` / `SCkDebug_IconToolbar`.
- **Never treat the CVar toggles as viewport controls** — see the warning above.
- **Named, filename-derived namespaces** (`ck_jolt_debugger`, `ck_jolt_debugger_3d_viewport`) — this
  module compiles unity and same-named anonymous-namespace helpers collide.
- **Demand must stay push+pull.** If you ever delete the `OnTabForegrounded` subscription, you
  reintroduce a per-frame capture into an invisible world that nothing will ever switch off.

---

## See also

- `CkJolt/CLAUDE.md` (CkFoundation) — the facility: target, capture processor, palette, bucket model.
  The contract this module renders.
- `CkDebuggerCommon/CLAUDE.md` — shared widgets, chrome and lane rules, row contracts, safety rules.
- `CkDebuggerLauncher/CLAUDE.md` — the descriptor census this module's tab id is enforced against.
