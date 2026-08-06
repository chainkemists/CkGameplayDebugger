# Gate 00 - Entity truth and navigation

> **Status:** Implementation verified; editor acceptance pending
> **Depends on:** pre-change baseline captured at root `7850d857`, debugger `e31bc646`
> **Estimate:** 2-3 implementation days; re-date at exit.

## Goal

After this gate, entity structure and selection remain truthful across ECS, overlay, GOAP, Crowd, State Machine, A*, and Aggro without manual refreshes or sibling-module dependencies.

## Entry criteria

- [x] Baseline captured with named failing set.
- [x] Existing `CkDebug_Navigator`, `CkDebug_SelectionSync`, GOAP external entry, ECS refresh, and feature module teardown patterns inspected.
- [x] Owner-transfer mechanism traced from `Replace<FFragment_LifetimeOwner>` to the empty membership diff.

## Work items

1. Add an unload-safe `CkDebuggerCommon` entity-target route registry with generation tokens, `CanTarget`, and `OpenAndTarget` callbacks.
   -> verify: unit tests cover register/replace/stale-unregister/order/query and invalid input rejection.
2. Extend selection sync with a one-way ECS primary-selection provider that stores no handle in Common.
   -> verify: provider registration/clear and invalid selection make the common action disabled/no-op.
3. Add common `Sync from ECS` and inspector `Open In` widgets backed by the route registry.
   -> verify: widget availability follows provider/route state; no Common-to-feature dependency is introduced.
4. Register routes for GOAP, Crowd, State Machine, and A*. Reuse GOAP's `OpenForEntity`; add equivalent feature-owned resolution to the other three. Aggro remains intentionally unregistered because its report has no selected-entity model, and an open-only action would violate the targeting contract.
   -> verify: each route opens/focuses one existing tab and selects the closest lineage-compatible entity.
5. Broadcast overlay quick-select through `CkDebug_SelectionSync` after ECS navigation.
   -> verify: one broadcast, no receive echo, already-open GOAP/Crowd/SM scroll to their match.
6. Rebuild ECS hierarchy links for stable existing nodes whenever the observed cache revision changes.
   -> verify: owner A -> owner B transfer moves the same node pointer; selection/expansion survive; destroyed nodes are absent.

## Expected observations and branches

| Run | Expected | If instead | Response |
|---|---|---|---|
| Pure route/selection tests | generation safety and invalid-input no-op pass | stale callback or duplicate broadcast | stop and fix registry ownership before feature adopters |
| ECS hierarchy regression | same child node moves from A to B without full rebuild | pointer changes or stale child remains | extract/review relink order; do not add periodic ForceFullRefresh |
| `[EDITOR-VERIFY]` overlay quick-select | ECS plus already-open entity debuggers select/scroll matching lineage | only ECS moves | trace broadcast source and receiver route; do not add delays |
| `[EDITOR-VERIFY]` `Sync from ECS` | current tab adopts nested/owned match and reports no-match cleanly | unrelated same-root entity selected | reject root matching; inspect route-specific lineage candidate set |
| `[EDITOR-VERIFY]` inspector `Open In` | only capable buttons appear; click opens/focuses and targets | irrelevant buttons appear | tighten feature-owned `CanTarget` predicate |

## Exit criteria

- [x] Focused unit tests pass after final edit and fresh build.
- [x] `Debugger` baseline failing set remains empty.
- [x] Exact editor checks are recorded in `PROGRESS.md` as pending or confirmed.
- [x] Common/ECS/feature `CLAUDE.md` files describe the durable contracts.
- [x] `PLAN.md`, this status header, and `PROGRESS.md` update together.
