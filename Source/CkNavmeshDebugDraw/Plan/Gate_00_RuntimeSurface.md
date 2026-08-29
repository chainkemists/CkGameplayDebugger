# Gate 0 — runtime surface and retained component

> **Status:** Done (2026-08-28)
> **Depends on:** CTO packaged-boundary confirmation received 2026-08-28

## Goal

After this gate, the new Runtime module is present in non-Shipping targets, inert in Shipping, and can lazily create, replace, and destroy an actorless retained triangle component without per-frame work.

## Entry criteria

- [x] CkGameplayDebugger and CkFoundation submodules clean on current `dev`.
- [x] Existing root crowd-planning dirt identified and excluded.
- [x] Runtime build-guard, module, component, and material patterns spot-checked against current code.

## Work items

1. Add `CkNavmeshDebugDraw` Runtime descriptor and non-Shipping definition.
2. Add immutable triangle snapshot validation and a `UMeshComponent`/`FPrimitiveSceneProxy` retained renderer, following the engine CustomMesh resource-lifetime pattern.
3. Add module-owned world-aware enable/disable/refresh commands with no disabled ticker.
4. Add focused tests for valid geometry, invalid atomic rejection, unchanged revision, and clear behavior.

## Expected observations

| I will run | I expect to observe | If instead I see | Response |
|---|---|---|---|
| Focused `Ck.NavmeshDebugDraw` tests | Invalid input leaves the prior snapshot intact; clear removes the proxy source. | Partial mutation or ensure-only recovery | Stop and repair the validation boundary before Gate 1. |
| Shipping compile guard inspection | Commands, session, and render resources are absent behind `WITH_CK_NAVMESH_DEBUG_DRAW=0`. | Runtime work remains reachable | Move the complete behavior, not only commands, behind the guard. |

## Exit criteria

- [x] Focused tests green with fresh log inspection (5/5 across the component and subsystem filters).
- [x] Development Editor and Development Game targets build through UnrealToolbox.
- [x] Change-control checklist complete for the guarded runtime surface.
- [x] `PLAN.md`, this status, `PROGRESS.md`, and module `CLAUDE.md` updated together.
