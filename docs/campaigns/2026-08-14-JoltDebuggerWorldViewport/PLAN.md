# Jolt Debugger — World Viewport — PLAN.md (phase index)

> **Written:** 2026-08-14. Update the Status column **in the same commit** as each phase landing.
> Phase contracts are authored at phase entry (PHASE_1 exists; later ones get written when their
> predecessor's exit checklist is green — plans written early snapshot conventions that move).

| Phase | Name | Scope (repo) | Status |
|---|---|---|---|
| 1 | World-targetable batched Jolt debug renderer | CkFoundation/CkJolt | ✅ Done 2026-08-15 (uncommitted, pending user commit approval) |
| 2 | Preview-world viewport shell + wireframe/solid + camera bar | CkGameplayDebugger/CkJoltDebugger | ✅ Done 2026-08-15 (uncommitted; `[EDITOR-VERIFY]` pending) |
| 3 | Outliner + selection sync + picking + detail | CkGameplayDebugger/CkJoltDebugger (+ CkJolt read APIs as needed) | ✅ Done 2026-08-15 (committed; `[EDITOR-VERIFY]` pending) |
| 4 | 100k scale hardening, measurement, polish, docs | both | ✅ Done 2026-08-15 (committed; measured 100k: revision/selection pass 250→23 ms; `[EDITOR-VERIFY]` pending) |
| 5 | Draw channels: lines/text/external, per-target draw flags, contact recording, highlight visibility, colour-by modes | CkFoundation/CkJolt | ✅ Done 2026-08-15 (committed locally; `[EDITOR-VERIFY]` pending) |
| 6 | Pause/step, body+character detail sample, selection contacts, multi-select/isolate, mouse-drag facility, stats extension | CkFoundation/CkJolt | ⏳ Pending |
| 7 | Unreal-scheme camera, Draw command lane, sim controls, detail panel, multi-select/isolate/follow UI, drag UI | CkGameplayDebugger/CkJoltDebugger | ⏳ Pending |
| 8 | Constraints population, probe results, health checks, labels+hover, grid/gizmo/bookmarks, close-out | both | ⏳ Pending |
| — | Ship (ck-ship-dev, user-gated) | both + superproject pointer bumps | 🟡 withheld — extended by phases 5–8 |

## Phase summaries (contracts to be authored at entry)

**Phase 1 — CkFoundation renderer facility.** Generalize `CkJoltDebugger`
(the `JPH::DebugRenderer` subclass in `CkJolt/Subsystem/CkJolt_DebugRenderer.h` — rename to kill
the name collision with the debugger module) into an instantiable, world-targetable batched
renderer: explicit target `UWorld` (incl. `FPreviewScene` worlds), solid/wireframe material mode
(P0-D2), body-class palette (P0-D3), capture API the debugger can pump from the game world's
`PhysicsSystem`, static-body no-op reconcile. Existing subsystem in-world draw keeps working
(success criterion 6). Headless spec coverage where possible.

**Phase 2 — viewport shell.** Copy `SCkCrowdDebugger_3dViewport` shell (preview scene, viewport
client, 9 camera presets, orbit/pan/zoom/WASD) into `CkJoltDebugger`; instantiate the Phase-1
renderer against the preview world; menu-bar wireframe/solid toggle + population toggles + color
legend; window chrome/command-lane contracts. `[EDITOR-VERIFY]` PIE pass.

**Phase 3 — outliner + selection.** `Data/CkJoltDebugger_Types.h` (flat snapshots) +
`DataCollector` + `ViewModel` (SM/Intent/Crowd shape); virtualized outliner with dual search;
`FCkDebug_EntityTargetRoute` + `DebugSelectionSync` round-trip; viewport click-pick (body →
entity via Jolt UserData); game-viewport picker with `TargetFilter`; Frame Selection wiring;
selected-body highlight; detail panel (motion type, sleep, velocity, shape, layer).

**Phase 4 — scale + polish.** Synthetic 100k-body stress source (gym or test map), measured
budgets recorded, culling/filtering as measurements dictate (population toggles, region/AABB
filter if needed), settings persistence, launcher census spec, module CLAUDE.md updates
(CkJolt + CkJoltDebugger), final full gate + adversarial review, ship-prep.

> **Phases 5–8 added 2026-08-15** after the user's live PIE pass. Phases 1–4 delivered a *shape
> viewer*; the user's report is that it must become a *physics debugger*: Unreal-scheme camera,
> the rest of Jolt's debug-draw vocabulary, and hands-on interaction. Phases 5–6 extend the
> CkFoundation facility; Phases 7–8 are the presentation surface. Ship is withheld until Phase 8.

**Phase 5 — draw channels + draw flags + colour modes (CkJolt).** `FCk_Jolt_DebugDrawTarget`
gains a per-target line channel (`ULineBatchComponent` in the target world), a label channel, and
an External channel for non-JPH contributors; `FCk_Jolt_DebugRenderer::DrawLine/DrawTriangle/
DrawText3D` route to the active target instead of `DrawDebugLine`. Per-target
`FCk_Jolt_DebugDrawFlags` bitmask replaces the CVar-only draw vocabulary (the in-world draw is
re-hosted onto the same flags, CVars remaining its source of truth). Contact draw is recorded
around `PhysicsSystem::Update` and replayed into every demanding target. Highlight becomes
unmistakable and gains a Hover sibling; colour-class becomes a colour-*mode* (BodyClass /
SleepState / ObjectLayer / Island / ShapeType) with a legend API.

**Phase 6 — sim control + inspection + interaction facility (CkJolt).** Pause/step-once on the
Jolt world plus a recorded step duration; a full per-body detail sample (mass, friction,
restitution, gravity factor, motion quality, layers, shape, island, AABB, user data) and a
character sample (ground state/normal/body/velocity), both taken in the capture pass; contacts of
the selected body via `NarrowPhaseQuery::CollideShape` on demand; multi-select and isolation;
a mouse-drag facility (kinematic anchor + spring `DistanceConstraint`) driven by queued requests
applied before the step; extended world stats.

**Phase 7 — camera, command lanes, sim controls, detail, selection, drag UI (CkJoltDebugger).**
The viewport's camera is rewritten to the Unreal editor scheme (RMB look + WASD fly, MMB pan,
wheel dolly, Alt orbit/dolly, LMB-drag forward+yaw). A "Draw" command lane exposes every Phase-5
flag and the colour mode, persisted in the settings object; toolbar Pause/Step + Space/Enter; the
detail panel shows the whole Phase-6 sample plus a clickable contacts list; outliner multi-select,
viewport Ctrl+click add, Isolate toggle, Follow-selection; Ctrl+LMB body dragging (authority
worlds only).

**Phase 8 — constraints, probes, health, labels, grid/gizmo/bookmarks, close-out.** A Constraints
population in the outliner; probe overlap/hit drawing from the ECS fragments; a health-check
collector (NaN, runaway velocity, below KillZ, degenerate shape) with a "Problems" filter chip and
a header badge; world-space labels rendered in `OnPaint` plus throttled hover highlight; ground
grid, world-axis gizmo, camera bookmarks; both module CLAUDE.md files, full serial gate,
adversarial review, local commits, ship still withheld.
