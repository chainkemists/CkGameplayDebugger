# Gate 1 — Authoritative runtime controls and BusterBlock policy

> **Status:** Complete; direct listen-server replication test passed
> **Depends on:** Gate 0

## Goal

After this gate, every debugger can change replicated authority-world speed, and the AI Overview can list session-only behavior overrides including BusterBlock’s NPC stuck-recovery suppression.

## Work items

1. Add Common authority-world resolution and a total `Try_SetWorldSpeed` API with pure truth-table tests.
2. Add shared `SCkDebug_WorldSpeedControl` to WindowChrome with `0.1x`, `0.25x`, `0.5x`, and `1x`.
3. Add a generation-safe Common behavior-override descriptor registry and shared panel/row widgets.
4. Add transient `UBb_NpcDebugPolicySubsystem`; register its override from BusterBlock without storing raw UObject pointers.
5. Gate the stuck processor before all stuck/recovery mutations; while suppressed, clear tracking and write a clean non-stuck state.
6. Add invalid/client/direct no-motion coverage and fresh AngelScript log checks.

## Expected observations

| Run | Expected | If instead | Response |
|---|---|---|---|
| Pure authority matrix | Standalone/listen/dedicated accepted, client/missing world rejected | client accepted | block Gate 1; fix common authority gate |
| Listen-server Development | host selection changes server and clients; client cannot mutate | only host local clock changes | verify selected WorldSettings replication and chosen world identity |
| Forced no-motion NPC | suppression off preserves current recovery; on emits no recovery; off starts a fresh timer | immediate stale trigger after off | reset every episode/timer field during suppression |

## Exit criteria

- [x] Common control is injected by every WindowChrome consumer and reflects authoritative readback.
- [x] BusterBlock override is non-persistent and authority-gated.
- [x] Focused C++/AS tests and fresh logs pass.
- [x] Direct multi-PIE listen-server/client set, replication, and reset evidence recorded.
- [x] PLAN, status, PROGRESS, and permanent docs updated together.
