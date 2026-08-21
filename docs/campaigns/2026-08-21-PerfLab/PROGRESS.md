# PerfLab — PROGRESS.md (living log)

## Current state  <!-- supersedes everything below; update at EVERY phase boundary and session end -->
**As of 2026-08-21 (planning session, no code):** Campaign planned; Phase 0 not started.
**Branches:** none yet — Phase 0 entry creates `perflab/phase-0` in CkGameplayDebugger + CkFoundation
(local only, never pushed).
**Baseline being diffed against:** NOT YET CAPTURED — Phase 0 entry captures the full-suite counts +
failing names. Known module baseline: `Ck.OptimizationDebugger` 76/76 (P18, 2026-08-21, per its
PLAN.md — re-verify at Phase 0 entry, do not trust this row as a run).
**Next action:** Executor session 1 runs Phase 0 per [PHASE_0.md](PHASE_0.md), starting with the
entry criteria.
**Blocked on:** nothing.

## Decision log
| Date | Decision | Why | Revisit when |
|---|---|---|---|
| 2026-08-21 | D1–D12 locked in PROMPT.md (augment; `-game` child; stat-unit globals; presets; enum-availability; harmonic score; 10 evidence-gated rules; path-based contributors; EdMode heatmap; versioned JSON; local branches; toolbox-only builds) | See PROMPT.md table + rejected-approaches | A rejected row's kill reason stops being true |
| 2026-08-21 | Subprocess utility lives private to CkPerfLab (interim) | Adjudication A-PerfLab-1 — maintainer rules on CkCore placement | Adam rules, or a second consumer appears |
| 2026-08-21 | Deep mode ships in v1 as a preset (not deferred) | Modes reduce to parameters over one runner (D4) | — |
| 2026-08-21 | v1 rule catalog = 10 (not 24) | Evidence-gated quality over count parity; family contract makes growth mechanical | Post-ship feature rounds |

## Dated entries (append-only, newest first)

### 2026-08-21 — Campaign planned (Fable session)
- Produced: PROMPT.md, PLAN.md, PHASE_0–9, RESEARCH_{Codebase,Competitive,EngineApis}.md,
  VALIDATION.md, CONTINUATION_PROMPT_Executor.md, this file.
- Confirmed (planner-read): engine timing globals `RenderTimer.h:105-129`;
  `RHIGetGPUFrameCycles` + `GGPUFrameTime` deprecated-5.6 (`RHIGlobals.h`/`RHICommandList.h`);
  OptimizationDebugger PLAN.md P7/P9/P12/P18 rows + 76/76 gate claim; capability review
  (2026-08-15) in full incl. resolutions table; campaign-dir precedent; no-live-handle §
  at `CkOptimizationDebugger/CLAUDE.md:44`.
- Inferred (agent-read, spot-checks pending in Phase 0): all remaining file:line cites in the
  RESEARCH docs; `FStatUnitData` expression shapes; streaming/navmesh API details; GitLink
  subprocess internals.
- Follow-ups recorded, not chased: JSON/CSV findings export for the STATIC checks (P18 shipped
  HTML/MD only — Gap 1's remainder); naming-convention family (Gap 2); `.utrace` composition (v2).

## Open items
| Item | Status | Next step |
|---|---|---|
| Full-suite baseline capture | Open | Phase 0 entry |
| Adjudication A-PerfLab-1 (subprocess placement) | Open — interim stance active | File row in Phase 5; Adam rules |
| All `[EDITOR-VERIFY]` items | Open | Accumulate in VALIDATION.md; Adam runs at Phase 9 |

**Rule: no completion claim may be written anywhere in this file while any row here is unresolved.**
