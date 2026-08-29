# Ck Navmesh Debug Draw — mission brief

> **Written:** 2026-08-28. Stable content only; current state lives in [PROGRESS.md](PROGRESS.md).
> **This doc dies when:** the packaged runtime visualizer ships and its permanent contracts live in `CLAUDE.md`.

## Goal

Provide an in-world Recast navmesh visualizer that works in packaged Development, DebugGame, and Test builds, appears progressively without a game-thread hitch, retains GPU geometry while navigation is unchanged, and performs no capture, ticking, component, or rendering work while disabled.

## Success criteria

1. `ck.Navmesh.DebugDraw 1` displays the active world's default Recast navmesh in editor and packaged non-Shipping builds.
2. `ck.Navmesh.DebugDraw 0` destroys all owned render components and unregisters every active-world observer/ticker.
3. Initial capture and navigation-generation refreshes are time-budgeted, nearest-first, and never perform an all-tiles gather.
4. Unchanged spatial buckets retain their existing scene proxies and GPU resources across refreshes.
5. Disabled-state instrumentation reports zero processed tiles, zero retained buckets, and no active work ticker.
6. Performance is reported as a same-machine packaged A/B comparison across at least three runs; visible GPU cost is measured rather than described as zero.

## Constraints and locked decisions

| Decision | Choice | Why |
|---|---|---|
| Product boundary | Development, DebugGame, and Test; inert in Shipping | CTO-confirmed 2026-08-28; matches CK's packaged-debug posture without exposing Shipping commands. |
| Ownership | New `CkNavmeshDebugDraw` Runtime module in CkGameplayDebugger | This is debug presentation, not CkNavigation gameplay state or the frozen UE GameplayDebugger integration. |
| Source access | Public `ARecastNavMesh` tile APIs under batch-query locking | Avoids editor/debug-menu dependencies and never retains Detour pointers. |
| Render lifetime | Immutable per-bucket component snapshots and proxy recreation only for changed buckets | Avoids per-frame dynamic draw and whole-city proxy rebuilds. |
| Activation | Lazy, command-driven, nearest-first, time-budgeted | Disabled worlds must not tick, capture, subscribe, or allocate render components. |
| UObject ownership | Weak world/nav references and strong component/material ownership | Raw stored pointers are prohibited; world cleanup must be fail-closed. |

## Non-goals

- Shipping-build access. Revisit only for a confirmed Shipping-only reproduction with a separate security review.
- Replacing Unreal navigation generation, pathfinding, or query filters.
- Extending the frozen Gen-1 UE GameplayDebugger category.
- Area labels, links, octree bounds, or poly-edge drawing in the first delivery; the walkable surface is the required vertical slice.
- Claiming zero GPU cost while visible.

## Reading list

- [PLAN.md](PLAN.md)
- `../CkEntityDebugOverlay/CkEntityDebugOverlay.Build.cs`
- `../CkCrowdDebugger/Public/CkCrowdDebugger/Data/CkCrowdDebugger_DataCollector.cpp`
- `../CkCrowdDebugger/Public/CkCrowdDebugger/Viewport/CkCrowdDebugger_3dSceneAdapter.cpp`
- `../../../CkFoundation/Source/CkDebugScene/CLAUDE.md`
- Engine `NavigationSystem` Recast and CustomMesh component sources resolved by the active project checkout

## Things ruled out — do not re-investigate

| Ruled out | Why | Evidence |
|---|---|---|
| Stock navmesh rendering component | Synchronous debug-data gathering and Shipping guards do not meet the packaged contract. | `ARecastNavMesh::UpdateNavMeshDrawing`; `UNavMeshRenderingComponent::GatherData`. |
| One invalid tile ref to gather everything | It explicitly walks every Detour tile. | `GetDebugGeometryForTile(..., FNavTileRef{})` and prior CK ~245 ms rebuild evidence. |
| Transient `UStaticMesh` construction for activation | Existing CK path measured ~194–245 ms whole-city rebuilds. | Crowd collector/adapter source comments. |
| Per-frame polling | It makes unchanged navigation pay forever. | Existing Crowd collector requires a one-second throttle and global hash. |
