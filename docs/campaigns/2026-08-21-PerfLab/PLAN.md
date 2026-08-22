# PerfLab — executive index (PLAN.md)

> **Written:** 2026-08-21. Status column updates land in the **same commit** as each phase's last work
> item (ck-methodology §7 — a stale index is worse than none). Volatile state: [PROGRESS.md](PROGRESS.md).

Mission: [PROMPT.md](PROMPT.md). Research: `RESEARCH_{Codebase,Competitive,EngineApis}.md`.
Acceptance: [VALIDATION.md](VALIDATION.md). Executor start prompt:
[CONTINUATION_PROMPT_Executor.md](CONTINUATION_PROMPT_Executor.md).

One phase ≈ one executor session. Every phase exits through the `ck-change-control` checklist for its
change class, with gate counts diffed against the baseline recorded at its entry.

| Phase | Name | Touches | Class | Status |
|---|---|---|---|---|
| [0](PHASE_0.md) | Verification spike and contract lock | scratch only + campaign docs | 1 | ✅ Done (2026-08-21) |
| [1](PHASE_1.md) | Timing surface in CkProfile | `CkFoundation/Source/CkProfile` | 2 | ✅ Done (2026-08-21) |
| [2](PHASE_2.md) | CkPerfLab module: session model, stats, codec | new `CkGameplayDebugger/Source/CkPerfLab` | 2 | ✅ Done (2026-08-21) |
| [3](PHASE_3.md) | Position planner | `CkPerfLab` | 2 | ✅ Done (2026-08-21) |
| [4](PHASE_4.md) | In-child measurement runner | `CkPerfLab` | 2 | ✅ Done (2026-08-22) |
| [5](PHASE_5.md) | Host orchestration (subprocess + session store) | `CkPerfLab` | 2 | ✅ Done (2026-08-22) |
| [6](PHASE_6.md) | Analysis: score, perf rules, contributors, recommendations | `CkPerfLab` (debugger check-family integration deferred to Phase 7) | 2 | ✅ Done (2026-08-22) |
| [7](PHASE_7.md) | Performance page UI | `CkOptimizationDebugger` (+ launcher catalog spec) | 3 | ⏳ Pending |
| [8](PHASE_8.md) | Viewport heatmap EdMode | new `CkOptimizationDebuggerEditor` | 2 | ⏳ Pending |
| [9](PHASE_9.md) | Compare, exports (HTML/CSV/JSON), CI entry, docs & close-out | `CkPerfLab` + `CkOptimizationDebugger` | 3 | ⏳ Pending |

Dependency shape: 0 → 1 → 2 → {3,4} → 5 → 6 → {7,8} → 9. Phases 3 and 4 may interleave (planner is
pure logic; runner consumes it), but land 3 first. Phases 7 and 8 are independent of each other.

## Standing gates (every phase)

- Baseline at entry: `UnrealToolbox --test --test-pattern <affected roots>` counts + failing names
  recorded in PROGRESS.md **before** the first edit.
- Exit: same invocation after the final edit **on the final binary** (stale-green trap —
  `ck-verification` doctrine), delta reported as `baseline N failing {names} → now M {names}`.
- OptimizationDebugger's own gate (`Ck.OptimizationDebugger`, 76/76 at campaign start) re-runs in any
  phase that touches the module (6, 7, 9) and in Phase 9 regardless.
- Same-commit doc updates: this table's Status cell + the phase file's Status header + PROGRESS.md
  dated entry.

## Post-ship cleanup (plan for deletion from day one)

Gate/phase files and RESEARCH_* are disposable after ship; the permanent survivors are
`Source/CkPerfLab/CLAUDE.md`, `Source/CkOptimizationDebugger/CLAUDE.md` (amended), and
`Source/CkOptimizationDebuggerEditor/CLAUDE.md`. PROMPT.md gets a tombstone line.
