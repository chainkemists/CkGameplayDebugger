# CK Debugger Suite — World Interaction Phase (Design Spec)

**Date:** 2026-07-11 · **Status:** approved (maintainer, pre-implementation) · **Scope:**
`CkDebuggerCommon`, `CkEcsDebugger`, `CkCrowdDebugger`, `CkGoapDebugger`, plus at most one
read-only additive getter in `CkIsmRenderer` (CkFoundation, class-2, gated per `ck-change-control`).

## 1. Problem & goals

Five maintainer-reported gaps in the debugger suite's *world* interaction (as opposed to the
Slate-tab interaction the redesign campaign already shipped):

1. Selecting an entity draws XYZ axes via one-frame `DrawDebugTransformGizmo` re-issued from the
   inspector panel's refresh-gated Slate tick — it flickers whenever the gate caps below frame
   rate, and re-batches lines every tick (`CkInspector_Transform.cpp:69`,
   `CkDebuggerPanel_Inspector.cpp:117`).
2. No way to focus the PIE viewport camera on an entity selected in any debugger (the existing
   "focus" double-tap only raises the ECS tab; no camera-move code exists in the suite).
3. Crowd/GOAP feature entities belong to an owner NPC/player, but neither the ECS inspector nor
   the Crowd list says who that is.
4. Crowd debugger: the player isn't drawn on the 2D map; agents can't be focused in PIE;
   commanding requires the "Take Control" arm + left-click; a world-picked or ECS-tree-picked
   entity doesn't visibly resolve to an agent (picker selections are applied `ESelectInfo::Direct`
   and are deliberately never re-broadcast — `CkDebuggerWidget_EntityTree.cpp:1497-1500`).
5. The viewport picker shows a diamond per entity; entities that HAVE visible meshes should be
   pickable by their mesh (Unreal-style hover/click), with diamonds reserved for meshless entities.

**Non-goals (this phase):** follow-cam while possessed; multi-select crowd commands; ISKM
per-pixel picking if batched clusters prove untraceable (falls back to diamonds, recorded as a
limitation); replacing the `DrawDebugString` entity label above the gizmo.

## 2. Workstreams

### A. Selection-sync broadcast fix + visible receives

- `FCkDebuggerModel_ViewportPicker::OnMouseClicked`: after `Set_SelectedEntities`, call
  `ck::DebugSelectionSync::Broadcast(Hit, TEXT("EcsDebugger"))`. (ECS's own receive ignores
  source `EcsDebugger`; Crowd/GOAP lineage-match as they already do.)
- Crowd + GOAP `OnGlobalSelectionSync`: on match, `RequestScrollIntoView` the row and start a
  ~1 s row accent flash (tick-faded tint on the row widget, no Slate rebuild). Crowd additionally
  centers the 2D map on the matched agent's dot (reuse follow/frame plumbing, one-shot).

### B. PMG selection gizmo

- New `FCkDebug_PmgGizmoSet` (`CkDebuggerCommon/Markers/`): keyed set of persistent PMG pivot
  overlays. `Sync(world, entries)` creates missing (`Create_Pivot` or 3 explicit arrows, see
  below; `Duration=-1`), moves existing via `UCk_Utils_Transform_UE::Request_SetTransform`,
  destroys vanished; `Reset()` destroys all. All handles cleared on `Reset`; owner clears on
  EndPIE per the module contract.
- `FCkInspector_Transform` + `FCkInspector_SceneNode`: replace per-tick
  `UCk_Utils_DebugDraw_UE::DrawDebugTransformGizmo` with a gizmo set updated from `Tick`,
  destroyed in `OnDeactivated`. Camera inspector unchanged.
- **Resolved-at-implementation:** Pivot is a composite of 3 child Arrow entities;
  `FProcessor_Pmg_DebugShape_UpdateTransform` skips composites. If moving the pivot parent does
  not carry its arrows, the gizmo set creates/moves the 3 arrows itself (X=Red `ForwardVector`,
  Y=Green `RightVector`, Z=Blue `UpVector`, 100 uu, arrowheads) — visual parity either way.

### C. Focus-entity-in-PIE (one-shot, ejected-only)

- Extract duplicated view logic from picker + overlay into
  `CkDebuggerCommon/Navigation/CkDebug_ViewportView.{h,cpp}`:
  `TryGet_LevelEditorViewport()`, `Get_ViewCameraLocation(World)`,
  `Deproject(World, ScreenPos, OutOrigin, OutDir)` — `WITH_EDITOR`-gated internals, both
  existing call sites re-pointed (behavior-neutral refactor).
- New `ck::DebugFocus::Focus_Entity(const FCk_Handle&) -> bool`
  (`CkDebuggerCommon/Navigation/CkDebug_Focus.{h,cpp}`): bounds resolution order —
  ISM proxy `Get_MeshBounds` → ISKM `Get_AnimatedMeshBounds` → recursive owning-actor
  `GetActorBounds` → 100 uu fallback box at `Get_EntityCurrentTransform` location; then frame
  the level-editor viewport on that box (`FocusViewportOnBox`; verify exact 5.7.4 API, manual
  `SetViewLocation` fit math as fallback). Returns false (no-op) when not ejected/simulating.
- Wiring: **F key** (`OnKeyDown`) + context-menu entry "Focus in Viewport (F)" on the ECS entity
  tree, Crowd agent list, GOAP agent list; Focus button in Crowd `AgentDetailPanel`.

### D. Crowd: player marker + RTS commands

- **Player on the 2D map:** DataCollector adds player pawn position (+ ejected-camera position
  when ejected) and camera yaw (already sampled) to the snapshot; `ViewportPanel::OnPaint` draws
  a distinct chevron + FOV wedge; legend updated. Assign `PlayerProxy` status to agent rows whose
  entity is the player-controlled pawn's (via `UCk_Utils_OwningActor` + controller check).
- **2D map RMB command:** RMB-down no longer pans immediately — pan engages after a ~4 px drag
  threshold; RMB-up under threshold with a selected agent issues
  `Request_SetDebugOverride(true)` (auto-arm, if not already) +
  `Request_MoveTo(FVector(MapPoint.XY, Agent.Z))`. "Take Control" button relabeled
  "Release Control" (explicit hand-back); LMB dot-select unchanged.
- **In-world RMB command:** Crowd-owned `IInputProcessor` (picker pattern: register on window
  open, unregister on close/EndPIE), active only when ejected + agent selected. RMB-up without
  drag → `Deproject` (shared utils) → `ECC_Visibility` trace → auto-arm + `Request_MoveTo(hit)`.
  Consumes only the qualifying up (and its down); RMB-drag camera-look untouched. Destination
  acknowledged with a ~1.5 s PMG ring (`Create_Ring`, finite duration — self-destroys).

### E. Parent-NPC linkage UX

- **ECS inspector breadcrumb:** pinned strip at the top of the inspector panel: lifetime-owner
  chain root→selected (transient root omitted, depth-capped at 8), each hop an
  `SCkDebug_EntityRef` pill (`ShowName=true`), separator chevrons. Rebuild only on selection
  change (stable identity; no per-tick Slate rebuild).
- **Crowd list/detail:** agent rows gain an Owner column — plain `STextBlock` of the owner's
  debug name (click-trap doctrine: no interactive pills inside `STableRow`); `AgentDetailPanel`
  gains an "Owner:" row with an `SCkDebug_EntityRef` pill. Owner = first lifetime ancestor that
  is not the agent feature entity itself (one hop up; matches how GOAP rows already list owners).

### F. Mesh-aware picking

- **ISM instance resolution:** during marker gather, build a reverse map
  `(UPrimitiveComponent*, InstanceIndex) → entity` from an EnTT view over ISM proxy fragments
  (read-only walk — the established data-collection pattern). On `LineTraceSingleByChannel` hit
  with a valid `FHitResult.Item`, resolve through the map before the actor-attachment walk.
  If proxy fragments don't expose the owning component publicly, add ONE read-only getter to
  `CkIsmRenderer` utils (class-2 additive, `ck-change-control` gate).
- **Hover = bounds:** the hovered entity additionally draws a one-frame `DrawDebugBox` of its
  resolved bounds (same resolver as C) from the picker's ungated tick — immediate-mode is correct
  for transient hover; retained PMG stays reserved for persistent selection.
- **"Meshes first" toggle** (picker toolbar, persisted in `UCkEcsDebuggerSettings`): suppresses
  diamonds for mesh-resolvable entities (actor- or ISM-backed), leaving diamonds only on meshless
  entities. Everything remains pickable.

## 3. Acceptance

| # | Check | How |
|---|---|---|
| A1 | World-picked entity selects the matching Crowd agent + GOAP row, scrolled into view with flash | `[EDITOR-VERIFY]` |
| B1 | Selecting an entity shows a stable RGB gizmo with zero flicker at any refresh-gate setting; deselect/EndPIE leaves no orphan shapes | `[EDITOR-VERIFY]` + existing `Ck.EcsDebugger.*` specs stay green |
| C1 | F in ECS tree / Crowd list / GOAP list frames the entity while ejected; no-op possessed | `[EDITOR-VERIFY]` |
| D1 | Player chevron + wedge on 2D map; RMB-click commands selected agent on map and in-world (ejected); RMB-drag still pans/looks | `[EDITOR-VERIFY]` |
| E1 | Breadcrumb shows owner chain; Crowd rows show owner name; pills navigate | `[EDITOR-VERIFY]` |
| F1 | Clicking an ISM-rendered NPC's mesh picks its entity; hover shows bounds box; "Meshes first" hides its diamond | `[EDITOR-VERIFY]` |
| G1 | Full debugger-suite compile + `--test-pattern EcsDebugger` green after each workstream | toolbox |

## 4. Risks / verify-against-source items

- `FocusViewportOnBox` availability/signature on UnrealEngine-Angelscript 5.7.4 (fallback: manual fit).
- Pivot composite transform propagation (fallback: explicit 3 arrows).
- ISKM batched clusters may not produce per-instance trace hits — ISKM entities then stay
  diamond-picked this phase (recorded limitation, not a defect).
- RMB threshold interplay with editor camera-look while ejected — tune threshold in PIE.

## 5. PIE feedback round 1 (2026-07-11) — verdicts + revisions

Maintainer verified A–F in PIE. A (sync) and E (owner linkage) passed as shipped. The rest
produced seven revision items, all code-complete in this round:

| # | Feedback | Root cause / change |
|---|---|---|
| R1 | In-world RMB never fires — the editor viewport's context menu owns the ejected RMB click. Maintainer: map-only commanding is fine. | `FCkCrowdDebugger_WorldCommandProcessor` deleted (files + registration). The 2D-map RMB command now draws the in-world PMG destination ring itself. Spec §D's "in-world (ejected)" acceptance clause is void; D1 is map-only. |
| R2 | Gizmo axes are flat (edge-on-invisible) — wants real cylinders/cones with bright outlines. | `FCkDebug_PmgGizmoSet` rebuilt on `UCk_Utils_Pmg_BasicShapes::Create_Cylinder/Create_Cone` (6 parts/gizmo). `InDrawLines=true` outlines are baked as a second procmesh section at alpha 1 (`FProcessor_Pmg_DebugShape_BakeLines`) — retained geometry, moves with the entity, cannot blink. |
| R3 | Focus: (a) force-eject when possessed, (b) glide not teleport, (c) too close — 2× distance, tunable. | (a) queued focus + `GEditor->RequestToggleBetweenPIEandSIE()` + ticker completes once ejected (EndPIE clears the queued handle); (b) `FViewportCameraTransform::TransitionToLocation` (fork-verified, EditorViewportClient.h:283/490); (c) `ck.Debug.Focus.DistanceScale` cvar, default 2.0. |
| R4 | Player chevron doesn't rotate. | Collector sampled `GetActorRotation().Yaw` — orient-to-movement bodies don't follow the camera. Now `GetBaseAimRotation().Yaw`. |
| R5 | Mesh picking dead in ISKM stress gyms (500 agents). | TWO causes: (1) ISKM entities carry `IskmProxy` (CkIskmRenderer), not `IsmProxy` — every gate was blind to them; (2) renderer ISMs run `NoCollision` — the visibility trace can never hit. Fix: `DoIsMeshResolvable` accepts IskmProxy; new analytic ray-vs-bounds candidate stage in `DoPickAtRay` over per-tick cached `_MeshPickBounds` (collision-independent). ISKM pick volume = the 1 m-box bounds fallback until a `Get_MeshBounds` exists on IskmProxy utils (recorded CkFoundation follow-up). |
| R6 | Transform missing from rail + has/is badges. | Deliberate exclusion in `Get_BadgeFeatures()` removed (Label stays excluded). Also enables `has:transform` query token. |
| R7 | One icon rail scrolls — split across both flanks of the tree. | `Build_FeatureRail(bool InRightFlank)`: whole groups greedily assigned to the lighter flank in display order; deterministic; each flank still scrolls when short. |

Re-verify queue (round 2): B1 solids + outlines legibility, C1 auto-eject/glide/distance feel,
D1 chevron rotation + map ping, F1 in the ISKM gym (hover box = 1 m box for now, "Meshes First"
declutter), R7 two-flank layout, Transform chip/badge presence.
