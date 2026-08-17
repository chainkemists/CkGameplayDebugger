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
the viewport shell (`FSceneViewport` + `FUMGViewportClient`), `DeveloperSettings` for the per-user
preferences, and `UnrealEd` / `WorkspaceMenuStructure` behind `Target.bBuildEditor`.

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

`SCkJoltDebugger_3dViewport` (`Public/CkJoltDebugger/Viewport/`) is a thin presentation facade over Common's
runtime-safe `SCkDebug_3dPreviewViewport`. Common owns the `FPreviewScene`, `FSceneViewport`, private viewport
client, icon-first controls, camera math, input handling, and teardown; Jolt supplies the adapter, labels, and
specialized physics callbacks.

**The widget draws no 3D.** There is no `Draw` / PDI override. The facility's instanced static meshes are
registered into `Get_PreviewWorld()` and render because they are *in* that world — and so does the ground
grid, which is not drawn here either but pushed into the facility's External channel by the WINDOW (below).
If you find yourself adding a PDI call here, the thing you want to draw belongs in CkJolt. The ONE thing
this widget does draw is 2D — the label overlay in `OnPaint`, below.

**The world-axis gizmo is a WIDGET, not a paint** (P8-D59, ruled to fork (b) by P5-D61/S7). `SCkDebug_OrientationCube`
sits in the viewport's own `ChildSlot` — `SViewport` is a compound widget whose child renders OVER the scene
texture — bottom-left, `HitTestInvisible` so it is not a dead zone in the pick surface. Its `Rotation` is a live
`TAttribute`, so it is bound to the client's view rotation rather than pushed each tick, and the widget is
volatile enough to repaint without any invalidation plumbing.
⚠ It is fed the **INVERSE** of the view rotation: the cube shows an object's orientation under a fixed
three-quarter camera, and what a world-axis gizmo shows is the WORLD's orientation under a moving one — turning
the camera left has to swing the axes right. The cube's own fixed camera offset stays, so the gizmo reads as a
three-quarter view of the world axes rather than as a screen-aligned triad. There is no hand-drawn gizmo.

The scene is physics-free but uses `FPreviewScene`'s directional light plus a movable skylight. Advanced
features, lighting and post processing are explicit, and the viewport forces temporal anti-aliasing locally even
when the host project disables AA; motion blur, depth of field, eye adaptation and dynamic shadows stay off because
they obscure an inspection surface. Every Jolt ISM also has
`SetCastShadow(false)`, so the light supplies form without hiding features. A preview world that simulated would
fight the world being inspected.

### Labels and hover (P8-D58)

The only two things this widget renders itself, and both exist because the facility cannot: JPH hands out
labels as WORLD positions and renders no text, and the hover overlay needs a screen ray only the viewport
client has.

**Labels — `OnPaint`.**

- The projection is built from **the same view inputs `GetCursorWorldRay` deprojects**: the same
  `FSceneViewInitOptions`, the same `FMinimalViewInfo::CalculateProjectionMatrixGivenView`, composed into
  one world→clip matrix (`TryGet_ViewProjection` on the client), then `FSceneView::ProjectWorldToScreen`.
  A projection derived any other way would put labels where clicks do not land. **Do not try to capture an
  `FSceneView` from Slate.**
- Screen pixels become local Slate units **by ratio** (`LocalSize / ViewRect.Size()`), because the two are
  the same rectangle at different DPI scales.
- ⚠ **`OnPaint` allocates nothing OF ITS OWN in the steady state** — which is not the same as allocating
  nothing, and this doc claimed the stronger thing until P8-D74/F3. The font is a member built once in
  `Construct` (`CkDebuggerCommon/CLAUDE.md` § OnPaint), colours come off the label and the palette, and the
  label selection writes into a **member scratch array** that keeps its capacity across paints. What still
  allocates is Slate's own `FSlateDrawElement::MakeText`, which copies the text into the element list exactly
  as it does for every other text-drawing widget here. Consequence of the member font: it does **not** follow
  a live Style Lab text-scale flip — it is fixed for the life of the window.
- **The primary selection is always labelled**, whatever the `Labels` draw flag says. That label is the
  WINDOW's (`Set_PrimaryLabel`, pushed from the ungated half of `Tick`, positioned at the top of the
  highlight bounds in the palette's highlight colour) — a selection the user made is not something they
  should have to turn a flag on to name. The facility's own `Get_Labels()` are drawn on top of it, and
  today the only one it emits is the numeric mass beside the Mass+Inertia box, which needs BOTH `Labels`
  and `MassAndInertia`.
- **Hard cap: 500, nearest to the eye first** (`ck_jolt_debugger_viewport::MaxPaintedLabels`). Over the cap
  the selection is **bounded, never a full sort** (P8-D74/F3): a max-heap of exactly the cap, farthest at the
  top, so a candidate is compared against the worst kept label and the whole pass is O(n log cap) with one
  O(cap log cap) ordering pass at the end. Sorting all of them to discard everything past 500 is what a
  100k-body Labels flag would otherwise have paid, every paint. **Under** the cap the capture's own order is
  left untouched, so a camera move does not reshuffle the draw order for nothing. The truncation logs
  `Display` **once per window**, because a silent truncation reads as the facility losing labels.
- `ProjectWorldToScreen` returns false behind the eye, so a label the camera turned away from is dropped
  rather than wrapped to the other side of the screen.

**Hover.**

- Driven from `FViewportClient::MouseMove` — the NON-captured move. `FSceneViewport` routes a move with no
  button held there and one with a button held to `CapturedMouseMove`, which is exactly the split hover
  wants.
- **Throttled to ≥ 60 ms.** A pick is O(live instances) and the mouse produces a move event per pixel;
  unthrottled this would be the most expensive thing the window does.
- **It never fights the pick or the drag gesture.** Any of `_PendingPickPress`, `_IsCtrlGesture`, or a held
  mouse button suppresses it outright, and `LostFocus` clears it (reporting an unset key) so an overlay can
  never latch on.
- **`MouseLeave` clears it too** (P8-D74/F6). The cursor leaving the viewport is not a move, so nothing else
  in the client ever hears about it, and `LostFocus` does not cover the common case — moving the mouse out
  of a viewport that still holds focus. Without the override the last hovered body kept its overlay and its
  tooltip while the mouse was somewhere else entirely.
- ⚠ **Known gap:** a hovered body whose row leaves the world does not clear the hover until the mouse moves.
  The key lives on the viewport CLIENT and its change-detection is what would latch, so clearing it needs a
  client-side path the window can call — deliberately out of scope for the P8-D74 fix-up, not an oversight.
- The delegate fires **on change only** — re-stamping the same key sixteen times a second would invalidate
  an overlay that never moved.
- The facility owns the overlay (`Set_HoveredBody`, its own always-visible half-alpha class); the WINDOW
  owns the name, because only the collector can turn a body key back into an entity. A drawn body the
  collector cannot attribute still highlights, with no tooltip — inventing a name from the key would read
  as an entity that does not exist.

### Camera

| Preset | Projection | Notes |
|---|---|---|
| Perspective | Perspective | default; the only mode with flight |
| Top / Bottom / Left / Right / Front / Back | Orthographic | fixed rotation, then frames content |
| Frame All | unchanged | frames `Get_ContentBounds()`; **Home** |
| Frame Selection | unchanged | frames `Get_HighlightedBodyBounds()`; **F**; inert with no selection |

**The camera is the Unreal editor's, gesture for gesture** (P7-D49). The viewport that this shell was copied
from — Crowd's — still orbits on RMB; that parity was broken deliberately on the Jolt side only, and the Crowd
file is not to be edited. The divergence is a documented follow-up for the Crowd campaign.

| Gesture | Perspective | Orthographic |
|---|---|---|
| RMB drag | look in place — the eye is fixed, the look-at moves | pan |
| MMB drag | pan (eye and look-at together) | pan |
| LMB drag (no Alt, no RMB) | vertical tracks along the view, horizontal yaws in place | inert |
| Alt + LMB drag | orbit about the look-at — the eye moves, the pivot does not | inert (no rotation, ever) |
| Alt + RMB drag | dolly | inert |
| Wheel | dolly along the view — **eye AND pivot** | ortho-width zoom |
| RMB + wheel | fly speed (shared per-user Common preference) | — |
| **Ctrl + 0…9** | store this camera pose in that slot | same |
| **bare 0…9** | recall that slot | same |
| WASD / QE / arrows | fly — **only while RMB is held** | — |
| **Home** | Frame All | Frame All |
| **bare F** | Frame Selection | Frame Selection |
| **Space / Enter** | toggle Jolt pause / single step | same |
| **bare I** | toggle Isolate | same |
| plain LMB click | pick (replaces the selection) | same |
| **Ctrl + LMB click** | pick and ADD to the selection | same |
| **Ctrl + LMB drag** | drag a dynamic body by a spring (authority worlds only) | same |
| **Ctrl + wheel (dragging)** | move the drag plane along the view | same |

**The look-at is real camera state, not a derived value.** `_LookAt` and `_OrbitDistance` are members, and every
path maintains `eye + forward × distance == look-at`. That invariant is what tells look-in-place from orbit at
all: one moves the pivot and leaves the eye, the other moves the eye and leaves the pivot, and a look-at
recomputed from the eye each time could express neither.

**Gesture state is TRACKED in the client, not polled off the viewport's key map.** A gesture is a sequence — the
button arrives on `InputKey`, the drag on `InputAxis`, which carries no modifier state of its own. Tracking it
also means the scheme can be driven with no viewport at all, which is the only way a headless spec can pin it
(`Input_Key` / `Input_MouseAxis` on the widget). The tracked state is refreshed from the viewport's own key map
whenever one is present, so a modifier already held when focus entered is still seen, and `LostFocus` clears it
so the next drag cannot open with a button this client still believes is down.

**An ortho preset refuses to rotate.** The presets are axis-locked views; rotating one turns it into an
arbitrary camera with no projection to go back to, so RMB and MMB both pan and Alt+LMB does nothing.

**`F` requires no modifiers.** `Ctrl+F`, `Alt+F`, `Shift+F` and `Cmd+F` fall through to whatever else
binds them; only an unmodified F frames the selection. Home is unchanged.

**The wheel dollies the WHOLE camera — eye and pivot both** (P7-D71/F6). Shrinking the orbit distance
instead walks the eye towards a pivot it can never pass, so the wheel stalls in front of whatever was last
framed and the user cannot fly through it. Moving both leaves `eye + forward × distance == look-at` intact
by construction, so the orbit distance stays exactly as sane as it was and orbit/framing are unaffected. The
step is a fraction of that distance, so one notch means the same thing on a rope link and on a streamed cell.
In ortho there is nothing to dolly towards and the wheel widens the frustum instead.

**Pan and dolly scale off how much WORLD is on screen, which in ortho is the ortho width** (P7-D71/F5) —
not the pivot distance, which in an orthographic camera says nothing about what a pixel covers. Scaled off
the pivot, one drag moved the same world distance at every zoom level, which is unusable at both ends of the
range.

**Camera bookmarks are `Ctrl+0…9` to store and a bare digit to recall** (P8-D59), both in the client's
`InputKey` beside every other hotkey. Ten DENSE slots: the digit IS the array index in
`UCkJoltDebuggerSettings::CameraBookmarks`, so a slot carries an `IsSet` marker — an untouched slot between
two real ones would otherwise recall a default-constructed pose and snap the camera to the origin. A pose is
eye + rotation + projection + ortho width, restored directly; the look-at is rebuilt from the restored
rotation, the way a preset does it.
⚠ **Bare digits are safe for the same reason bare Space, Enter, I and F are**: nothing reaches this client
unless the VIEWPORT holds keyboard focus, so a digit typed into a search box stays in the search box. Only
the top-row digits bind — a numpad digit is where a value gets typed. `Ctrl+Alt+3` and friends fall through:
storing takes Ctrl and nothing else.

**Ortho framing backs the eye off along −view by the bounding-sphere radius plus a margin.** Sitting the
eye on the box centre — the shape inherited from the Crowd viewport — puts half the content behind the
near plane, which is invisible with wireframes and obvious the moment the bodies are solid.

**Frame Selection frames the highlighted body regardless of class visibility; Frame All does not.**
That asymmetry is deliberate. `Get_ContentBounds()` excludes hidden classes because a camera must never
frame what the viewer cannot see, but the Highlight overlay is its own colour class with no toggle — a
selection is always visible, so framing it is always meaningful. The selection bounds come from the
body's NORMAL instance, so a row click frames on THAT click without waiting for the next capture.

### Click-picking

A plain left click deprojects the cursor and asks the facility for the nearest live instance:
`TryPick_BodyHit(Origin, Direction, …)`, an oriented-box test in instance space. Every click path here
goes through it — the hit point is free once the slab test has run, and the Ctrl gesture needs it — while
the hover, which wants a key and nothing else, keeps to the `TryPick_Body` wrapper. **The widget never raycasts
the physics world** — the preview scene has no physics scene, and the game world's belongs to a step
this module must not touch. A click on empty space clears the selection; a click that hits an instance
no row can be found for leaves the selection alone, because the click did hit something.

**Cursor deprojection's inverse view includes the camera world translation.** The split
`FSceneView::DeprojectScreenToWorld` overload expects both rotation and `ViewOrigin` in its inverse-view
matrix (`ViewRotationMatrix.GetTransposed() * FTranslationMatrix(ViewOrigin)`). Supplying only inverse
rotation makes the ray originate around world zero while the preview renders from the actual camera, so
both hover and click selection miss the same visible bodies.

**The pick resolves on RELEASE, not press, and only within ~4 px of where the button went down.** A
camera drag also opens with a button press, so a press alone cannot tell a click from the start of a
gesture — picking there means every orbit that happens to begin over a body re-selects it. The press
position is remembered, the release compares against it, and a right or middle button arriving in
between cancels the pending pick outright (that gesture became a camera drag after the fact). Losing
focus clears it too — `FSceneViewport` empties its key state on the way out, so a press that never gets
its release must not pair with the next release that arrives.

**Plain LMB is CONSUMED by the viewport and never falls through.** Both the press and the release
return handled, whether or not anything was picked, so nothing behind the viewport sees a left click.
That is what keeps a click on empty space a deliberate "clear the selection" rather than a stray event.

**Ctrl+LMB picks on the PRESS, not the release — and that asymmetry is load-bearing.** Ctrl+LMB is one
gesture with two jobs: it adds the body to the selection *and* it opens the drag, and the drag has to open
at the top of the gesture or the first mouse move has nowhere to pull from. The release then knows a Ctrl
gesture is finishing and does **not** pick a second time — a release-side pick would replace the
multi-selection the press just built with the one body under the cursor. A Ctrl+click on empty space adds
nothing rather than clearing what the user was assembling.

**One pick answers both jobs.** The press resolves through `TryPick_BodyHit`, which returns the body key
AND the world point where the ray entered its oriented bounds; the key goes to the selection and the point
goes to the drag as its grab point. Picking twice would be two O(live instances) walks for one answer.

**`Alt`+LMB arms no pick at all** (P7-D71/F7). Alt+LMB is the orbit gesture, and an orbit that begins and
ends over the same body would otherwise re-select it on release.

### Drag (P7-D54)

The viewport owns the cursor and the deprojection; the **window** owns the world, the subsystem and the
drag plane. The widget never touches either — it forwards a ray.

- **The drag OPENS on the PRESS, at the exact point the ray met the body** (P7-D70/i). `TryPick_BodyHit`
  hands back the hit point along with the key, so the grab point is the surface the user clicked and
  nothing has to wait a capture for `Get_BodySample()` to publish a bounds centre. The earlier
  open-on-first-move path was a workaround for a key-only pick and is gone — there is no window in which a
  drag is armed but not begun, and no `_IsDragBegun` beside `_DragBodyKey` to keep in agreement.
- **The arm is keyed on the PICKED body** (P7-D71/F4). It carries the key the press resolved, and the
  window refuses when it is unset or is not the primary the press just made — a Ctrl+click on empty space
  used to arm a drag on whatever was selected BEFORE, which grabbed a body the user never clicked.
- **The plane is camera-parallel through the grab point**, captured once at drag open. Ctrl+wheel slides
  it along its own normal, scaled by how far away it already is, so one notch means the same thing on a
  crate at arm's length and on one across a streamed cell.
- **A live drag OWNS the mouse** — every camera gesture is suppressed while it runs, or the plane the
  grab point was built from stops describing the same world.
- **Only DYNAMIC bodies arm the drag.** The facility drops anything else at Verbose; arming on a static
  body would eat the mouse and silently do nothing.
- **`LostFocus` ends a live drag.** `FSceneViewport` empties its key state on the way out, so a release
  that never arrives would leave the facility's spring attached to a body forever.
- **`HandleDragRelease` is IDEMPOTENT and is the single teardown path** (P7-D71/F3). It runs on the
  release, at the top of `HandleWorldChanged`, at the top of the destructor and at the top of a fresh arm.
  **It ends the drag on the subsystem the drag BEGAN on** — a `TWeakObjectPtr` captured at arm (P8-D73) —
  not on whatever the world selector currently points at. The world-change path calls it *after* the
  selector has already re-pointed, so asking the selector there answered with the NEW world's subsystem,
  which never had this drag: the old world's body stayed on its spring until `FJoltWorld` shutdown, and a
  live server↔client selector switch never reaches that shutdown at all. The `Get_IsDragging()` gate stays,
  against that captured subsystem, which is what keeps a second call a no-op; the weak ref is cleared on
  release. It also `Clear_External`s the drag line's channel, which is what stops a retained
  "JoltDebugger.Drag" line outliving the world it described (P7-D71/F2).
- **Authority only.** The window computes `GetNetMode() != NM_Client` and pushes it down each tick; on a
  client the gesture never opens and the toolbar's Drag chip is dark with a tooltip saying why.

---

## The outliner

`SCkJoltDebugger_OutlinerPanel` lists one row per Jolt-backing entity of the selected world, collected
by `FCkJoltDebugger_DataCollector` on the window's refresh-gated Tick. **Five** populations, one flat list:
`JoltBody`, `BakedStatic` (a JoltStaticActor attribution entity), `Sensor` (a CkSpatialQuery Probe),
`Character`, `Constraint`.

- **`Constraint` is the odd population and deliberately so (P8-D55).** A constraint entity draws NOTHING,
  so it has no colour class and no population toggle — the other four line up one-to-one with those. Its
  row `BodyKey` is **UNSET**, because a key there would collide with the body row that already owns it in
  the shared key → row lookup; what the row carries instead is `ConstraintBodyKeys` (the pair it joins,
  **A first**) plus `ConstraintType` and `IsBodyBWorldAnchor`. `NumBodies` is reused for the pair count.
  The collector reads the pair through `UCk_Utils_JoltConstraint_UE::Get_BodyA/Get_BodyB` and turns each
  into a key through the SAME `FFragment_JoltBody_Current` the JoltBody pass keys its own rows from, so a
  constraint's body key and that body's row key are the same number by construction.

- **Row identity is `(Handle, Population)`, not the handle alone.** One entity can back two populations
  at once — a JoltBody that also owns a Probe — and a handle-keyed map would collapse those into one
  row and churn the list every pass. Identity drives the `SListView` pointer-reuse contract
  (`CkDebuggerCommon/CLAUDE.md` §"List / tree rows"): items are mutated in place and
  `RequestListRefresh` only fires when the visible SET changes.
- **`BodyKey` is `TOptional<uint64>`.** A row with no drawn body behind it — a baked actor whose bodies
  were already removed — has none, and 0 is a VALID Jolt body key, so a sentinel would alias onto the
  first body ever created. Rows are looked up by handle wherever a lookup has the choice.
- **Rows are sorted by population, then display name, STABLY.** Stable because the collector emits each
  population in registry order, and two rows with the same name must not swap places between refreshes
  for no reason the user can see. The list refreshes when the visible SET changes **or when the ORDER
  does** — a set-only check would leave the view rendering its cached order after a rename moved a row.
- **Dual search**: the Filter query hides rows; the Highlight query dims the survivors. Both match what
  the row renders (name, population pill, detail text) plus the body key.
- **The selected row is pinned past the filter, dimmed.** Narrowing the query after selecting a row must
  not make that row vanish: the selection would be invisible while the detail panel beside it still
  showed its facts. The pin follows the selection and nothing else — clear the selection and the filter
  hides the row again. A pinned row renders muted, the same way a highlight non-match does
  (`Get_IsRowDimmed`). With a multi-selection, **every** selected row is pinned, not just the primary.
- **`ESelectionMode::Multi`, and the panel — not the view — is the selection STORE.** It owns
  `TArray<FRowIdentity> _SelectedIdentities` plus a `TOptional<FRowIdentity> _Primary`, and the view is
  driven from them rather than read back out of it. That inversion is forced by the engine:
  `SListView::Private_SignalSelectionChanged` hands `OnSelectionChanged` an **arbitrary** element of its
  selection `TSet` (the first iterator), *not* the row the user just clicked, so "primary = last clicked"
  cannot be read off the delegate parameter. It is derived from the set **delta** instead — whatever
  joined the selection is the new primary; if the primary was deselected, the last survivor takes over.
  The delegate's item is used only as a tie-break when a Shift-range added several at once.
- **Identities, not row pointers.** The filter pipeline replaces item pointers whenever a row leaves the
  visible set and comes back, so a pointer-keyed selection would silently lose members every refresh.
  After a refresh the model is re-stamped onto the view (Direct, under an apply guard) rather than read
  back from it.
- **A selected row whose entity leaves the world leaves the selection with it**, and if that row was the
  primary, the last survivor is promoted — a multi-selection with no primary has nothing to show in the
  detail panel and nothing for the facility to sample.
- **`_IsApplyingSelection` guards every programmatic stamp.** `ClearSelection` and `SetItemSelection` each
  signal a selection change back through `OnSelectionChanged`; letting that rewrite the model from the
  view's half-applied state is exactly how a multi-selection loses members mid-apply.
- **An external selector reveals its row.** `SelectByHandle` / `SelectByEntity` search the UNFILTERED
  set; if the match is currently filtered out they clear the FILTER query and then select it — the pin
  covers the row that IS selected, not the one about to be. A selection the user cannot see is
  indistinguishable from no selection, which would read as a broken "Open In". The Highlight query is
  left alone — it dims, it never hides.
- **The "Problems" chip is a SECOND filter stage, not a second query** (P8-D57). It stacks with the text
  filter (`Matches_Query && (NOT _ProblemsOnly || Get_HasProblem())`) and obeys the same pin rule — a
  selected row survives both, dimmed. The flags are pushed in by the collector, not computed here, so a
  body that stopped being broken stops being listed without anyone touching the chip. **Its COUNT is cached
  at `Refresh`** (P8-D74/F4) — the chip's text and its kind are two attribute lambdas asking every paint, and
  the flags can only move when the collector hands over a new set, so an O(rows) walk per paint was scanning
  the whole collection to render a number that could not have changed. The chip lives ABOVE
  the query boxes because it is a different KIND of filter — one the facility computed, not one the user
  typed — and its tooltip rides an enclosing `SBox` (`SCkDebug_Chip` does not apply the base `ToolTipText`
  argument, and Slate resolves a tooltip by walking UP the hovered path).
- Right-click copies the row text or the full entity handle (`SListView::OnContextMenuOpening`, never a
  click-trapping widget inside the row). Multi-select aware — every selected row, joined with `\n`.

---

## Selection model

The window owns the model in two halves: `TOptional<FCkJoltDebugger_BodySnapshot> _Selection` (the
**primary**) and `TArray<FCkJoltDebugger_BodySnapshot> _SelectionAll` (the whole set, **primary first**).
`DoApplySelectionSet` is the ONE writer; `DoApplySelection` is its single-selection convenience. Every
source funnels through it and every sink is driven from it.

**A constraint selection highlights the PAIR, not the row** (P8-D55). A constraint row has no body key,
so `DoApplySelectionSet` contributes its `ConstraintBodyKeys` instead — body A first, so the facility's
sample and the detail panel follow the constraint's own first body. Keys are `AddUnique`d, because
selecting a constraint AND one of its bodies must not highlight that body twice.

⚠ **Selecting a constraint turns on `ConstraintReferenceFrames` for the WHOLE WORLD.** Jolt's
`PhysicsSystem::DrawConstraintReferenceFrame` takes no constraint — the draw is all-or-nothing — so
"draws its reference frames" is implemented as arming that flag while a constraint is selected. Accepted
and documented (P5-D61/S8); do **not** fake a per-constraint framing. The flag is derived, never
remembered: `DoApplyConstraintReferenceFrames` recomputes it as
`persisted-user-intent OR a-constraint-is-selected`, so deselecting restores exactly what the user chose
and the state is never persisted behind their back. **`HandleWorldChanged` re-derives it too** (P8-D74/F1):
a world change made while a constraint was selected drops the selection, and without the re-derive the next
world drew everybody's reference frames under a flag nobody set. Visible consequence: while a constraint is selected
the Reference Frames toggle reads ON even if the user's own preference is off.

**The primary leads the set because the facility says so**: `Set_HighlightedBodies` samples the FIRST key
and asks only that body for its contacts. A set in list order would describe a body the user did not click.

| Source | Source enum | Path |
|---|---|---|
| Outliner row click (incl. Ctrl/Shift) | `Outliner` | `OnSelectionChanged` (non-`Direct` only), primary + set on the delegate |
| Outliner PRUNE (a row's entity left) | `OutlinerPrune` | the panel drops it; `DoSyncSelectionFromOutliner` re-derives the window's set from `Get_SelectedAll()` |
| Viewport click | `Viewport` | `TryPick_BodyHit` → key → row |
| Viewport **Ctrl**+click | `ViewportAdditive` | `TryPick_BodyHit` → key → row → `OutlinerPanel::Add_ToSelection` → read the set back |
| Contacts-list row click | `Viewport` | other body key → row (the same resolution a pick does) |
| Global `DebugSelectionSync` | `External` | `Resolve_ClosestLineageMatch` with `Is_JoltDebuggerEntity` |
| `FCkDebug_EntityTargetRoute` ("Open In") | `External` | `TargetEntity` |
| Game-viewport picker | — | broadcast + route, so it re-enters through the two above |

| Sink | Path |
|---|---|
| Facility highlight | `Set_HighlightedBodies(<every selected key, primary first>)` — an empty set clears it |
| Facility contacts demand | `Set_WantsSelectionContacts(_Selection.IsSet())` |
| Facility isolation | `DoApplyIsolation()` — re-pushed on every selection change while Isolate is on |
| Viewport framing / follow | `Set_SelectionBounds(Get_HighlightedBodyBounds())` (the UNION over the set) |
| Outliner rows | `SelectByHandle` under `FApplyGuard`, `ESelectInfo::Direct` |
| Detail panel | reads `_Selection` + `_SelectionFacts` through its two `Get…` attributes |
| `DebugSelectionSync::Broadcast` | **the PRIMARY only**, and only from user-originated sources |

**The outliner is the multi-selection store, and a Ctrl+click in the VIEWPORT routes through it.** It is
the same act as a Ctrl+click on the row; driving the panel and reading the set back is what keeps the two
from diverging. That is also why `ViewportAdditive` exists as its own source: it re-broadcasts like a
user-driven source, but the outliner is **not** re-stamped from it — a single-row stamp would collapse the
multi-selection the panel just built.

**The window's set follows the panel's PRUNE, and that is its own source** (P7-D71/F8). The panel drops a
selected row whose entity left the world and promotes a survivor; without `DoSyncSelectionFromOutliner`
right after `Refresh`, the window's `_SelectionAll` kept the dead members and every sink downstream of it —
the highlight, the isolation set, the bounds union — went on naming keys that draw nothing, which under
Isolate is a blank viewport under a lit toggle. `OutlinerPrune` re-stamps nothing (the panel already holds
the state), re-broadcasts nothing (nobody selected anything) and leaves a pending route target alone (it
runs on the very refresh that target is waiting for).

**Echo suppression is triple-guarded**: only the three user-driven sources re-broadcast; programmatic
selects use `ESelectInfo::Direct`, which `OnSelectionChanged` ignores; and `HandleGlobalSelectionSync`
drops anything tagged with this tab's own id. Only the PRIMARY is ever broadcast — the rest of the suite
is single-selection and a set has no meaning there.

### Isolate and Follow (P7-D53)

- **Isolate names the SELECTED KEYS** — including a constraint row's `ConstraintBodyKeys`, exactly as the
  highlight gathers them (P8-D74/F2); a constraint-only selection would otherwise gather nothing and CLEAR
  the isolation, so selecting a constraint under a lit Isolate turned isolation off. It is re-pushed from
  `DoApplySelectionSet` every time the keys change; the toolbar toggle and the bare `I` hotkey are the same
  call. An **empty** selection CLEARS the
  isolation rather than isolating nothing — a viewport that went blank under a lit toggle is
  indistinguishable from a broken draw.
- **Follow keeps the camera's OFFSET; it does not re-frame.** The viewport client tracks the selection
  bounds' centre and translates the eye and the look-at by its delta each tick — rotation, orbit distance
  and projection are untouched. Turning Follow on re-arms the anchor, so it never teleports the camera.
  ⚠ **What it tracks is the UNION of the highlighted bounds, not the primary** (P7-D71/F12b, amending
  P7-D53): the bounds pushed down are `Get_HighlightedBodyBounds()`, so a multi-selection is followed by
  the centre of everything the highlight is showing. Following the primary alone would drift the rest of a
  selection off screen while the toggle claimed to be following it.
- Both persist (`IsolateActive` / `FollowSelection`) and are restored as **state**: with nothing selected
  yet, the restore pushes no isolation set and the first selection arms it.

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
reading the window's live selection **and** its facility facts. Rows that do not apply to the selected
population read `--` rather than disappearing, so the panel's height never jumps as the selection moves.

**Two delegates, both by const reference.** `GetSelection` hands back the primary snapshot;
`GetSelectionFacts` hands back `FCkJoltDebugger_SelectionFacts` (body sample + character sample +
contacts). By-value delegates would copy a snapshot, two sample structs and a contacts array once per row
per paint.

**`FCkJoltDebugger_SelectionFacts` is deliberately NOT folded into the snapshot.** The snapshot is the
outliner's flat row, copied per row per collector pass; three facility structs on every one of them would
be paid for by every row to serve the one that is selected.

| Group | Rows | Source |
|---|---|---|
| (header) | Entity (`SCkDebug_EntityRef`), Population, Motion Type, Sleep State, Body Key | the snapshot |
| Motion | Linear Velocity, Angular Velocity, Mass, Motion Quality, Allows Sleeping | sample (velocity via the snapshot) |
| Material | Friction, Restitution, Gravity Factor | sample |
| Layers | Object Layer, Broad Phase Layer, Sensor | sample |
| Shape | Shape Type, Shape Sub Type, Shape Scale, World Bounds | sample |
| Misc | User Data, Source Actor, Baked Bodies | sample + snapshot |
| Character | Ground State, Character Velocity, Ground Normal, Ground Velocity, Up Vector, Ground Body | character sample |
| Contacts | one `SListView` row per touched body | `Get_SelectionContacts()` |

**Two values that would LIE if rendered raw.** `Mass` of 0 means **Infinite** — Jolt stores an inverse
mass, and a static or explicitly infinite-mass body has an inverse of 0; "0.00 kg" says the opposite.
`Allows Sleeping` is a `TOptional` and is **unset for a static body only** — Jolt dereferences
`MotionProperties` without a guard, so the facility never asks one.

**The character group is COLLAPSED for a rigid body, and it is the one group that disappears.** Six
permanent `--`s on the population the window shows most is worse than a group that comes and goes. Its
visibility is keyed on the SELECTION's population, not on whether a character sample happens to be
present — the sample lands one capture late, and a group that appeared a frame after the click would
shift every row below it exactly once per selection.

**The contacts list is `STextBlock`-only rows** (the click-trap rule — `CkDebuggerCommon/CLAUDE.md`
§"Don't put click-consuming widgets inside `STableRow`"), because clicking a contact row is the whole
point: it selects the other body through the window's own selection path. Rows keep `TSharedPtr` identity
across refreshes keyed on the other body's key, so a contact set refreshing under the user's click does
not destroy it. The list is **pushed** (`Refresh_Contacts`) rather than bound — an `SListView` renders
from its own item source. Contacts are a DEMAND on the facility: `Set_WantsSelectionContacts` is armed
exactly while something is selected, because the query behind it is a `NarrowPhaseQuery::CollideShape`
nothing should pay for while no consumer is showing the result.

**Linear velocity comes from the facility, never from `UCk_Utils_JoltBody_UE`.** The capture processor
samples the highlighted rigid body's velocity in the physics pipeline's async-safe window and publishes
it as `FCk_Jolt_DebugDrawTarget::Get_BodySample()`; the window copies that into the
selection facts. A Slate-side `Get_LinearVelocity` call would lock a body interface on a world whose
step may be in flight — intermittently wrong is worse than never. Consequences to expect in the UI:
the row reads `--` for a character (a `CharacterVirtual` has no rigid-body velocity), and for a
sleeping or static body once the scene-revision pass that drew it has passed, because the sample
belongs to a capture rather than to the selection.

---

## Stat rail

`FCkJoltDebugger_Stats` is refreshed on the gated Tick and read by `TAttribute` lambdas; nothing in the rail is
rebuilt. Sections: World, **Simulation**, Rigid Bodies, Characters, Static World, Viewport.

**The Simulation section has three different sources, and which one a row comes from is a correctness question,
not a preference.**

| Rows | Source | Why |
|---|---|---|
| Jolt Paused, Last Step, Contact Pairs | `UCk_Jolt_Subsystem` directly | the pause state is not on the target at all, and the other two are only pushed onto it while something is capturing |
| Active Rigid / Active Soft Bodies | `Get_WorldStats()` LIVE block | plain counters the capture reads every pass |
| Bodies, Body Budget, Static / Dynamic / Active Dynamic / Kinematic / Active Kinematic / Soft, Constraints | `Get_WorldStats()` SAMPLED block | `GetBodyStats()` walks every body and `GetConstraints()` copies the array |

**The sampled rows are LABELLED "(sampled)", and the label is part of the row's name, not its value.** They
refresh every 30th capture (`WorldStatsSampleInterval`, a constant, not a knob — P5-D61/S10), so a lagging count
is designed behaviour. A row with no label would read as a bug the first time the user added a body and the
number did not move. They read `--` until `_HasSample` is true, which distinguishes "no bodies" from "not asked
yet".

**Paused rides the SUMMARY line as an `SCkDebug_StatusPill`** (PAUSED / LIVE), not a row in the section: it is
the one piece of state that changes what every number below it means.

---

### Health badge (P8-D57)

The stats header carries an `SCkDebug_CountBadge` with the number of flagged rows, **collapsed when that
number is zero** — a badge reading "0" is a permanent alarm nobody can silence, and an absent one is the
good news. Its colour is `ck::debug_axes::Get_HeatColor(1.0f)`, never a hand-written hex.

The flags themselves come from the FACILITY, not from this module: `DoApplyProblemThresholds` pushes the
per-user runaway bar and the selected world's `AWorldSettings::KillZ` into
`FCk_Jolt_DebugDrawTarget::Set_ProblemThresholds`, and `FCkJoltDebugger_DataCollector::Apply_ProblemFlags`
stamps `Get_ProblemBodies()` onto the rows between the ECS pass and the outliner refresh. The collector
**never reads `JPH::PhysicsSystem`** — that is the module's founding rule, and a Slate-side physics read
would break it.

- Thresholds are re-pushed only on CHANGE. `Set_ProblemThresholds` drops the last verdict by design, so
  pushing an identical pair every refresh would blank the problem set between captures and flicker the chip.
- A **baked-static** row inherits the UNION of its own bodies' flags (through the collector's owner index):
  an actor with one broken body is a row the user has to be able to find.
- A **constraint** row inherits the flags of the pair it joins.
- With no inspectable world the thresholds are cleared, so nothing is scanned outside PIE.

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
| Primary | `JoltRender` | one cycling wireframe button (`Grid`): Off -> Transparent sensors only -> All; it swaps/overlays the facility's materials without rebuilding geometry — plus the ground-grid toggle (`Net`) |
| Primary | `JoltSim` | Pause toggle + Step button; both disabled with a tooltip when no world has begun play |
| Primary | `JoltSelection` | Isolate toggle (`Lock`, also `I`), Follow toggle (`Target`), and the Drag STATE chip (`Hand`) |
| Context | `JoltTarget` | `SCkDebug_WorldSelector` |
| Context | `JoltCamera` | 8 icon buttons (7 presets + Frame All) |
| Context | `JoltDraw` | one `SCkDebug_IconToolbar` per draw-flag group (Bodies / Constraints / Contacts / Labels) + the colour-mode `SSegmentedControl` |
| Context | `JoltInWorldDraw` | all six in-world CVars — `.Enabled` (master) gating `.SleepColoring` / `.Velocity` / `.WorldTransform` / `.Constraints` / `.Contacts` |
| Context | `JoltPopulations` | four class-group toggles, disabled outside BodyClass colour mode |
| Context | `JoltLegend` | one swatch + label per class of the CURRENT colour mode, from `Get_LegendEntries` |

### Draw lane and colour mode

**`JoltSim` and `JoltSelection` are Primary while `JoltDraw` is Context, and the split is the lane doctrine,
not a layout accident.** Pause, Step, Isolate and Follow act on the world (or the selection) the pane above is
showing; the draw flags decide what that pane emits, which is a property of the target rather than an action on
the viewport.

**The Drag chip is STATE, not a control.** It is always disabled and merely lit while the selected world is the
authority; the gesture is Ctrl+LMB in the viewport. Its explanation rides the enabled `SBox` wrapped around it,
because a disabled widget shows no tooltip of its own — and a Ctrl+LMB that silently did nothing on a client
would read as a broken debugger rather than as a refused sim mutation.

Each toggle writes the facility (`Set_DrawFlags` / `Set_ColorMode`) **and** the preference, in the toggle's own
handler, then `SaveConfig()`. Draw flags persist as the RAW bitmask (`UCkJoltDebuggerSettings::DrawFlags`)
rather than as one property per flag: a per-flag property set would rename its own config keys every time the
facility gained a flag, and silently drop the user's choice on every one of them.

**The legend is the one surface here that is REBUILT rather than attribute-bound.** A colour mode does not
merely recolour classes — it changes how many there are and what they are called, so `DoRebuildLegend()`
repopulates the lane from `Get_LegendEntries(Get_ColorMode())`. It runs from the mode control's own handler and
from the restore pass, never from `Tick`. The swatch COLOUR stays bound, because the palette can move under a
fixed set of classes (a Style Lab flip, `Set_Palette`) with no rebuild needed.

**The population toggles are a BodyClass mask, so they are disabled in every other mode.** The facility's
visibility mask is indexed by the CURRENT mode's class indices, and `Set_ColorMode` clears it outright — index 5
is `BakedStatic` in one mode and `Cylinder` in another. Two consequences the window owns: the restore pass sets
the colour mode BEFORE the population visibility (the reverse order would wipe what it just restored), and
`Set_ColorMode` re-applies the saved visibility whenever the mode returns to BodyClass, or the toggles would
read "everything visible" while the preferences said otherwise.

**"Probe results" is in the Draw lane but is NOT a facility draw flag** (P8-D56). From the user's side
"show me what the probe is touching" is the same kind of question as "show me velocities", so it sits in
the same lane; underneath it is a **retained named External sub-channel**, `"JoltDebugger.ProbeResults"`
(P5-D61/S3), owned by this window:

- Pushed only when the DRAWING changes. `UCk_Utils_Probe_UE::Get_CurrentOverlaps` returns a full `TSet`
  **copy**, so the result is gathered, digested into a signature, and only re-drawn when that signature
  moves. The signature covers everything the lines are built from — the overlap identities, the contact
  points, **the origin the lines leave from and each other entity's location** (P8-D74/F5). An
  identity-only digest froze the drawing in place for the commonest case there is: a probe resting against
  a moving body, whose overlap membership never changes. **Never poll it for every probe** — it is read for
  the primary selection alone.
- Drawn per overlap: a line to the other entity, a small sphere at each contact point, and an arrow along
  the contact normal from each point.
- `Clear_External("JoltDebugger.ProbeResults")` is the ONLY thing that empties it, and the window calls it
  when the selection leaves a probe and when the world changes. The capture re-emits a retained channel
  every pass without clearing it, which is what stops it flickering.
- ⚠ **A persistent probe TRACE has no hit positions at all** — `ck::FFragment_ProbeTrace_WorldContacts`
  holds a `TSet<FCk_Handle>` and an anonymous-contact bool, nothing more — so a trace selection can only
  draw the entity lines. **The tooltip says so**, rather than the user seeing points on some selections
  and not others and drawing their own conclusions.
- ⚠ **A ProbeTrace entity is not one of this window's five populations today**, so that branch only fires
  for an entity carrying the world-contacts fragment ALONGSIDE something listed (a Probe). Adding a
  ProbeTrace population was out of Phase 8's scope; the branch is written and guarded on the fragment.

### The ground grid (P8-D59)

A metre lattice at Z=0 — 1 m cells over a 20 m half-extent, a heavier line every 10, and the world axes
through the origin — pushed **ONCE** into the retained named External sub-channel `"JoltDebugger.Grid"`
(P5-D61/S3). The capture re-emits a retained channel every pass without clearing it, so a grid that never
moves costs exactly one push for the life of the window; the toggle either pushes it or `Clear_External`s
the channel, and nothing about it runs on `Tick`.

- **Colours come off the shared style, never a hex.** The two lines through the origin ARE the world axes
  and are drawn in `CkStyle::AxisX()` / `AxisY()`, so the grid doubles as the ground truth the orientation
  gizmo is claiming; majors are `CkStyle::TextMute()` and minors the same colour dimmed. Dimming is a
  MULTIPLY rather than an alpha — the lines go through Jolt's own colour path, which is not a translucency
  budget this window owns.
- **41 lines per axis, both axes: 82.** The shape is a set of constants, not a knob — "1 m cell" has to
  keep meaning the same thing to the eye reading it. `Get_NumGridLines()` is the window's read surface for
  the count, which is how a headless spec pins that the toggle pushed anything at all.
- ⚠ **The grid is not part of the physics world.** It is debugger-side line work in a debug-draw channel;
  nothing collides with it, and it is invisible to the game viewport.

**Contact flags are process-wide** and every contacts tooltip says so — Jolt's contact draw switches are plain
statics with no per-system variant, so arming them here arms contact emission for every world and every other
debugger preview at once (`CkJolt/Claude.md` § Contact recording).

### ⚠ In-world draw is NOT this viewport

**The single most confusing thing about this window.** The six CVar toggles in `JoltInWorldDraw` gate
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

## Per-user preferences

`UCkJoltDebuggerSettings` (`Public/CkJoltDebugger/Settings/`) — `Config=GameUserSettings`,
`GetContainerName() == "Editor"`, category `CkGameplayDebugger`. Per-user, never committed, available in
packaged developer tools, presented under **Editor Preferences → CkGameplayDebugger → Ck Jolt Debugger**.
This is the plugin-wide settings split, not a local choice: see `CkGameplayDebugger/CLAUDE.md`
§ "Settings split", precedent `UCkCrowdDebuggerSettings`.

| Preference | Restored into |
|---|---|
| `RenderMode` (Off / Transparent Only / All) | `Set_RenderMode` on the target; stored by reflected enum name so the old Solid/Wireframe preferences still restore as Off/All |
| `ShowJoltBodies` / `ShowBakedStaticWorld` / `ShowSensors` / `ShowCharacters` | `Set_ClassVisibility` per colour class — **only while the mode is BodyClass** |
| `CameraPreset` (the 7 orientations) | `ApplyPreset` on the viewport |
| `DrawFlags` (raw `ECk_Jolt_DebugDrawFlags` bits) | `Set_DrawFlags` on the target |
| `ColorMode` (`ECkJoltDebugger_ColorModePref`) | `Set_ColorMode`, then the legend rebuild |
| `ShowProbeResults` | `_ShowProbeResults`, then `DoUpdateProbeResults` — the External sub-channel, not a draw flag |
| `RunawayVelocityCmS` | half of `Set_ProblemThresholds` on the target; the other half is the world's `KillZ` |
| `IsolateActive` | `_IsolateActive` + `DoApplyIsolation()` — state, not an act; the first selection arms it |
| `FollowSelection` | `Set_FollowSelection` on the viewport |
| `ShowGrid` | `_ShowGrid`, then `DoApplyGrid()` — one push into the retained grid channel, or a clear |
| `CameraBookmarks` | nothing at restore: the slots are read by the viewport client's own `0…9` recall, on demand |

**A camera bookmark is stored by the VIEWPORT, not by the window.** `Ctrl+0…9` writes
`UCkJoltDebuggerSettings::CameraBookmarks[slot]` + `SaveConfig()` straight from the viewport client's
`InputKey`, which is the same "write the moment the user flips it" rule every toggle here follows — the
window is not in the loop because the pose is the client's own state and nothing else consumes it. The
array is DENSE (ten slots, digit = index) and each slot carries `IsSet`, so an untouched slot is inert
rather than a pose at the origin.

**The bool preferences carry no `b` prefix** — the house rule and the four population toggles beside them both
say `ShowSensors`, not `bShowSensors`. PHASE_7 spells three of them `bIsolateActive` / `bFollowSelection` /
`bShowGrid`; the fields are `IsolateActive` / `FollowSelection` / `ShowGrid`.

**Written the moment the user flips a control** (`GetMutableDefault<>` + `SaveConfig()` inside the
toggle's own handler), **read once at the end of `Construct`** (`DoApplySavedPreferences`). Restoring
after the widget tree is built, not before, is load-bearing: every toggle reads its state back off the
target, so a restore that ran first would be overwritten by nothing and simply not show.

**One table defines a population toggle** — `ck_jolt_debugger::Get_PopulationGroups()` carries the icon,
label, tooltip, colour classes AND the `bool UCkJoltDebuggerSettings::*` it persists into. Both the
toolbar builder and the restore pass read that table, so a toggle and its saved value cannot come to
describe different colour classes. Add a population by adding a row.

**Framing presets are deliberately not persisted.** Frame All and Frame Selection are actions against
whatever is in the world at that moment, not a camera state a window can be restored into.

---

## Tests

`Private/Tests/*.spec.cpp`, whole file inside `#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS`,
`IMPLEMENT_SIMPLE_AUTOMATION_TEST`, `EditorContext | EngineFilter`.

| Spec | Asserts |
|---|---|
| `Ck.JoltDebugger.Window.ConstructsWithoutSlotAttributeEnsure` | the window builds and prepasses with no slot-attribute ensure |
| `Ck.JoltDebugger.Window.ConstraintIsADebuggerEntity` | the FIFTH clause of `Is_JoltDebuggerEntity` (P8-D55, pinned by P8-D74/F10): an entity carrying `FFragment_JoltConstraint_Current` answers TRUE, a bare entity FALSE, an invalid handle FALSE. That one predicate answers for both the entity-target route and the game-viewport picker's filter, and the four body-ish clauses cannot cover it — a constraint entity carries none of them |
| `Ck.JoltDebugger.Viewport.ConstructsWithoutEnsure` | the viewport builds ensure-free, owns a preview world, enables advanced lighting/post-processing/TAA, supplies directional + sky light, and keeps shadows, motion blur, depth of field, and eye adaptation disabled |
| `Ck.JoltDebugger.Viewport.CursorRayUsesCameraTranslation` | the inverse view used for hover, click, and drag deprojection maps view-space origin back to the camera's world location, pinning the translation term that keeps cursor rays aligned with rendered bodies |
| `Ck.JoltDebugger.Viewport.CameraPresets` | every preset sets its projection mode and points the camera down its expected axis (compared as forward vectors — rotator normalization must not make it flaky) |
| `Ck.JoltDebugger.Viewport.FrameAllWithoutContentIsInert` | framing invalid bounds leaves the camera untouched instead of snapping to the origin |
| `Ck.JoltDebugger.Viewport.CameraSchemeIsUnrealStyle` | the DISCRIMINATING facts of P7-D49 + P7-D71, driven through the client's own input entry points: RMB-drag leaves the eye exactly where it was while turning the camera; Alt+LMB-drag moves the eye while the look-at pivot stays put (the reverse of the first, which is what tells orbit from look-in-place); MMB-drag moves the eye with the rotation untouched; **the wheel moves the eye AND the pivot, leaving the distance between them untouched** (F6 — a wheel that shrank the orbit distance would stall at the pivot); **the same ortho pan covers more world zoomed out than zoomed in** (F5 — scaled off the ortho width, not the pivot distance); and an ortho preset refuses both a rotate gesture and an orbit gesture |
| `Ck.JoltDebugger.Outliner.ConstructsWithoutEnsure` | the panel builds, reconciles an EMPTY pass and a `Clear` without an ensure, and resolves an absent handle to nothing |
| `Ck.JoltDebugger.Outliner.MultiSelectKeepsPrimaryAndSurvivesRefresh` | the multi-select contract on three real entities: a plain select takes one row, `Add_ToSelection` (the Ctrl+click path, from the outliner OR the viewport) makes two with the LAST as primary, the set is reported primary-FIRST (which is what the facility samples), a refresh over the same entities keeps both rows AND the same primary, a filter matching neither of them hides neither — both stay pinned and dimmed beside the one match — a selected row whose entity leaves the world leaves the selection and promotes the survivor to primary, and clearing drops the set with its primary. Then the same contract again through the CLICK path (`Simulate_RowClick` → `SListView::SetItemSelection(…, ESelectInfo::OnMouseClick)`), which is the only thing that exercises the set-DELTA derivation the engine forces: a click takes one row and makes it primary, a Ctrl+click adds and re-primaries, a Shift RANGE arriving as one signal leaves a primary that is one of the rows it added, and a Ctrl+click REMOVING the primary promotes a survivor |
| `Ck.JoltDebugger.Outliner.RowsSelectFilterAndSurviveRefresh` | three rows on three real entities (a standalone `ck::FEcsWorld`): select-by-handle finds the right row, a refresh over the same entities KEEPS the selection (the pointer-reuse contract), the filter hides non-matches EXCEPT the selected row, which stays pinned and dimmed while the matching row is not, the pin disappears with the selection, and an external select reaches a filtered-out row and reveals every row again |
| `Ck.JoltDebugger.Outliner.ListsConstraintRows` | a constraint row is listed beside the two bodies it joins and is selectable; it carries NO body key of its own; it names BOTH bodies with **A first** (which is what the facility samples) and reports its constraint flavour; a refresh over the same set keeps it selected; and its own row text answers the shared text filter |
| `Ck.JoltDebugger.Outliner.ProblemsChipNarrowsToFlaggedRows` | three rows, one flagged: the chip is off to begin with and all three are listed; turning it on leaves exactly the flagged row; a SELECTED healthy row stays pinned beside it and renders dimmed (the pin rule outranks the chip exactly as it outranks the text filter); clearing the selection narrows to one again; clearing the chip restores three; and a row whose flags are cleared by the next collector pass leaves the chip with nothing to show |
| `Ck.JoltDebugger.Viewport.LabelCapKeepsTheNearest` | the pure half of the label paint: under the cap every label survives in the CAPTURE's order (a camera move must not reshuffle the draw order); over it exactly the cap survives, **nearest first**, from a fixture authored farthest-first so a take-the-first-N implementation cannot pass; a zero cap paints nothing; no labels select nothing; and the shipped cap is 500 |
| `Ck.JoltDebugger.Settings.ConstructRestoresPreferences` | the settings live in the `Editor` container, and the window constructs ensure-free with NON-DEFAULT preferences — which is what drives the restore path, camera preset included. Every preference the restore reads is asserted UNCHANGED afterwards, so a restore that quietly re-saved a default over the developer's own choice surfaces here rather than in an ini. Covers draw flags, colour/render modes, isolate, follow, grid, probe results, direction-glyph scale, runaway velocity, and a bookmark. **And that the restore LANDED** (P7-D71/F10): the window's own read surface reports the modes and scale ON THE FACILITY TARGET, its isolate/follow/grid state, and that a grid restored OFF pushes no lines while a second window with it ON pushes exactly 82. The developer's own CDO values are saved and put back by an RAII guard **which then calls `SaveConfig()`** (P8-D74/F7) — every toggle these specs drive saves as it goes, so restoring the CDO alone left the test values, a slot-3 bookmark among them, sitting in the developer's real ini. The guard covers `ShowProbeResults` as well |
| `Ck.JoltDebugger.Settings.RenderModeCyclesAndPersists` | the toolbar's single wireframe button cycles Off → Transparent Only → All → Off, pushes each mode to the facility target, and persists the matching preference |
| `Ck.JoltDebugger.Settings.CameraBookmarksStoreAndRecall` | bookmarks end to end through the REAL hotkeys (P8-D59): `Ctrl+3` stores the live pose in slot 3 and marks it set; moving to a different projection AND a different eye and pressing a bare `3` puts the projection, the eye and the rotation back; a slot nobody stored is inert rather than a snap to the origin; and `Ctrl+Alt+3` does not store, so a modified digit stays available to whatever else binds it. Under the same RAII guard |
| `Ck.JoltDebugger.Detail.ConstructsWithoutEnsure` | every value lambda survives a completely unbound `GetSelection` AND an unbound `GetSelectionFacts` — including the rows that dereference a `TOptional` sample — and reads `--`; `Refresh_Contacts` on an unbound panel lists nothing |
| `Ck.JoltDebugger.Detail.RowsReflectTheSelection` | the built rows render the bound selection's own values and semantic presentation: numeric/math/enum roles use the shared value palette, unset values mute, population/simulation/sensor/character-ground states use status tones and dots; every facility row still degrades to `--` while its sample is unset; zero mass reads "Infinite"; the character group flips live with Character selection; contacts reconcile one row per reported contact and clear with the selection |

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
(content bounds come from captured ISM buckets, which need a live Jolt world). **`OnPaint` and the hover
throttle are in that set**: a paint needs a live `FSceneViewport` with a non-zero rect and hover needs
real mouse-move delivery, neither of which an automation run has. The projection under the paint is engine
math with no branch of ours in it, so what IS pinned headlessly is the pure half — which labels survive the
cap and in what order (`Ck.JoltDebugger.Viewport.LabelCapKeepsTheNearest`). These remain manual:

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
- **preferences persist across an editor restart** — all three render modes, the four population toggles, and the
  camera orientation all come back as they were left
- **a selected row stays visible, dimmed, when a filter typed afterwards excludes it**
- **`Ctrl+F` / `Alt+F` do NOT frame; a drag-then-release does NOT pick; a click does**
- **the camera gestures, in a live viewport** — RMB-drag looks around without moving the eye; WASD/QE fly ONLY
  while RMB is held; RMB+wheel changes fly speed; MMB-drag pans; wheel dollies; Alt+LMB orbits the current
  look-at; Alt+RMB dollies; in an ortho preset RMB and MMB pan, wheel zooms, and the view never rotates. The
  SIGN of the plain LMB-drag track (drag down = forward) is manual too — a spec can assert that the eye moved
  along the view, not which way feels right
- **every Draw-lane toggle visibly changes the viewport** (velocity arrows, AABBs, COM axes, constraints,
  contacts, labels) and survives an editor restart
- **switching colour mode recolours the bodies and the legend follows**, the population toggles grey out
  outside Class mode, and returning to Class restores the toggles the user had set
- **Pause freezes the sim (in-world too), Step advances exactly one step**, Space/Enter work with the viewport
  focused, and the stat rail shows the PAUSED pill, the step ms, and a body breakdown that visibly lags by up
  to 30 captures under its "(sampled)" labels
- a baked-static click on a NON-first body of an actor selects that actor's row
- **Ctrl+click in the outliner and in the viewport builds a multi-selection**, all of it highlighted, and
  the detail panel follows the LAST body clicked
- **Isolate hides everything else**, `I` toggles it, and it re-applies as the selection changes; with
  nothing selected it is inert rather than blanking the viewport
- **Follow keeps the camera on a moving body** without re-framing it — rotation and distance stay put
- **Ctrl+LMB drag on a dynamic body pulls it around with a visible yellow drag line**; Ctrl+wheel pushes
  the plane away and pulls it back; release drops it and the line disappears; dragging a static or
  kinematic body does nothing
- **the drag grabs the body AT THE POINT CLICKED and starts on the press** — click a corner and the body
  hangs from that corner, not from its centre, and the very first mouse move already pulls it
- **Ctrl+LMB on EMPTY SPACE opens no drag** — the previously selected body does not jump to the cursor
- **switching the world selector mid-drag drops the body and takes the drag line with it**; closing the
  tab mid-drag does the same
- **the wheel keeps flying forward past whatever was framed** instead of stalling in front of it, and an
  ortho pan drags proportionally at every zoom level
- **the ground grid gives the empty preview world a sense of scale** — 1 m cells, a heavier line every
  10 m, red/green axes through the origin — the toggle takes it away and brings it back, and it survives
  an editor restart
- **the world-axis gizmo in the bottom-left points the right way in every camera preset** and turns with
  the camera; clicking through it still picks the body underneath
- **`Ctrl+3`, move the camera, then `3` returns it exactly** — including the projection, so a bookmark
  taken in Top comes back orthographic; an unused digit does nothing; digits typed into the outliner's
  search box do not move the camera
- **On a PIE client world the Drag chip is dark with an explanatory tooltip and Ctrl+LMB does nothing**
  (it still adds to the selection — only the drag is refused). ⚠ The tooltip now binds on the
  `SCkDebug_IconToggle` itself rather than on an `SBox` around it (P7-D71/F11) — confirm the AUTHORITY-aware
  text is what appears on hover, not the toggle's own generic one
- **the contacts list fills for a resting body and its rows select the other body on click**; a character
  selection lists none
- **constraints appear in the outliner with their type; selecting one highlights BOTH bodies** and turns
  the world's constraint reference frames on for as long as the selection holds; "Open In → Jolt" from the
  ECS debugger reaches a constraint row
- **a rope built by `UCk_Utils_JoltRope_UE` lists its constraints** and they all highlight sensibly
- **selecting an overlapping probe with "Probe results" on draws contact points, normals and a line to
  each overlapping entity**; the lines do not flicker between captures and disappear when the selection
  leaves the probe; the tooltip explains why a persistent trace has no points
- **throwing a body far below KillZ, or making one a runaway, lights the header badge and the Problems
  chip** and narrows the list to it; slowing it down clears both
- **labels appear on the selection with no draw flag set**, and turning the Labels flag on labels bodies up
  to the cap without tanking the frame rate (the cap logs once)
- **hovering a body highlights it subtly and shows its name**; the hover never fires mid-click or mid-drag,
  and leaving the viewport clears it
- `[PACKAGED-VERIFY]` all three engine debug materials render in a packaged Development build — exact
  acceptance steps in `CkJolt/Claude.md` § "Colour + wireframe"

---

## Known costs

- **The health scan is one extra O(active) walk per capture, and only while armed.** It is a separate
  capture step rather than a hook in `Draw_Body` (the incremental pass would have made it lie), and the
  window disarms it whenever no world is inspectable.
- **The probe-results channel costs one full `TSet` copy per REFRESH of the selected probe**, not per
  tick, and nothing at all while the toggle is off or the selection is not a probe.
- **`OnPaint` costs one matrix build plus one `ProjectWorldToScreen` per painted label**, hard-capped at
  500, plus the bounded label selection over ALL of the capture's labels — one distance-squared per label
  and a heap sift only for the ones that beat the current worst (O(n log cap), P8-D74/F3). With no labels
  and no selection it early-outs before building the matrix at all. The selection's index array is a member
  reused across paints, so the steady state does not grow an allocation; the text copies Slate's `MakeText`
  makes are the only per-paint allocation left, and they are the same ones every text widget in the suite
  pays.

- **One selection change = one full inactive-body WALK — CLOSED, measured.** `Set_HighlightedBody`
  re-arms the facility's revision-keyed pass so a static or long-asleep body gains its overlay on the
  very next capture. That pass became incremental in Phase 4, so it now draws only the newly selected
  body: **23.6 ms at 100k bodies, down from 249.9 ms** (`Ck.Jolt.DebugDraw.Benchmark.ScaleMatrix`, full
  table in `CkJolt/Claude.md`). What remains is the walk itself. Accepted.
- **`TryPick_BodyHit` is O(live instances) per click — measured, accepted.** 14.0 ms at 100k instances
  (measured as `TryPick_Body`, which is now the same walk). A click handler, not a tick; a seventh of a
  frame once per click is not a problem worth structure. A Ctrl+press pays it **once** for both the
  selection and the drag's grab point, and the hover pays it at most every 60 ms.
- **The grid is 82 retained lines, pushed once.** They ride the capture's line component from then on;
  the per-frame cost is emitting them, and nothing re-pushes.
- **Collection and filtering are O(all rows) per refresh — STILL UNMEASURED**, with no virtualisation
  beyond `SListView`'s own, and the sort adds an O(n log n) pass on top. The facility benchmark covers
  the CkJolt side only; a 100k-body world's collector walk, filter pass, sort and row reconcile have
  never been profiled together. Profile them as one before changing any of them.

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
- **Never read the "row the user clicked" off `OnSelectionChanged`'s item parameter.** `SListView` hands
  back an arbitrary element of its selection `TSet`. The primary comes from the set DELTA.
- **Never key a multi-selection on row POINTERS.** The filter pipeline replaces them; key on
  `FRowIdentity` and re-stamp the view.
- **Never push a single-row select back into the outliner from a source that already drove it** — it
  collapses the multi-selection. That is what `ViewportAdditive` distinguishes.
- **Never re-push the drag line every tick.** External sub-channels are RETAINED (P5-D61/S3): the capture
  re-emits them without clearing. Push only when the grab point or the anchor moved, and move it by
  `Clear_External` + a fresh `Draw_ExternalLine` — the draw calls APPEND, they do not replace.
- **Never guess the drag's grab point.** It comes from `TryPick_BodyHit`, on the surface the user clicked
  — a bounds centre is a guessed depth, and a wrong one gives the spring a lever arm that spins the body.
  Never re-pick to get it either: the press's own pick already produced it.
- **Never arm the drag on the selection rather than on the PICK.** The arm carries the key the press
  resolved; a Ctrl+click on empty space must open no drag at all.
- **Never end a drag anywhere but `HandleDragRelease`.** It is idempotent and gated on the subsystem's own
  `Get_IsDragging()`, which is what lets the destructor and the world-change path just call it.
- **Never end a drag on the CURRENTLY selected subsystem.** The release runs after the world selector has
  moved; the subsystem the drag began on is captured weakly at arm and is the one that has to be asked.
- **Never fully sort the capture's labels to apply the cap.** The selection is a bounded max-heap of `Cap`
  and it writes into a member scratch array — a full sort per paint at 100k labels is the cost the cap
  exists to avoid.
- **Never scale an ORTHO gesture off the orbit distance.** In ortho the ortho width is what says how much
  world a pixel covers.
- **Never let the wheel shrink the orbit distance.** Eye and pivot move together, or the dolly stalls at
  the pivot and the user cannot fly through what they last framed.
- **Never leave the window's selection set behind the outliner's.** The panel prunes; the window re-derives
  from `Get_SelectedAll()` on the same refresh, or the highlight and the isolation name dead keys.
- **Never re-push the grid.** It is a retained channel: one push, and `Clear_External` is the only thing
  that empties it.
- **Never allocate a brush or a font in `OnPaint`.** The label overlay's font is a member built once in
  `Construct` (`CkDebuggerCommon/CLAUDE.md` § OnPaint), and the deprecated no-argument `ToPaintGeometry()`
  overload is not used.
- **Never poll `Get_CurrentOverlaps`.** It returns a full `TSet` **copy** — read it for the PRIMARY
  selection only, digest the result, and re-push the External channel only when that digest moves.
- **Never fake a per-constraint reference frame.** Jolt's `DrawConstraintReferenceFrame` takes no
  constraint; selecting one arms the flag for the whole world, and that is disclosed rather than
  worked around.
- **Never give a constraint row a `BodyKey`.** It draws nothing, and a key there would shadow the body
  row that owns it in the shared key → row lookup. `ConstraintBodyKeys` is the pair, A first.
- **Never let the hover fire during a gesture**, and never leave it un-throttled. It is a
  `TryPick_Body` per event, and the mouse produces one per pixel.
- **Never let the collector compute a health flag.** It reads the ECS; the flags are the CAPTURE's, and
  the collector's `Apply_ProblemFlags` only stamps what the facility already decided.
- **Never let the drag path run on a client.** It is the only sim-mutating act this module performs, and
  the whole path is gated on `GetNetMode() != NM_Client` and behind the same `#if !UE_BUILD_SHIPPING` as
  the facility API it calls.

---

## See also

- `CkJolt/Claude.md` (CkFoundation) — the facility: target, capture processor, palette, bucket model,
  draw channels and draw flags. The filename really is `Claude.md`, not `CLAUDE.md` — the module's own
  docs are the odd one out in this suite, and a `CLAUDE.md` link from here resolves on Windows and breaks
  everywhere else.
  The contract this module renders.
- `CkDebuggerCommon/CLAUDE.md` — shared widgets, chrome and lane rules, row contracts, safety rules.
- `CkDebuggerLauncher/CLAUDE.md` — the descriptor census this module's tab id is enforced against.
