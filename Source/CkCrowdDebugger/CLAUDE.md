# CkCrowdDebugger

## 3D viewport ownership

- `SCkCrowdDebugger_3dViewport` is a thin feature facade over Common's `SCkDebug_3dPreviewViewport`. Do not add a
  Crowd-owned `FPreviewScene`, viewport client, camera implementation, or input router.
- The window/view model may collect live Crowd, Queue, Recast, VoxelNav, and PathNetwork state, but the viewport boundary
  copies it into `FCkCrowdDebugger_3dSceneSnapshot`. Scene/preview adapters retain values and opaque `uint64`
  identities only; never store a gameplay `UWorld`, actor, ECS handle/registry, navmesh, or VoxelNav producer.
- `FCkCrowdDebugger_3dSceneAdapter` owns Crowd-specific translation: capsule/status appearance, velocity and selected
  paths, Recast triangles, width-aware PathNetwork ribbons and opacity, Voxel layers/bounds/chunks/portals, and
  current-row pick mapping. Reusable component/material/reconciliation/picking mechanics belong in `CkDebugScene`.
- Recast and PathNetwork fills use the shared Unlit translucent CkDebugScene material with two-sided sanitized
  geometry. Recast remains at world depth/sort 0; PathNetwork preserves the legacy foreground pass with a stable
  positive sort priority. Never substitute Engine `M_Simple*` materials, which lack the retained ISM shader usage
  contract.
- Preserve the `ck.CrowdDebugger.PathNetworkTrace` compatibility token and all specialized source/CVar/detail-panel
  controls when changing the adapter. Common controls are capability-driven and must not be recreated in the Crowd
  window.
- The `GroundNavField` role draws a whole GroundNav bake from `FCkCrowdDebugger_GroundNavField` - the
  debugger's own copy of `FCk_GroundNav_DebugSnapshot`, held as the snapshot cache's immutable
  `TSharedPtr<const>` beside the cache key it was captured under and the source it came from. Null is
  NO FIELD, not an empty one, and submits nothing. `_Plates` feeds the plate outlines and `_Boundary` feeds
  the boundary runs, read straight off the copy so the overlay and CkGroundNav's own Plates/Boundary
  modes cannot drift apart. A field that is not drawable submits no geometry: failure is a status the
  copy carries, never a shape. Item identity is the plate's or run's own geometry, so a rebake that
  only reorders the arrays costs nothing; `_Revision` is the producer's change stamp and moves when
  the cache key does.
- Queue data comes only from CkQueue's detached `Get_DebugSnapshots` surface. Keep queue origins, reservations,
  per-origin formation links, member-to-slot links, and queued-agent rank/category as copied values; the Crowd
  debugger must never retain a queue or member handle.

## Navmesh status panel

- `SCkCrowdDebugger_NavmeshStatusPanel` renders a provider-neutral header first - provider,
  provider health, surface revision, surface bounds - read from `UCk_Utils_NavSurface_UE` for every
  world, then a Recast detail section (NavSystem / NavData / Filter / Supported Agents) only while
  Recast is the provider answering. The collector gates its Recast pull the same way, so a GroundNav
  world reports that provider's own health and revision instead of a missing navmesh.

## Shadow parity panel

- `SCkCrowdDebugger_ShadowParityPanel` renders `FFragment_GroundNav_ShadowDiagnostics` - active
  fixture, fixture count, aggregate agreement, diverging query ids, and a per-fixture breakdown -
  from `FCkCrowdDebugger_ShadowParity`, a debugger-side value holding the fragment BY VALUE beside a
  `_Sampled` flag. The flag is what separates "a world was read and holds no diagnostics" from "no
  world was read"; an empty fragment cannot say which.
- The value is pushed in through `Set_Parity` rather than pulled per attribute read: it carries a map
  and an array, and a bound attribute would copy both every repaint. Every row is a static taking the
  value, so what the panel shows for a run is readable back with no world and no Slate tree.
- The collector fills both. The field copy is taken through its own `FCk_GroundNav_DebugSnapshotCache`
  only while `Get_Provider` answers GroundNav, off the volume nearest the selection (else the first
  that has published), and only when the cache key moved; `_GroundNavRevision` is DERIVED from that key
  by `Get_GroundNavRevisionFromKey`, so a key change can never leave the overlay stale. The shadow
  parity value is copied off the world's transient entity when it carries
  `FFragment_GroundNav_ShadowDiagnostics`. Both are still rendered from the value alone, which is what
  makes the role and the panel checkable with no world behind them.

## Verification

- `Ck.CrowdDebugger.Viewport3d.*` covers copied lifetimes, stable identity reorder, exact pick mapping, bounds,
  ribbons, Voxel content, atomic failure, appearance ownership, and 240-agent stable instancing.
- Re-run the full serial `Crowd` family after adapter changes. There is no inherited baseline failure to work
  around: the `Viewport3d.*` family was 19/19 with zero failures before the two `GroundNavField*` pins below, and
  the `Nav.Filter.Customer` tag exists and its consumer is green. Any red is new.
- `Ck.CrowdDebugger.NavmeshStatus.*` pins the header/detail split from a status value alone, with no world.
- `Ck.CrowdDebugger.Viewport3d.GroundNavField*` pins the copy outliving its producer, the non-drawable
  status drawing nothing, and identity surviving a reordered plate/boundary array across a rebuild.
- `Ck.CrowdDebugger.ShadowParity.*` pins every row from a diagnostics fragment accumulated in
  isolation, and pins that an unread world never reads as a clean run.

## Cross-debugger reuse

`FCkCrowdDebugger_ViewModel` and `SCkCrowdDebugger_3dViewport` are exported reuse surfaces for concise overview tools.
Both publish/consume value-only snapshots. Keep live fragment/world ownership in the collector/view-model and never copy
the Crowd collector, preview adapter, or scene adapter into another debugger.
