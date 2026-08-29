# Ck Navmesh Debug Draw — gate index

> **Written:** 2026-08-28. Update this index and the active gate in the same change.
> **This doc dies when:** all gates are complete and permanent contracts have moved to `CLAUDE.md`.

| Gate | Scope | Status |
|---|---|---|
| [Gate 0](Plan/Gate_00_RuntimeSurface.md) | Runtime module, immutable retained mesh component, lazy command/lifecycle | Done (2026-08-28) |
| [Gate 1](Plan/Gate_01_TileStreaming.md) | Per-tile capture, nearest-first bucket streaming, event-driven refresh | In progress: automated green, live acceptance pending |
| [Gate 2](Plan/Gate_02_PackagedPerformance.md) | Focused tests, A/B performance, packaged acceptance | Pending |

## Locked architecture

`ARecastNavMesh` tile values are copied on the game thread under batch-query locking. The producer groups copied triangle soup into stable 50 m world buckets. Each bucket owns one immutable custom mesh component snapshot and one retained scene proxy. Activation and rescans process a bounded amount of work per frame. Only content-signature changes recreate a bucket proxy.
