# Gate 1 — tile capture and bucket streaming

> **Status:** In progress: implementation automated-green; live acceptance pending
> **Depends on:** Gate 0 complete

## Goal

After this gate, enabling the command renders nearby navmesh buckets first, progressively completes the world without an all-tiles gather, retains unchanged bucket proxies, and coalesces navigation refreshes.

## Entry criteria

- [x] Gate 0 exit observations reverified on the current working diff.
- [x] Per-tile Recast extraction implemented under batch-query locking.

## Work items

1. Resolve the default `ARecastNavMesh` without creating navigation data.
2. Enumerate valid tile refs and sort them by bounds distance to the active view.
3. Extract one tile at a time under `BeginBatchQuery`/`FinishBatchQuery` and append value-only single-winding triangles to 50 m buckets; the retained mesh batch disables backface culling without duplicating translucent surfaces.
4. Publish a bucket only after all tile refs assigned to it have been processed; compare content signatures before proxy replacement.
5. Bind only while active to navigation-generation completion, coalesce refresh requests, and remove missing buckets after a completed scan.

## Expected observations

| I will run | I expect to observe | If instead I see | Response |
|---|---|---|---|
| Enable with a multi-tile test navmesh | Nearest completed bucket appears before the full scan ends, with readable topology edges and authored nav-area colors. | Nothing appears until all tiles finish, edges are absent, or area colors are flattened | Fix bucket publication/render-data preservation before performance work. |
| Refresh unchanged navigation | Zero bucket proxy replacements. | Any bucket rebuilds | Audit deterministic ordering/signatures before continuing. |
| Dynamic rebuild stress | Dirty/building/invalidated scans retain the entire last committed mesh; non-default area polygons may update only through the non-destructive preview layer; one stable completed scan commits transactionally, and genuine removals fade continuously for approximately two seconds. | Any preview replaces base topology, per-bucket intermediate replacement flashes, white holes appear during generation, or ghost history accumulates indefinitely | Audit preview throttling/isolation, navigation-build gating, scan invalidation, transactional commit, canonical triangle diffing, and ghost budgets/expiry. |
| Disable during scan | Ticker, delegate, pending values, and components all clear. | Callback/work survives | Treat as a lifetime blocker. |

## Exit criteria

- [x] Focused lifecycle/content tests green (16/16), including transactional staging/replacement, invalidated-scan discard, partial same-bucket coverage, and smooth-fade expiry lifetime.
- [ ] Live in-world observations completed using the exact `[EDITOR-VERIFY]` steps in `PROGRESS.md`.
- [x] Index, gate, progress, and permanent module doctrine updated together.
