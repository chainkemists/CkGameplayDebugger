# Phase 7 — Performance page UI

> **Status:** ⏳ Pending
> **Depends on:** Phase 6 ✅
> **Estimate:** 1–2 sessions
> **Change class:** 3 (modifies SCkOptimizationDebuggerWindow)

## Goal

After this phase: the CkOptimizationDebugger window has a seventh page, **Performance** — run setup
(map, budget, mode), live progress from the heartbeat, session list, score card with the disclosed
component table, per-position results with confidence, findings list flowing through the existing
findings UI (filters, mute, severity axis), and contributor verbs (select / focus / open asset).

## Entry criteria

- [ ] Read `SCkOptimizationDebuggerWindow.cpp` page-registration + one full existing page
      (Snapshots — closest in shape: sessions, compare, report) and the model/view split the doc
      mandates; read `CkDebuggerCommon/Widgets/` inventory; read the refresh idiom
      (`RegisterActiveTimer` per P17 — the sanctioned polling path).
- [ ] Branch `perflab/phase-7`; baselines: `Ck.OptimizationDebugger` (76/76), `Ck.PerfLab`,
      `Ck.DebuggerCommon`, `Ck.DebuggerLauncher`.

## Work items

1. **Model first** (the module's own doctrine): page state in the model layer
   (`ck_optimization_debugger_model` conventions) — selected session, run-in-flight handle, filter
   reuse. All new model logic spec-tested.
2. **Run setup + progress**: map picker (levels list — reuse the dashboard's sub-level source), budget
   input (`SCkDebug_NumericEditor`), mode selector; Run/Cancel via Phase 5 API; progress =
   `RegisterActiveTimer` polling the heartbeat (the invariant forbids `Tick` overrides — respect it);
   status via `SCkDebug_StatusPill`.
3. **Results**: score card (big number + component table printed from the score result's own data);
   per-position list (position id, avg/worst/1% low, confidence chip, availability notes); findings
   reuse the existing findings widgets + filter state (perf family = one new category bit — follow
   the `k_CategoryCount` growth discipline P18's Audio family established; its gate caught exactly
   this constant going stale, so grep `k_CategoryCount` first).
4. **Contributor verbs**: resolve object path in the CURRENT editor world; select
   (`CkEditorOnly_Utils` selection helpers — verify what exists), focus viewport, open asset. Path
   fails to resolve (map not open / renamed) → disabled verb + reason tooltip, never silent.
5. **PIE boundary**: measured sessions persist (disk-backed — unlike scan findings), but
   world-resolving verbs re-check on invalidation
   (`ck::DebugSessionLifecycle::Get_OnSessionInvalidated()`).
6. **"Measured clean"**: all-under-budget session renders an explicit Ok-toned status-strip statement
   (NOT a findings row — resolution #4).
7. Launcher: page lives inside the existing OptimizationDebugger tab — **no new tool descriptor**;
   only if a nomad-tab surface is later demanded does the launcher catalog change (out of scope).

## Expected observations — branches

| I will run | I expect | If instead | Response |
|---|---|---|---|
| `Ck.OptimizationDebugger` gate | 76/76 + new model specs green | Any existing spec red | Class-3 alarm: diff your model edits; category-count specs are the likely tripwire (P18's `k_CategoryCount` lesson — grep it) |
| `[EDITOR-VERIFY]` full run from the page | Editor stays responsive; progress advances; results render | — | Human-only; enumerate in VALIDATION.md, do not claim |

## Exit criteria — same commit

- [ ] Gates green (all four roots) on the final binary; counts vs baselines recorded.
- [ ] Every editor-only behaviour enumerated as `[EDITOR-VERIFY]` rows in VALIDATION.md (run,
      cancel mid-run, contributor verbs, clean-session strip, PIE boundary).
- [ ] `CkOptimizationDebugger/CLAUDE.md` amended: Performance page section (page contract, the
      disk-backed-session vs scan-findings lifetime distinction, the no-Tick compliance note).
- [ ] PLAN.md row + Status header + PROGRESS.md entry.

## Fences

- No `Tick` override on the window — active timer only (invariant).
- No `FCk_Handle` anywhere in page state (invariant).
- Filters/mute/severity flow through existing shared state — no parallel filter system.
- UI strings state the limitation verbatim: measured in-editor `-game` on this machine; relative
  benchmark, not absolute (PROMPT.md non-goal honesty).
