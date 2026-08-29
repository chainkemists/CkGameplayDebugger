# Ck Navmesh Debug Draw — living progress log

## Current state

**As of 2026-08-29 (CkGameplayDebugger `6a33f696` plus the current working diff):** Gate 0 is complete. Gate 1 implementation is automated-green; live Recast rendering and teardown acceptance remain open.
**Baseline being diffed against:** no dedicated packaged in-world CK navmesh visualizer; the existing Crowd preview performs a throttled all-tile pull and records ~194–245 ms whole-city mesh rebuilds in source comments.
**Next action:** run the `[EDITOR-VERIFY]` Gate 1 observations on a representative multi-tile map, then execute Gate 2 packaged A/B acceptance on the build machine.
**Blocked on:** packaged Development/Test artifacts and representative-map performance captures are build-machine/manual acceptance work. No local cook, package, or Shipping build was authorized.

## Decision log

| Date | Decision | Why | Revisit when |
|---|---|---|---|
| 2026-08-28 | Compile behavior out of Shipping. | CTO-confirmed packaged boundary. | A bug is confirmed Shipping-only. |
| 2026-08-28 | Use 50 m retained buckets with custom mesh proxies. | Avoids both whole-city proxy recreation and per-frame dynamic drawing. | Measurements show component/draw-call cost dominates. |
| 2026-08-28 | Time-budget tile extraction and sort nearest-first. | First useful geometry should appear before a full-city scan completes. | Tile extraction itself exceeds the per-frame budget. |
| 2026-08-28 | Retry source discovery only while enabled, and retry one rejected tile scan once. Keep last-known-good geometry statically highlighted after persistent rejection. | Enabling before navigation readiness and transient tile invalidation must not expose the scene through bucket-sized holes. | Runtime evidence shows retry churn or the stale treatment obscures current geometry. |

## Dated entries

### 2026-08-28 — research and architecture gate

- Ran: read-only CK and engine archaeology through two delegated workstreams.
- Confirmed: the existing Crowd collector gathers all tiles once per second; the adapter retains unchanged 50 m buckets; `CkDebugScene` is Runtime and world-cleanup safe.
- Confirmed: public Recast APIs provide `GetAllNavMeshTiles`, per-ref `GetDebugGeometryForTile`, and batch-query locking.
- Confirmed: the CustomMesh engine exemplar owns retained vertex/index buffers in a scene proxy and recreates them through `MarkRenderStateDirty`.
- Inferred: time-budgeted per-tile capture plus retained bucket proxies will remove the observed activation hitch; Gate 2 measurements must confirm it.
- Follow-ups recorded, not chased: Shipping-only enablement and extra nav layers remain non-goals.

### 2026-08-28 — implementation and automated gates

- Added the `CkNavmeshDebugDraw` Runtime module, command-driven world subsystem, immutable retained mesh component, and non-Shipping compile boundary.
- Added deterministic nearest-first 50 m bucket capture using valid Recast tile refs, batch-query locking, a 0.5 ms per-frame capture budget with forward progress, content-signature retention, navigation-generation refresh, bounded source readiness polling, and one coalesced retry after a rejected tile.
- Verified Development Editor build: `Saved/Logs/Build-NavmeshDebugDraw-PostReview-20260828.log` reports `=== Build succeeded ===`.
- Verified all five focused automation rows across `Saved/Logs/Test-NavmeshDebugDraw-PostReview-20260828.log` (4/4) and `Saved/Logs/Test-NavmeshDebugDraw-Subsystem-Fresh-20260828.log` (1/1), with fresh-log inspection and only the expected invalid-input diagnostic.
- Verified Development Game target: `Saved/Logs/Build-NavmeshDebugDraw-Game-Development-20260828.log` reports 297/297 actions and `=== Build succeeded ===`; existing Jolt import warnings remain unrelated.
- Not yet verified: live RHI presentation, disable during an active real scan, dynamic-nav replacement behavior, packaged Development/Test availability, or N >= 3 A/B performance.

### 2026-08-28 — PIE tile-return contract repair

- Observed: every valid single-tile `GetDebugGeometryForTile` call could return `false`, triggering `GeometryWasGathered` even when geometry was appended.
- Root cause: Unreal documents and implements the return value as "done collecting," and only sets it for invalid-ref whole-mesh collection; it is not a per-tile success result.
- Repaired: ignore that continuation value for valid per-tile requests, distinguish stale refs with a fresh bounds check inside the same batch, and validate captured arrays through the existing atomic `TryAppendTileGeometry` boundary.
- Verified automatically: Development Editor rebuilt and all five focused tests passed in `Saved/Logs/BuildTest-NavmeshDebugDraw-TileContract-20260828.log`.
- Pending runtime verification: repeat `ck.Navmesh.DebugDraw on` in the reporting PIE map with zero `GeometryWasGathered` ensures and visible geometry.

### 2026-08-28 — area colors and retained topology edges

- Observed: the first proxy flattened every `AreaIndices` group to one green material and retained no topology edges, so modifier-volume semantics and polygon structure were unreadable.
- Added deterministic per-area fill batches using `GetAreaIDColor`, with Unreal's default-area config override and stock draw offset.
- Added white triangle topology as a retained GPU line-list batch. Edges are derived during the existing index pass; the explicitly slow Recast `bGatherPolyEdges` traversal and per-frame per-edge `DrawLine` path remain unused.
- Verified: Development Editor build succeeded and fresh discovery ran 7/7 focused tests, including color-only and edge-only signature/proxy replacement coverage, in `Saved/Logs/BuildTest-NavmeshDebugDraw-AreaEdges-20260828.log`.
- Verified after adversarial review: the final combined fill/edge material relevance change compiled in `Saved/Logs/Build-NavmeshDebugDraw-AreaEdges-Relevance-20260828.log`; no additional editor boot was used.
- Pending visual acceptance: confirm high-contrast topology lines and the authored avoidance-area colors in the reporting PIE map.

### 2026-08-28 — stable unlit fill and atomic bucket replacement

- Confirmed: `DebugMeshMaterial` is unlit and translucent; the apparent lighting came from blending with the lit scene beneath it.
- Removed reversed-winding duplicates and enabled mesh-level backface-culling suppression, eliminating coplanar double blending while retaining two-sided visibility.
- Normalized every semantic area color to alpha 160 so default and authored areas share one predictable overlay opacity.
- Replaced the scan-global rejection counter with per-bucket retry state. A transient stale/empty capture keeps last-known-good geometry for one retry; persistent failure or completed-scan absence removes it.
- Verified: Development Editor build succeeded and 7/7 focused tests passed in `Saved/Logs/BuildTest-NavmeshDebugDraw-StableFill-20260828.log`.
- Fresh-log caveat: the editor reported one unrelated `CachedAssetRegistry_0.bin.tmp` write failure; no compiler, linker, AngelScript, navmesh ensure, or focused-test failure accompanied it.
- Pending visual acceptance: rerun the 100-volume stress case and confirm stable opacity with no bucket-sized white flashing.

### 2026-08-28 — retained stale-bucket presentation

- Replaced persistent-failure removal with a last-known-good stale state: rejected or empty refreshes retain the bucket, while teardown still removes it.
- Added a static stale treatment: subtly amber-tinted and faded fill plus amber topology edges. It changes only when bucket state changes and adds no animation or per-frame CPU work.
- Source loss or mid-scan nav-data invalidation marks every retained bucket stale; a successful snapshot, including identical geometry, atomically restores the normal presentation.
- Added `stale_buckets` to `ck.Navmesh.DebugDraw status` and automation coverage for idempotent stale transitions, invalid-snapshot atomicity, and stale clearing on identical valid content.
- Verified: Development Editor build succeeded, fresh discovery found 8 focused tests, and 8/8 passed in `Saved/Logs/BuildTest-NavmeshDebugDraw-StaleHighlight-20260828.log`.
- Pending visual acceptance: repeat the 100-volume stress case and confirm changed buckets remain visible with a subtle amber/faded treatment instead of exposing white floor.

### 2026-08-29 — completed-scan absence tombstones

- Live evidence still showed bucket-sized ground holes after rejected captures, source loss, and stale rendering had all been covered.
- Discriminating trace found one remaining enabled-state removal path: `DoFinishScan` destroyed every retained component omitted from a single `GetAllNavMeshTiles` enumeration, bypassing stale presentation.
- Changed completed-scan reconciliation to retain unseen buckets as static faded/amber stale tombstones. A later valid snapshot restores the same component; only explicit disable or world teardown destroys it.
- Added focused subsystem coverage that publishes a bucket, reconciles an empty seen-key set, verifies one retained/one stale bucket, then republishes the same key and verifies one retained/zero stale buckets.
- Verified: Development Editor build succeeded, fresh discovery found 9 focused tests, and 9/9 passed in `Saved/Logs/BuildTest-NavmeshDebugDraw-UnseenFade-20260829.log`.

### 2026-08-29 — partial same-bucket coverage repair

- Live evidence still showed disappearing regions after rejected, empty, and wholly unseen buckets all retained stale geometry.
- Root cause: a dynamic scan could enumerate only part of a still-seen bucket. Every returned tile and the resulting nonempty snapshot were valid, so the smaller snapshot replaced the complete retained mesh and exposed the omitted tile regions.
- Added stable source coverage from `GetNavMeshTileXY(TileRef, X, Y, Layer)`. Existing buckets now reject any candidate missing a previously published coordinate, retain the complete mesh as stale, and schedule the bounded retry. Regenerated refs at the same stable coordinates remain eligible for normal replacement.
- Red proof: `Saved/Logs/BuildTest-NavmeshDebugDraw-PartialCoverage-RED-20260829.log` found 10 focused tests with exactly `PartialTileCoverageRetainsPreviousSnapshot` failing; the bad path accepted the partial snapshot, reduced triangle coverage 2→1, advanced revision 7→8, and left stale count at zero.
- Green proof: `Saved/Logs/BuildTest-NavmeshDebugDraw-PartialCoverage-GREEN-20260829.log` rebuilt successfully, reran that failed row successfully, and finished with 10/10 focused tests passing.
- Pending visual acceptance: repeat the 100-volume stress case and confirm no previously visible tile region is replaced by a partial same-bucket snapshot.

### 2026-08-29 — removed-polygon ghost layer

- Runtime status sampling proved all four bucket components stayed retained with `stale_buckets=0` while proxy replacements climbed. The apparent holes were valid current snapshots removing polygons inside stable tile coordinates, so bucket-level stale handling could not represent the change.
- Added a separate removed-polygon ghost layer inside each retained mesh component. Successful snapshots diff canonical 1 mm-quantized triangle geometry independent of winding and area color; removed triangles render as faint amber fill plus amber edges, and reappearance removes the ghost.
- Ghost history persists without component ticking or animation and is bounded to 4,096 triangles per bucket / 65,536 subsystem-wide. `ck.Navmesh.DebugDraw status` now reports `ghost_triangles`.
- Added focused lifecycle and color-only tests. Development Editor build succeeded, fresh discovery found 12 rows, and 12/12 passed in `Saved/Logs/BuildTest-NavmeshDebugDraw-PolygonGhosts-20260829.log`.
- The initial persistent amber treatment was rejected visually because it was too bright and accumulated overlapping history; the correction below supersedes it.

### 2026-08-29 — bounded dark ghost fade

- Replaced the bright amber persistent ghost treatment with dark, desaturated fill and edges, lifted 1 cm above the navmesh to avoid z-fighting.
- Each retained triangle now owns its fade step. A dedicated 0.5-second subsystem ticker advances four steps and deletes the triangle after approximately two seconds; the ticker unregisters itself when the final ghost expires.
- Existing ghosts retain their age across later snapshots, while reappearance removes the matching ghost immediately. This prevents repeated rebuilds from resetting old marks or accumulating permanent overlapping history.
- Added focused expiry and age-preservation coverage. The Development Editor build succeeded and all 13 focused tests passed in 1m 15s (`Saved/Logs/BuildTest-NavmeshDebugDraw-GhostFade-20260829-v2.log`), with no compiler, linker, AngelScript, or focused-test errors.
- Pending visual acceptance: rerun the 100-volume stress case and confirm ghosts are dark/muted, visibly step down, and disappear after approximately two seconds without exposing a long-lived white hole.
- Visual follow-up: the unlit translucent debug material composited the initial `0.38` fill alpha too close to the light floor. Kept the dark hue, edge treatment, and four-step lifetime unchanged; raised only the fill-opacity curve to `0.88 / 0.62 / 0.36 / 0.16`. The incremental Development Editor build succeeded in `Saved/Logs/Build-NavmeshDebugDraw-DarkInitialFill-20260829.log`; pending controlled visual comparison.

### 2026-08-29 — transactional refresh and continuous fade

- Runtime feedback clarified that the mesh starts complete, then flashes holes while agents trigger navigation rebuilds. The cause was per-bucket publication from an in-flight scan: another nav generation could invalidate the scan after some intermediate replacements were already visible.
- Every scan now stages completed bucket snapshots without touching retained components. Active/queued navigation generation blocks capture and commit; a generation event during capture invalidates and discards the entire transaction. Only a stable completed scan updates existing buckets and reveals prepared new buckets.
- Existing buckets use a side-effect-safe preflight followed by an infallible validated apply, so the transaction has no late recoverable failure after mutation begins. Invalidated scans preserve previous revisions, geometry, visibility, stale state, and ghost history.
- Replaced four 0.5-second opacity jumps with render-time smoothstep interpolation from one shared app-real-time clock. Birth times use 0.25-second cohorts (at most eight active cohorts over two seconds), and one delayed ticker rebuilds a component only when a cohort expires.
- Development Editor build succeeded and all 16 focused tests passed in 56 seconds (`Saved/Logs/BuildTest-NavmeshDebugDraw-TransactionalSmoothFade-20260829-v2.log`). Coverage includes staged initial publication, staged replacement, invalidated-scan discard, pre/post expiry, age preservation, reappearance, stale tombstones, partial coverage, and retry teardown.
- Pending visual acceptance: rerun the 100-volume stress case and confirm the previously complete mesh stays visible throughout agent-driven nav generation, stable replacements appear together, and genuine removals fade continuously without opacity steps.

### 2026-08-29 — generation-storm quiet-period coalescing

- Video and temporary runtime telemetry disproved the remaining partial-publication hypothesis. The 100-volume stress case produced approximately 130 navigation-generation revisions in 15 seconds; Unreal repeatedly reported navigation as momentarily stable between events, so whole but transient scans were committed and created hundreds of overlapping ghosts.
- Navigation events now reset a two-second one-shot quiet period instead of immediately scanning. The last committed mesh remains unchanged throughout the event storm; after navigation has stayed quiet and is not building, one whole scan runs and genuine removals enter one smooth fade.
- The settle callback is delayed and resettable rather than per-frame. Explicit `ck.Navmesh.DebugDraw refresh` remains immediate, disable cancels pending settle work, and `status` exposes `settling` plus `settle_ticker`.
- Removed the temporary generation/transaction/ghost-expiry telemetry after it established the mechanism. Development Editor build succeeded and all 17 focused tests passed in 58 seconds (`Saved/Logs/BuildTest-NavmeshDebugDraw-NavigationSettle-20260829.log`), including settle reset, delayed publication, ticker release, and disable teardown. Live visual acceptance remains pending.

### 2026-08-29 — live non-destructive area preview

- Quiet-period coalescing eliminated disappearing surfaces but delayed modifier-volume colors until the final authoritative scan. Recoloring retained triangles alone was rejected because Recast area modifiers can retessellate the surface, so the current colored polygons may not share the committed triangle keys.
- During settling, a resettable preview callback now samples current Recast geometry at most twice per second using the existing 0.5 ms frame budget. It derives no topology edges and publishes only non-default area triangles into a fill-only layer lifted 1.5 uu above the unchanged authoritative surface. Publication is capped to the eight nearest retained buckets per pass so large-map proxy recreation cannot become an unbounded end-of-scan burst.
- Preview publication cannot mutate base area meshes, edges, ghosts, revisions, stable tile coverage, or stale state. A successful authoritative transaction clears every preview in the same proxy replacement, including retained unseen tombstones.
- `status` now reports `preview_scan`, `preview_ticker`, and `preview_triangles`. Development Editor build succeeded and all 20 focused tests passed in 54 seconds (`Saved/Logs/BuildTest-NavmeshDebugDraw-LiveAreaPreview-20260829-v3.log`), including invalid-preview rejection, authoritative-state preservation, default-only clearing, eight-bucket publication bounding, and atomic authoritative replacement. Live visual acceptance remains pending.

## Open items

| Item | Status | Next step |
|---|---|---|
| Gate 0 implementation | Done | Automated component/lifecycle coverage and Editor/Game compile gates are green. |
| Gate 1 streaming | In progress | Perform the exact `[EDITOR-VERIFY]` observations below on a representative multi-tile map. |
| Gate 2 evidence | Pending | Measure packaged A/B and perform Development/Test packaged live acceptance. |

**Rule:** no completion claim may be written while any row above is unresolved.

## [EDITOR-VERIFY] Gate 1 observations

1. In PIE or standalone on a representative multi-tile Recast map, run `ck.Navmesh.DebugDraw on`; confirm nearby buckets appear progressively, white topology edges are readable, modifier-volume areas use their authored nav-area RGB at consistent opacity, and dynamic rebuilds do not expose flashing bucket-sized holes.
2. Run `ck.Navmesh.DebugDraw status`, then `refresh`; confirm the completed refresh does not visibly churn unchanged buckets and `stale_buckets` returns to zero after successful capture.
3. Trigger dynamic navigation generation; confirm the last committed mesh remains unchanged while navigation is dirty/building, the next stable scan appears as one transaction, and genuinely removed polygons fade continuously over approximately two seconds.
4. Run `ck.Navmesh.DebugDraw off` during a scan; confirm drawing disappears immediately and `status` reports no active ticker, capture, or retained buckets.
