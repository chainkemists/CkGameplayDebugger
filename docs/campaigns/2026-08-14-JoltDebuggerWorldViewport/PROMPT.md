# Jolt Debugger — World Viewport (PROMPT.md)

> **Written:** 2026-08-14. STABLE content only — current state lives in [PROGRESS.md](PROGRESS.md).
> **This doc dies when:** the campaign ships and `Source/CkJoltDebugger/CLAUDE.md` +
> `CkFoundation/Source/CkJolt/Claude.md` absorb the permanent contributions. On death: delete, or
> tombstone ("Superseded by X — kept for history").

## Goal

The Jolt debugger window owns a real Unreal world (an `FPreviewScene`, the way the Animation
Sequence viewer owns one) and renders **every Jolt body of the selected game/PIE world** into it as
retained instanced meshes — wireframe or solid, colored by body class — with an outliner that
selects the ECS entities backing those bodies, and Crowd-debugger-parity camera controls. Rendering
stays smooth at 100,000+ bodies. This becomes the reference implementation the Crowd debugger is
refactored onto in a later campaign.

## Success criteria

1. Opening the Jolt debugger during PIE shows a 3D viewport (debugger-owned preview world) drawing
   all four body populations: JoltBody entities (dynamic/kinematic/static), baked static-world
   bodies (`JoltStaticActor`), Probe sensors, and JoltCharacter capsules.
2. Wireframe and solid modes toggle from the menu bar; per-body color by class (static / kinematic
   / dynamic / sensor / character distinct; sleeping visibly dimmed) survives in **both** modes.
3. Camera bar has parity with the Crowd debugger: Perspective, Top/Bottom/Left/Right/Front/Back
   ortho, Frame All, Frame Selection; orbit/pan/zoom/WASD flight; `F`/`Home` hotkeys.
4. An outliner (virtualized `SListView`, searchable/filterable) lists body-backing entities;
   row-select highlights the body in the viewport; clicking a body in the viewport selects the row;
   selection round-trips through `ck::DebugSelectionSync` and an `FCkDebug_EntityTargetRoute`
   ("Open In" from the ECS debugger reaches it).
5. At 100,000 total bodies (mostly static world) with ≤~1,000 awake dynamic bodies, the debugger's
   per-frame cost is bounded: static buckets reconcile to no-ops, only changed transforms upload,
   and the measured reconcile stat (`STAT_CkJolt_DebugDrawReconcile` or successor) stays within the
   budget recorded at Phase 4 entry. Numbers are measured, not estimated.
6. The existing in-world debug draw (`ck.Jolt.DebugDraw.*` CVars + `SCkJoltDebuggerWindow` stat
   sections) keeps working unchanged — regression bar, not a rewrite target.
7. The window survives PIE end without crash: cached handles cleared on
   `ck::DebugSessionLifecycle` invalidation / `EndPIE`; teardown on `OnEnginePreExit`.
8. Full toolbox build+test gate green, delta-zero vs the campaign baseline;
   `CkDebuggerLauncherCatalog.spec.cpp` census still exact.

## Constraints & locked decisions

| # | Decision | Choice | Why |
|---|---|---|---|
| P0-D1 | Scene architecture | Debugger-owned `FPreviewScene` world + a **generalized, world-targetable instance** of CkJolt's existing batched `JPH::DebugRenderer` (shape→StaticMesh→per-(geometry,color) ISM, `BatchUpdateInstancesTransforms`). **No private ECS registry/EcsWorld.** | User-ruled 2026-08-14. Every piece exists and is packaged-safe; a private ticking EcsWorld has zero precedent and needs new scheduler machinery for no render-perf gain. |
| P0-D2 | Wireframe mechanism | Material swap on the same ISM instances: unlit solid material ⇄ wireframe-flagged material. Toggle = material swap, no geometry rebuild. | User-ruled 2026-08-14. Preserves per-body color in both modes; GPU-side wireframe. |
| P0-D3 | Coverage | All four populations: JoltBody entities, baked static world, Probe sensors, JoltCharacter capsules. | User-ruled 2026-08-14. |
| P0-D4 | Scale bar | 100,000+ total bodies smooth. Culling/change-gating/instancing discipline designed in from Phase 1, measured at Phase 4. | User-ruled 2026-08-14. |
| P0-D5 | Data split | Debug data/scaffolding lives in CkFoundation (`CkJolt`) so its processors/subsystem update it fast; `CkJoltDebugger` owns only presentation (DataCollector → flat snapshots → ViewModel → Slate, the SM/Intent/Crowd pattern). Never a CkFoundation→debugger dependency. | User requirement + suite doctrine (`ck-gameplaydebugger-extension`). |
| P0-D6 | Routing | Fable orchestrates; Opus 5 executes by default; Sonnet for mechanical units; Fable only for design rulings + adversarial audits. | User directive 2026-08-14 (Fable usage constrained). |

## Non-goals

- **Crowd debugger refactor** — explicitly the *next* campaign; this one only builds the reusable
  facility it will adopt.
- **Editing/manipulating bodies from the debugger** — v1 is read-only observation.
- **Replication/multi-world aggregation** — one selected world at a time (shared
  `FCkDebuggerModel_WorldSelector` behavior).
- **AngelScript/Blueprint surface** — the debugger plugin has none; keep it that way.
- **Gen-1 `CkGameplayDebugger` module** — frozen; nothing lands there.

## Reading list (reference modules to mimic — mimicry beats invention)

- `CkFoundation/Source/CkJolt/Subsystem/CkJolt_DebugRenderer.{h,cpp}` + `CkJolt_Subsystem.cpp:511-601`
  — the batched renderer being generalized; `CkJolt/Claude.md` §"Debug draw + stats".
- `CkGameplayDebugger/Source/CkCrowdDebugger/Public/CkCrowdDebugger/Viewport/SCkCrowdDebugger_3dViewport.{h,cpp}`
  — the preview-scene viewport + camera-preset shell to copy (`FPreviewScene` + `FSceneViewport` +
  `FUMGViewportClient`; 9 camera presets; pick-ray).
- `CkCrowdDebugger` window/list/detail/ViewModel/DataCollector files — the module shape template.
- `CkIntentDebugger` + `CkStateMachine/Debug/` — the CkFoundation-side debug-data pattern
  (demand gate: `Get_IsDebugDataDesired` / `NotifyDebugDataConsumed`).
- `CkDebuggerCommon/CLAUDE.md` §"Creating a new debugger module — checklist" + §"Common window
  chrome and entity targeting"; `CkGameplayDebugger/CLAUDE.md` lifecycle contracts.
- `CkJolt/Body/CkJoltBody_Fragment.h`, `StaticWorld/CkJoltStaticActor_Fragment.h`,
  `CkSpatialQuery/Probe/CkProbe_Fragment.h`, `Character/CkJoltCharacter_Fragment.h` — the four
  populations and their tags.

## Things ruled out — do not re-investigate

| Ruled out | Why | Evidence |
|---|---|---|
| Debugger-owned ticking `FEcsWorld` + CkIsm mirror entities | Zero precedent; `FEcsWorld` has no scheduler (only the two world subsystems build one); CkIsm + ECS subsystems exclude `EditorPreview` worlds; adds framework blast radius for no render-perf gain | Research 2026-08-14: `CkEcsWorld_Subsystem.cpp:450-487`, `CkEcsEditor_Subsystem.cpp:31-46`, `CkIsmSubsystem.cpp:143-149` |
| PDI immediate-mode drawing as the primary body renderer (Crowd style) | Re-emits all geometry every frame; cannot meet 100k bar; user explicitly wants retained rendering in a real world | User brief + `CkPmg/CLAUDE.md` (per-tick line drawing "tanked perf") |
| CkPmg procedural shapes for bodies | No convex-hull/arbitrary-mesh generator; entity-based (same world gating problem); 32 analytic shapes insufficient for Jolt mesh/convex shapes | Research 2026-08-14 rendering survey |
| Gen-1 `Bridge/`/`Category/` machinery | Legacy UE-GameplayDebugger plumbing, frozen since 2024 | `CkGameplayDebugger/CLAUDE.md` §Three generations |
| `DebugRendererSimple` instead of batched | Decomposes every triangle of every body per frame | `CkJolt/Claude.md` §"Debug draw + stats" |
| `SEditorViewport`-based viewport | Editor-only; house pattern is `SViewport`+`FUMGViewportClient` for packaged support | Rendering survey; zero `SEditorViewport` in `Plugins/` |
