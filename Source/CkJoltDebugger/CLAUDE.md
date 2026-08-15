# CkJoltDebugger — the Jolt physics world, rendered somewhere you can actually look at it

> **Read [CkDebuggerCommon/CLAUDE.md](../CkDebuggerCommon/CLAUDE.md) first.** It covers the shared
> conventions this module obeys without restating them: window chrome and command lanes, the icon-toggle
> rule, list-row contracts, the PIE world lifecycle, and the safety rules. This file covers only
> CkJoltDebugger's own architecture.

## Identity (verified 2026-08-15)

**DeveloperTool** module — included in editor and packaged Development/DebugGame, excluded from
Test/Shipping. One nomad tab (`CkJoltDebugger`, label "CK Jolt Physics"), opened by the
`ck.JoltDebugger` console command (`[0/1]`, or bare to toggle) or from the shared launcher
(**Systems** category, slot 30, `Cube` glyph). The window is a preview-world **viewport** as the main
pane with world-level stats as its right-hand rail.

**Depends on:** `CkCore`, `CkEcs`, `CkJolt`, `CkDebuggerCommon`, `CkEditorTools`, plus `RenderCore`,
`RHI`, `InputCore` and `UMG` for the viewport shell (`FSceneViewport` + `FUMGViewportClient`), and
`UnrealEd` / `WorkspaceMenuStructure` behind `Target.bBuildEditor`.

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

Orbit RMB · pan MMB · wheel zoom · RMB+wheel camera speed · WASD / QE / arrows flight (perspective
only) · **Home** = Frame All.

**Ortho framing backs the eye off along −view by the bounding-sphere radius plus a margin.** Sitting the
eye on the box centre — the shape inherited from the Crowd viewport — puts half the content behind the
near plane, which is invisible with wireframes and obvious the moment the bodies are solid.

**There is no Frame Selection.** Nothing in this window produces a selection yet, and a framing command
that always frames everything is a button that lies about what it does. It returns when a selection
source exists, alongside the outliner that produces one.

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

Also run `Ck.DebuggerLauncher` — its catalog spec asserts the exact tool census, so renaming this tab id
requires editing that spec in the same change. `Ck.Jolt.DebugDraw.*` (CkTests) covers the facility this
module consumes.

### What the specs cannot reach — `[EDITOR-VERIFY]` only

Headless specs construct widgets; they cannot render, and they cannot give a target real content
(content bounds come from captured ISM buckets, which need a live Jolt world). These remain manual:

- bodies actually render in the viewport, and match the in-world draw
- framing on real content — Frame All / the ortho presets landing on the bodies
- the ortho eye offset — solid bodies not clipped by the near plane in Top/Front/etc.
- demand goes off when the tab is backgrounded (click a sibling tab during PIE)
- the window survives PIE end without crashing, showing its last state

---

## Warnings / anti-patterns

- **Never include `CkJolt_DebugDrawTarget_Impl.h`.** Public header only.
- **Never read `JPH::PhysicsSystem` from Slate.** If a value isn't on the target's public surface, add
  it in CkJolt and expose it JPH-free — do not reach across.
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
