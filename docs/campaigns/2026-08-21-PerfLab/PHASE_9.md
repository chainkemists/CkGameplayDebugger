# Phase 9 — Compare, exports, CI entry, docs & close-out

> **Status:** ⏳ Pending
> **Depends on:** Phases 7 ✅ and 8 ✅
> **Estimate:** 1–2 sessions
> **Change class:** 3 (touches window + PerfLab; final full gate)

## Goal

After this phase: two same-map sessions compare position-by-position with regression flags; sessions
export to self-contained HTML, fixed-table CSV, and JSON — each deterministic and spec-pinned; a
headless entry runs measure+analyse+report and exits non-zero past a threshold; all campaign docs are
closed out and the full VALIDATION.md protocol has run.

## Entry criteria

- [ ] Read `Model/CkOptimizationDebugger_FindingsReport.{h,cpp}` + its determinism spec (P18) and
      `_SnapshotReport` (P12) — the export patterns to clone; read `Build_SnapshotDelta`
      (asset-keyed delta, decision #48) for the compare shape.
- [ ] Read `CkAssetExporter` commandlet + dispatch/summary shapes (the headless pattern) and
      `CkInsightsAnalyzerCommandlet.h` (flag conventions).
- [ ] Branch `perflab/phase-9`; baselines: ALL roots (`Ck.PerfLab`, `Ck.OptimizationDebugger`,
      `Ck.DebuggerCommon`, `Ck.DebuggerLauncher`, `Ck.Profile`) + full suite.

## Work items

1. **Compare** (`Public/CkPerfLab/Analysis/CkPerfLab_SessionCompare.h/.cpp`, pure): match by position
   id (Phase 3's stable ids ARE this contract); per-position metric deltas; unmatched positions
   listed both ways; regression flag = worsened beyond noise band (band from both sessions' spreads);
   refuse compare across maps; **warn — render, but banner — across differing config/machine/RHI**
   (comparability doctrine). Page: A/B pickers + delta table + regression pills; heatmap gains a
   diverging-delta mode (slot snapshot grows a compare variant — diverging palette per research §4).
2. **Exports** (each WITH its determinism spec, the module doctrine):
   - HTML: single self-contained file (clone `_FindingsReport` builder style) — score card +
     component table + formula text, environment block, positions, findings + contributors +
     recommendations, embedded legend; limitation paragraph verbatim.
   - CSV: fixed six tables (sessionInfo, positions, directions, findings, contributors,
     scoreComponents), one file per table or sectioned single file — decide by cloning
     `DataTableExporter` conventions; header row fixed; sorted rows, final tie-break.
   - JSON: the session file already IS the JSON export — the export verb copies it + injects the
     analysis block; schema documented in SCHEMA.md.
   - Save via `DesktopPlatform` dialog (dep added THIS commit — hygiene rule).
3. **Headless CI entry**: commandlet `-run=CkPerfLabReport -session=<dir> [-budget=<ms>]
   [-failbelow=<score>] -output=<dir>` → analyse + export + exit code (0 pass / 2 below threshold /
   1 error). Optionally `-measure -map=<path> -mode=<m>` to run the full child pipeline first (reuses
   Phase 5 launcher; commandlet runs in the host-less editor context — verify subprocess viability
   from a commandlet in this phase's first hour; if hostile, split: measurement stays `-game`-child
   launched by script, the commandlet only analyses — record the branch taken).
   This lands the capability review's Gap 4 for the perf half.
4. **Close-out**:
   - VALIDATION.md protocol executed: all gates on final binary, full-suite delta vs the Phase 0
     baseline, every `[EDITOR-VERIFY]` row handed to Adam as a checklist.
   - `CkPerfLab/CLAUDE.md`, `CkOptimizationDebugger/CLAUDE.md`,
     `CkOptimizationDebuggerEditor/CLAUDE.md` final; OptimizationDebugger `PLAN.md` gains a P19-style
     row pointing at this campaign.
   - Campaign docs: PLAN.md all-✅; PROGRESS.md final Current state (branch names, SHAs, what is
     NOT pushed); PROMPT.md untouched unless scope genuinely changed (dated).
   - **No push, no gitlink bump** — enumerate for Adam: per-submodule branch + head SHA + the merge
     order (CkFoundation first — CkGameplayDebugger's PerfLab includes CkProfile's new header).

## Expected observations — branches

| I will run | I expect | If instead | Response |
|---|---|---|---|
| Determinism specs (3 exports + compare) | Byte-stable given handed-in timestamp | Environment leakage (machine name in a sort? locale float format?) | The determinism spec exists precisely for this — fix at the builder, culture-invariant formatting |
| Full suite vs Phase 0 baseline | Baseline failing set unchanged; new greens added | New unexplained red outside campaign roots | Isolate: A/B stash; flake signatures per memory `unreal-toolbox-test-gotchas`; unresolved → PROGRESS blockers, surface to Adam, do NOT hand-wave |
| Commandlet run on fixture session | Exit codes 0/2/1 per scenario | Subprocess-from-commandlet hostile | Take the documented split branch (analyse-only commandlet); update PROMPT.md success criterion 7 wording with a dated note |

## Exit criteria — campaign done

- [ ] All success criteria in PROMPT.md checked off with named evidence or listed `[EDITOR-VERIFY]`.
- [ ] Full-suite gate delta vs Phase 0 baseline recorded (counts + names both sides).
- [ ] All same-commit doc updates landed; PROGRESS.md final entry distinguishes
      confirmed / inferred / human-owed.
- [ ] `ck-change-control` done-checklist run for class 3; comment audit done (no process
      breadcrumbs, no what-comments across the whole diff).

## Fences

- JSON export never re-serialises through a second schema — one schema, one writer (drift ban).
- Compare never silently mixes configs — banner, not suppression; suppression would hide the
  comparability rule this campaign exists to respect.
- Do not delete the campaign docs yourself — post-ship cleanup is Adam's call (PLAN.md notes the
  intent).
