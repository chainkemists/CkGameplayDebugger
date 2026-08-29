# CkNavmeshDebugDraw

`CkNavmeshDebugDraw` is the packaged non-Shipping, in-world Recast surface visualizer. It owns presentation only; navigation generation and gameplay queries remain in Unreal NavigationSystem and CkNavigation.

## Permanent boundaries

- Runtime module, behavior compiled under `WITH_CK_NAVMESH_DEBUG_DRAW`; Shipping must remain inert.
- Activation is command-driven. Disabled worlds own no ticker, navigation delegate, render component, or geometry snapshot.
- The command surface is `ck.Navmesh.DebugDraw [on|off|toggle|refresh|status]` and resolves the console world without creating navigation data.
- Copy Recast values under `BeginBatchQuery`/`FinishBatchQuery`. Never retain Detour pointers, Recast arrays, raw UObject pointers, or source-world references inside render data.
- `GetDebugGeometryForTile` returns whether collection is complete, not whether a valid tile produced geometry. Ignore that continuation value for single-tile requests and validate the resulting arrays instead.
- Because scan enumeration and tile capture occur in separate batches, revalidate each tile ref through its bounds inside the capture batch; only an unresolved ref is stale and eligible for the bounded rescan path.
- Track successfully published source coverage by stable Recast `(TileX, TileY, Layer)` coordinates, never generation-dependent tile refs. A candidate for an existing bucket must retain every previously published coordinate; otherwise keep the last-known-good mesh stale and retry instead of publishing a partial hole.
- Render immutable 50 m spatial buckets. Recreate only buckets whose deterministic content signature changes.
- Preserve `FRecastDebugGeometry::AreaIndices` as deterministic per-area batches. Resolve colors through `ARecastNavMesh::GetAreaIDColor`, including Unreal's default-nav-config color override, so modifier-volume area colors survive into the retained proxy.
- Fill uses Unreal's packaged `DebugMeshMaterial` (`MSM_Unlit`, `BLEND_Translucent`). Normalize semantic area RGB to alpha 160, retain one triangle winding, and set `bDisableBackfaceCulling`; never duplicate a translucent surface to make it two-sided.
- Derive triangle-edge pairs during the existing area-index pass and render them as one retained GPU `PT_LineList` batch per bucket. Do not enable Recast's slow `bGatherPolyEdges` walk or issue per-edge `DrawLine` calls every frame.
- Apply Unreal's navmesh draw offset, including the agent-radius separation when configured, equally to fill and edges.
- Sort buckets nearest-first with a stable key tie-break, and sort tile refs inside each bucket so unchanged navigation produces identical content.
- If enabled before the default Recast navmesh exists, poll only while enabled at a bounded interval. Source loss marks retained geometry stale and permits bounded rediscovery without exposing holes.
- Bucket capture and completed-scan reconciliation are atomic per key: rejected, empty, or unseen buckets retain last-known-good geometry as stale. A later successful replacement clears staleness; only explicit disable or world teardown removes retained geometry.
- Present stale geometry as a static, subtly amber-tinted and faded fill with amber edges. Staleness must not animate or add per-frame CPU work, and `status` must report the stale bucket count.
- Diff successful snapshots by canonical 1 mm-quantized triangle geometry, ignoring winding and area color. Triangles removed from otherwise valid buckets enter a separate dark, muted ghost fill/edge layer; color-only changes create no ghosts and reappearance removes the matching ghost immediately.
- Stage every budgeted scan as copied value data. If navigation is still building, becomes dirty, or completes another generation during capture, discard the entire staged scan and retain the last committed mesh. Apply a completed stable scan as one game-thread transaction; never expose per-bucket intermediate replacements.
- Coalesce navigation-generation storms with a resettable two-second one-shot quiet period. Retain the last committed mesh throughout settling, start one refresh only after no new generation event arrives and navigation is not building, and keep explicit console refresh immediate.
- While navigation is settling, sample current Recast geometry at most twice per second through the existing 0.5 ms work budget and publish only non-default area triangles as a fill-only preview layer. A preview pass may span generation events because it is explicitly non-authoritative, but each tile is still validated and incomplete buckets are rejected. Publish at most eight nearest retained preview buckets per pass, clear a bucket when a valid preview becomes default-only, and never replace base geometry, edges, ghosts, revisions, tile coverage, or stale state; the next successful authoritative transaction clears every preview atomically.
- Give each removed-triangle ghost a quantized birth time and interpolate fill/edge alpha continuously from the shared app-real-time clock for approximately two seconds. Use one delayed subsystem callback only to remove the earliest expired cohort, never a component or per-frame game-thread tick. Cap retained ghosts at 4,096 triangles per bucket and 65,536 across the subsystem; `status` reports both the expiry ticker and ghost-triangle total.
- World cleanup and disable are fail-closed: remove observers/tickers first, then destroy components and copied state.
- Performance wording requires same-machine packaged A/B evidence across at least three runs. Visible GPU cost is never described as zero.

## Campaign

Active implementation state lives in [PROGRESS.md](PROGRESS.md). Delete or tombstone the campaign documents when all gates ship.
