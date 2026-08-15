# Phase 8 — Constraints, probe results, health checks, labels/hover, grid/gizmo/bookmarks, close-out

> **Status:** ⏳ Pending
> **Depends on:** Phase 7 ✅
> **Scope (repo):** `Plugins/CkGameplayDebugger/Source/CkJoltDebugger/` primarily; **narrow**
> CkFoundation additions are in scope only where a work item names them (constraint read surface,
> external-channel retention if the STOP is ruled that way).
> **Estimate:** 2 sessions (3 implementation units + close-out unit)

## Goal

After this phase the debugger answers the questions the first seven phases left open: which
constraints exist and what they connect; what a probe is actually overlapping (not just where its
sensor shape is); which body in a hundred thousand is broken; what am I looking at (labels, hover,
a ground grid, an axis gizmo, camera bookmarks). Then the campaign closes: both module CLAUDE.md
files become the permanent record, the full serial gate runs on the final artifact, and everything
is committed locally with the ship still withheld.

## Entry criteria

- [ ] Phase-7 exit green and committed.
- [ ] Baseline for Phase 8 = those numbers.

## Design rulings (orchestrator, binding — verbatim)

| ID | Ruling |
|---|---|
| P8-D55 | **Constraints population** — if CkJolt exposes constraint entities/handles, list them keyed by handle with entity route; else list `PhysicsSystem::GetConstraints()` by index with body-pair display names and no route (record which branch applies after research). Selecting a constraint highlights both bodies + draws its reference frames. |
| P8-D56 | **Probe results** — draw the selected probe's current overlaps/hits from the ECS fragments via the External channel; toggle in the Draw lane. |
| P8-D57 | **Health checks** — collector flags per body: NaN transform/velocity, \|v\| > threshold (setting, default 5000 cm/s), below world KillZ, zero-extent shape; outliner filter chip "Problems", warning badge with count in the stats header. |
| P8-D58 | **Labels + hover** — labels channel (D38) rendered in viewport `OnPaint` (project world→screen with the current view); label on the primary selection always, on all bodies when the Labels flag is set (cap 500 nearest, log once); hover = throttled `TryPick_Body` on mouse move (≥ 60 ms) → Hover class + name tooltip. |
| P8-D59 | **Grid, gizmo, bookmarks** — grid at Z=0 (100 cm cells, 20 m extent, major line every 10) via the External channel each capture (toggle, persisted); world-axis gizmo in the viewport's bottom-left in `OnPaint`; camera bookmarks Ctrl+0..9 set / 0..9 recall, persisted in settings. |
| P5-D61 | **STOP-list rulings — the parts that bind this phase.** **S7 → fork (b):** the world-axis gizmo **REUSES `SCkDebug_OrientationCube`** (`CkDebuggerCommon/Public/CkDebuggerCommon/Widgets/SCkDebug_OrientationCube.h:34`) dropped into an overlay slot over the viewport — **no hand-drawn `OnPaint` gizmo**. Labels still need `OnPaint` (S13 stands as an accepted risk). **S8 → fork (a):** add public **`Get_BodyA` / `Get_BodyB`** read accessors on `UCk_Utils_JoltConstraint_UE` — or on the fragment via a friend-free getter; **the executor picks the doctrine-conformant spot and STOPs if it is unclear**. Reference frames are drawn **all-constraints while a constraint is selected** — ACCEPTED, documented, not faked per-constraint. **S3:** probe results and the grid push to **retained NAMED External sub-channels** and are removed with `Clear_External(Name)`; they are **not** re-pushed every Slate tick and they do **not** flicker. |
| P8-D60 | **Close-out** — module CLAUDE.md files updated (both), full serial gate + scoped gates, adversarial review per phase (fresh Opus drafts triage; orchestrator ratifies), commits local per phase, ship withheld (P0-D8/P4-D36 unchanged). |

## Research facts the executor must not re-derive

**P8-D55 — the branch is decided: (a), constraint ENTITIES exist.**
CkJolt ships a full constraint feature at `Plugins/CkFoundation/Source/CkJolt/Public/CkJolt/Constraint/`:
- `ck::FFragment_JoltConstraint_Current` (`CkJoltConstraint_Fragment.h:41-67`) holding
  `JPH::Ref<JPH::TwoBodyConstraint> _Constraint` `:54`, `_BodyA` `:55`, `_BodyB` `:56`,
  `_BodyBIsWorldAnchor` `:59`, `_ConstraintAdded` `:60`; tag `FTag_JoltConstraint_NeedsSetup` `:33`;
  params alias `:37`; requests fragment `:71-92`.
- `FCk_Handle_JoltConstraint`, `ECk_JoltConstraint_Type` (`CkJoltConstraint_Fragment_Data.h`).
- `UCk_Utils_JoltConstraint_UE` (`CkJoltConstraint_Utils.h:18`) — `Has :43-45`, `DoCast :52-55`,
  `Get_ConstraintType :76-78`, `Get_IsConstraintAdded :83-85`,
  `Get_Hinge_CurrentAngleDegrees :92-94`.
- Ropes build on it: `UCk_Utils_JoltRope_UE::Create_Rope` (`CkJoltRope_Utils.h`).
⇒ The outliner lists constraints **by handle with an entity route**, exactly like the other four
populations, and `Is_JoltDebuggerEntity` (`SCkJoltDebuggerWindow.cpp:213-226`) gains a fifth clause.
The **body keys are not on the public utils surface** — `_BodyA`/`_BodyB` are private with a friend
list (`CkJoltConstraint_Fragment.h:26-29`), so a narrow CkFoundation read API
(`Get_BodyA`/`Get_BodyB` as `FCk_Handle_JoltBody`, or the two body keys) is the one CkFoundation
addition this phase is authorised to make. Reference frames come from
`PhysicsSystem::DrawConstraintReferenceFrame` (`PhysicsSystem.h:159`) which is already wired to a
Phase-5 draw flag — per-constraint reference-frame drawing is NOT available in Jolt (the call is
all-or-nothing), so "draws its reference frames" means **enabling that flag while a constraint is
selected**; record the limitation.

**P8-D56 — probe results**
- Overlaps live in `ck::FFragment_Probe_Current::_CurrentOverlaps`, a
  **`TSet<FCk_Probe_OverlapInfo>`** at `CkSpatialQuery/Public/CkSpatialQuery/Probe/CkProbe_Fragment.h:58`
  (`CK_PROPERTY_GET` `:63`); the body id is `_BodyId` `:57` (`CK_PROPERTY_GET` `:62`).
- Public read API exists — **`UCk_Utils_Probe_UE::Get_CurrentOverlaps(const FCk_Handle_Probe&) -> TSet<FCk_Probe_OverlapInfo>`**
  (`CkProbe_Utils.h:192-197`, impl `.cpp:245-251`, **returns by value — a full set copy, so gate it
  on the selection, never poll it for every probe**). ProbeTrace twin at `CkProbeTrace_Utils.h:126`.
- `FCk_Probe_OverlapInfo` (`CkProbe_Fragment_Data.h:255-289`): `_OtherEntity` (`FCk_Handle`),
  `_ContactPoints` (`TArray<FVector>`), `_ContactNormal` (`FVector`) — enough to draw points +
  normals + a line to the other entity, and nothing else.
- `ck::FFragment_ProbeTrace_WorldContacts` (`CkProbe_Fragment.h:93-104`) holds only
  `TSet<FCk_Handle> _Entities` + `bool _AnonymousContact` — **no hit positions**. A ProbeTrace can
  therefore only be visualised as "these entities were hit", not as hit points. Record that.
- Probes carry **no shape data of their own** — the shape is a CkShapes fragment on the same entity
  (`UCk_Utils_Probe_UE::Add` ensures it, `CkProbe_Utils.cpp:44-45`); draw styling is
  `FCk_Probe_DebugInfo` (`CkProbe_Fragment_Data.h:222`: `_LineThickness :232`, `_Color :236`,
  `_OverlapColor :240`, `_DisabledColor :244`).
- **Dependency direction is CkSpatialQuery → CkJolt** (the probe processor calls the Jolt world's
  `Request_NoteBodyRemoved` at `CkProbe_Processor.cpp:1247`). CkJolt therefore **cannot** read probe
  fragments, so this drawing must originate in the debugger through `Draw_External*` — which
  collides with D38's "cleared per capture". See the STOP list.

**P8-D57 — health checks**
- The collector is `FCkJoltDebugger_DataCollector::Collect` (`Data/CkJoltDebugger_DataCollector.cpp:48-140`),
  four ECS passes; the row struct is `FCkJoltDebugger_BodySnapshot`
  (`Data/CkJoltDebugger_Types.h:38-57`). Health flags belong on the snapshot, computed from the
  **facility's** data — the collector must not read `JPH::PhysicsSystem`
  (`CkJoltDebugger/CLAUDE.md:409-412`). Transform/velocity per body are not currently exposed
  per-body by the facility (only for the selection) — so NaN/velocity checks need either a
  facility-side pass (a `Get_ProblemBodies()` computed in the capture, cheap because it is
  O(active)) or restriction to what the ECS already knows. **Prefer the facility-side pass** and
  name it in the work item; a Slate-side physics read is forbidden.
- KillZ is `AWorldSettings::KillZ` — reachable from the collector's `UWorld*` without touching Jolt.
- Shared widgets: filter chip `SCkDebug_Chip` (`Widgets/SCkDebug_Chip.h:39`), count badge
  `SCkDebug_CountBadge:15`, alert row `SCkDebug_AlertRow:20`, status pill `SCkDebug_StatusPill:18`.
  Never write a heat/category hex by hand — `ck::debug_axes::Get_HeatColor`/`Get_CategoricalColor`
  (`CkDebuggerCommon/CLAUDE.md:249-266`).

**P8-D58 / P8-D59 — 2D overlays are greenfield in this suite**
- **No `OnPaint` override exists in either the Jolt or the Crowd viewport**, and there is **no
  world→screen projection anywhere in `CkGameplayDebugger`**. The only view math in-suite is the
  inverse: `GetCursorWorldRay` (`SCkJoltDebugger_3dViewport.cpp:298-332`) builds a
  `FMinimalViewInfo`, calls `FMinimalViewInfo::CalculateProjectionMatrixGivenView` and
  `FSceneView::DeprojectScreenToWorld`. The forward direction must be built the same way from the
  client's current view location/rotation/FOV/ortho width — do not try to capture an `FSceneView`
  from Slate.
- `OnPaint` hard rule: **never allocate brushes or fonts in `OnPaint`**
  (`CkDebuggerCommon/CLAUDE.md:476-480`) — cache them as members. Also do not use the deprecated
  `ToPaintGeometry()` overload (`:471`).
- **`SCkDebug_OrientationCube` already exists** (`Widgets/SCkDebug_OrientationCube.h:34`, an
  `SLeafWidget` orientation gizmo). **P5-D61/S7 RULED that this widget is used** — D59's
  "gizmo in `OnPaint`" is superseded for the gizmo only; labels still go through `OnPaint`.
- Grid: Crowd's PDI `Draw` override (`SCkCrowdDebugger_3dViewport.cpp:518-547`) is the in-suite
  precedent for world-space line drawing in a preview scene, but **no debugger draws a ground grid
  today** (searched: no grid primitive, no `DrawGridLines`, no `UGridComponent`). D59 routes it
  through the External channel instead.
- Bookmarks: all key handling is `FCkJoltDebugger_3dViewportClient::InputKey`
  (`SCkJoltDebugger_3dViewport.cpp:118-221`) — there is no `FUICommandList` in this module or in
  `CkDebuggerCommon`. Ctrl+0..9 / 0..9 go there. Camera state to persist is
  `Get_ViewLocation`/`Get_ViewRotation`/`Get_ProjectionMode` (`:469-493`).

## Work items

### Unit A — constraints + probe results (P8-D55, P8-D56)

1. **CkFoundation (narrow, P5-D61/S8):** public `Get_BodyA`/`Get_BodyB` (or their debug-draw keys)
   on `UCk_Utils_JoltConstraint_UE`, **or** on the fragment via a friend-free getter — the executor
   picks whichever matches module doctrine and **STOPs if that is unclear** rather than widening the
   friend list. → verify: build; `--test-pattern Jolt` unchanged.
2. `Data/CkJoltDebugger_Types.h` — add `ECkJoltDebugger_Population::Constraint`; the snapshot gains
   the constraint type and the two body keys (reuse `NumBodies`/`SourceActorName` where the
   semantics fit rather than growing the struct for its own sake).
3. `DataCollector.cpp:48-140` — a fifth pass over `ck::FFragment_JoltConstraint_Current`.
   → verify: build; extend `Ck.JoltDebugger.Outliner.RowsSelectFilterAndSurviveRefresh` or add
   **`Ck.JoltDebugger.Outliner.ListsConstraintRows`** on a `ck::FEcsWorld{}` fixture.
4. `SCkJoltDebuggerWindow::Is_JoltDebuggerEntity` (`cpp:213-226`) — fifth clause; the module's
   `FCkDebug_EntityTargetRoute` predicate and the picker `TargetFilter` follow automatically
   (one shared predicate — `CkDebuggerCommon/CLAUDE.md:565-578`).
5. Selecting a constraint row highlights **both** bodies via `Set_HighlightedBodies` (Phase 6) and
   turns on the ConstraintReferenceFrames draw flag while the selection holds — documenting that
   Jolt's reference-frame draw is all-constraints, not per-constraint. → verify: build.
6. Probe results: a "Probe results" toggle in the Draw lane; when the primary selection is a Probe,
   read `UCk_Utils_Probe_UE::Get_CurrentOverlaps` and push contact points, normals and a line to
   each other entity through `Draw_External*`; a ProbeTrace selection draws only the
   hit-entity lines (`FFragment_ProbeTrace_WorldContacts` has no positions — say so in the UI's
   tooltip, not silently). Push to a **retained named sub-channel** (P5-D61/S3), e.g.
   `"JoltDebugger.ProbeResults"`, re-pushed only when the overlap set changes and removed with
   `Clear_External(...)` when the selection leaves a probe. → verify: build; `[EDITOR-VERIFY]` 3.

### Unit B — health checks + labels + hover (P8-D57, P8-D58)

7. Facility-side `Get_ProblemBodies()` (or an equivalent per-capture problem set) computed in the
   capture pass — NaN position/rotation/velocity, `|v| > threshold`, world AABB below the
   caller-supplied KillZ, zero-extent local bounds; threshold and KillZ passed in from the
   debugger so CkJolt owns no policy. → verify: build; new facility spec
   **`Ck.Jolt.DebugDraw.ProblemBodiesFlagTheBrokenOnes`** — a body given a NaN velocity and a body
   given a runaway velocity are both flagged, healthy bodies are not, and the flags clear when the
   condition does.
8. Snapshot gains the problem flags; the outliner gets a `SCkDebug_Chip` "Problems" filter that
   narrows to flagged rows, and the stats header gets a `SCkDebug_CountBadge` with the count (zero
   → no badge, not a "0" badge). → verify: build; new spec
   **`Ck.JoltDebugger.Outliner.ProblemsChipNarrowsToFlaggedRows`** (three rows, one flagged: the
   chip leaves exactly one visible, the pinned-selection rule still wins, clearing restores three).
9. Labels: `OnPaint` on `SCkJoltDebugger_3dViewport` projecting `Get_Labels()` (Phase 5) with a
   view/projection matrix built the same way `GetCursorWorldRay` builds its inverse; the primary
   selection is always labelled; the Labels flag adds all bodies capped at the **500 nearest** with
   a single `Display` log when the cap bites; brushes/fonts cached as members, never allocated in
   `OnPaint`. → verify: build; `[EDITOR-VERIFY]` 5.
10. Hover: throttled (≥ 60 ms) `TryPick_Body` on mouse move in the viewport client →
    `Set_HoveredBody` (Phase 5 Hover class) + a name tooltip. Must not fight the pick gesture
    (`_PendingPickPress`, `:127-142`) and must clear on `LostFocus` (`:226-230`). → verify: build.

### Unit C — grid, gizmo, bookmarks (P8-D59)

11. Grid: pushed **once** into a retained named External sub-channel (P5-D61/S3), e.g.
    `"JoltDebugger.Grid"`, Z=0, 100 cm cells, 20 m extent, major line every 10, colour from
    `ck::debug_axes` not a hand-written hex; the toggle simply pushes or `Clear_External`s it, and
    the capture re-emits it every frame without the debugger doing anything. Persisted.
    → verify: build; `[EDITOR-VERIFY]` 6.
12. World-axis gizmo — **P5-D61/S7 ruled fork (b): reuse `SCkDebug_OrientationCube`**
    (`Widgets/SCkDebug_OrientationCube.h:34`) in an overlay slot over the viewport, fed the
    client's current view rotation. **No hand-drawn `OnPaint` gizmo.** → verify: build.
13. Bookmarks: `Ctrl+0..9` stores the current view location/rotation/projection into
    `UCkJoltDebuggerSettings::CameraBookmarks`, `0..9` recalls it; both in `InputKey`; persisted.
    → verify: build; extend `Ck.JoltDebugger.Settings.ConstructRestoresPreferences` to round-trip a
    populated bookmark array.

### Unit D — close-out (P8-D60)

14. `CkJolt/Claude.md` **and** `CkJoltDebugger/CLAUDE.md` finalised: every new API, flag, mode,
    spec, cost number, anti-pattern, `[EDITOR-VERIFY]` line and the standing `[PACKAGED-VERIFY]`.
    Fix the two known docs defects: the spec-census file attribution for `Benchmark.ScaleMatrix`
    (`CkJolt/Claude.md:523`) and the `CkJolt/CLAUDE.md` vs `Claude.md` filename mismatch in the
    debugger's cross-references (`CkJoltDebugger/CLAUDE.md:386`, `:430`).
15. Final adversarial review (fresh Opus drafts the triage; orchestrator ratifies) → fix-up.
16. **Gate of record on the final artifact**: full serial suite (== baseline set), scoped serial
    `Jolt`, `JoltDebugger`, `DebuggerLauncher`, `Probe`; benchmark re-run recorded.
17. PLAN + PROGRESS updated; the "Ship" section rewritten for the whole 8-phase campaign; commits
    LOCAL per phase; **ship withheld** (P0-D8 / P4-D36 unchanged).

## Fences

- Constraint reference-frame drawing is all-or-nothing in Jolt — do not fake per-constraint framing.
- The collector never reads `JPH::PhysicsSystem`; health data comes from the facility.
- `OnPaint` allocates nothing per frame; the 500-label cap is a hard cap with one log, not a
  silent truncation.
- `Get_CurrentOverlaps` returns a full `TSet` copy — call it for the selection only.
- No new debugger→CkFoundation dependency beyond the constraint read API named in item 1.
- Ship stays withheld regardless of how green the gate is (outward action, user-gated).

## `[EDITOR-VERIFY]`

1. Constraints appear in the outliner with their type; selecting one highlights both bodies;
   "Open In → Jolt" from the ECS debugger reaches a constraint row.
2. A rope (built by `UCk_Utils_JoltRope_UE`) lists its constraints and they all highlight sensibly.
3. Selecting a probe that is overlapping something draws contact points and normals; a ProbeTrace
   selection draws lines to the hit entities and the tooltip explains why there are no points.
4. Throwing a body far below KillZ, or NaN-ing one, makes the Problems chip and the header badge
   light up and narrows the list to it.
5. Labels appear on the selection, and turning the Labels flag on labels bodies up to the cap
   without tanking the frame rate; hovering a body highlights it subtly and shows its name.
6. The grid gives the empty preview world a sense of scale; the axis gizmo points the right way in
   every camera preset; Ctrl+3 then moving then 3 returns the camera exactly.
7. `[PACKAGED-VERIFY]` (still open from Phase 1 — `CkJolt/Claude.md:449-463`).

## Exit criteria — campaign close

- [ ] Full serial suite == baseline set; scoped serial `Jolt`, `JoltDebugger`, `DebuggerLauncher`,
      `Probe` all green on the FINAL artifact
- [ ] ≥ 5 new specs across the two repos (problem bodies, constraint rows, problems chip,
      bookmarks round-trip, plus whatever the fix-up adds)
- [ ] Both module CLAUDE.md files finalised; the two known docs defects fixed
- [ ] PLAN + PROGRESS updated; all commits LOCAL; **ship withheld and the ship instructions rewritten
      to cover phases 1–8 in one pass**
- [ ] The full `[EDITOR-VERIFY]` backlog (Phases 3–8) is collected in one place for the user
