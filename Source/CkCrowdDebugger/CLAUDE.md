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
- Queue data comes only from CkQueue's detached `Get_DebugSnapshots` surface. Keep queue origins, reservations,
  per-origin formation links, member-to-slot links, and queued-agent rank/category as copied values; the Crowd
  debugger must never retain a queue or member handle.

## Verification

- `Ck.CrowdDebugger.Viewport3d.*` covers copied lifetimes, stable identity reorder, exact pick mapping, bounds,
  ribbons, Voxel content, atomic failure, appearance ownership, and 240-agent stable instancing.
- Re-run the full serial `Crowd` family after adapter changes. The current CkPlugins user config has an inherited
  `Nav.Filter.Customer` mapping failure; do not hide new failures behind that known baseline.
