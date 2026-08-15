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
| — | Ship (ck-ship-dev, user-gated) | both + superproject pointer bumps | 🟢 READY — all commits local; push + pointer bumps await user (P0-D8/P4-D36) |

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
