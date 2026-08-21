# Phase 6 — Analysis: score, perf rule family, contributors, recommendations

> **Status:** ⏳ Pending
> **Depends on:** Phase 5 ✅ (real sessions exist to analyse)
> **Estimate:** 1–2 sessions
> **Change class:** 2 in CkPerfLab; **3 where it touches CkOptimizationDebugger's check plumbing**

## Goal

After this phase: loading a session produces — purely and deterministically — a 0–100 score with its
disclosed component table, a set of evidence-gated perf findings each naming its measurement, ranked
likely contributors per finding, and impact-vs-effort-ordered recommendations; all host-side, no
child involvement.

## Entry criteria

- [ ] Read `CkOptimizationDebugger/CLAUDE.md` §checks + `Analysis/Checks/` (two check files end to
      end, e.g. `_Checks_Lighting` and `_Checks_Actor`) + `_Thresholds.h` + `Build_Finding` — the
      family contract you are extending.
- [ ] Read the severity/tone axis (`CkDebuggerAxes.h`) and the suppression model
      (`_Suppression.{h,cpp}`) — perf findings must flow through both unchanged.
- [ ] Branch `perflab/phase-6`; baselines: `Ck.PerfLab`, `Ck.OptimizationDebugger` (76/76).

## Work items

### 6.1 Score (`Public/CkPerfLab/Analysis/CkPerfLab_Score.h/.cpp` — pure)

Locked shape (D6): 8 components, each 0–100 vs the request's `budgetMs`, weights from settings
(defaults below, disclosed verbatim in UI + exports):

| Component | Source | Default weight |
|---|---|---|
| Average frame attainment | per-position avg vs budget | 0.25 |
| Worst-case attainment | per-position worst vs budget | 0.15 |
| 1% low attainment | per-position 1% low vs budget×tolerance | 0.15 |
| Game-thread headroom | GT ms vs budget | 0.10 |
| Render-thread headroom | RT ms vs budget | 0.10 |
| GPU headroom | GPU ms vs budget | 0.10 |
| Spatial consistency | dispersion of position scores (CV → score) | 0.10 |
| Measurement confidence | share of ≥Medium-confidence positions | 0.05 |

- Per-position component score: `Clamp(100 × budget/actual, 0, 100)`; aggregate across positions by
  harmonic-style mean (weighted harmonic of per-position values; positions with `Unusable`
  confidence excluded from means but COUNTED in the confidence component).
- Unavailable metric (e.g. GPU) → its component drops out and remaining weights renormalise; the
  score result carries `componentsUsed` + reasons so exports can print exactly what was and wasn't
  measured. **Never a 0-score for an unmeasured axis.**
- Result struct carries every intermediate (per-component value, weight used) — the "formula visible"
  claim is satisfied by data, not prose.

### 6.2 Perf check family — v1 catalog (10 rules)

**Module split (load-bearing — the dependency arrow is UI → engine):** the pure analysis
(score, contributor ranking, evidence-gate predicate evaluation over session data) lives in
**CkPerfLab**. The check family itself — the files that produce debugger findings — lives in
**CkOptimizationDebugger** (`Analysis/Checks/CkOptimizationDebugger_Checks_Perf.{h,cpp}`), which
gains a `CkPerfLab` dependency in this phase's commit and consumes `FCk_PerfLab_AnalysisContext`
(the loaded session + thresholds). CkPerfLab never includes anything from CkOptimizationDebugger
(Phase 2 fence stands). Findings emit through the existing `Build_Finding` stable-key discipline
(key = checkId + position id + primary subject path) and the existing severity axis via
`Get_GraduatedSeverity`-style budget ratios.

**Evidence gate (hard rule, spec-pinned):** a rule may fire only when
`(metric at position P) > budget × ruleFactor` AND its static signal S is present in P's census.
The finding text names P, the metric value, and S's count. No over-budget metric → no finding, no
matter how ugly the census. No static signal → the generic over-budget finding fires instead
(rule 10), never a specific attribution.

| # | CheckId | Metric gate | Static signal (census) |
|---|---|---|---|
| 1 | `Perf.Gpu.TriangleDensity` | GPU over | summed in-view tri proxies over threshold |
| 2 | `Perf.Gpu.TranslucencyVolume` | GPU over | translucent-material / particle component count |
| 3 | `Perf.Gpu.DynamicShadowLights` | GPU over | shadow-casting movable lights in radius |
| 4 | `Perf.Gpu.LightOverlap` | GPU over | ≥N attenuation-overlapping lights at P |
| 5 | `Perf.GameThread.TickDensity` | GT over | ticking-actor count in radius |
| 6 | `Perf.GameThread.NiagaraDensity` | GT over | Niagara component count |
| 7 | `Perf.RenderThread.PrimitiveCount` | RT over | component/section count in view |
| 8 | `Perf.RenderThread.UninstancedDuplicates` | RT over | ≥N identical static meshes as separate components |
| 9 | `Perf.Confidence.StreamingUnsettled` | confidence < Medium w/ streaming reason | — (measurement-quality finding) |
| 10 | `Perf.General.OverBudgetUnattributed` | any metric over | no specific signal matched |

Thresholds (radius, factors, counts) live beside the existing per-user thresholds
(`_Thresholds` pattern — reflection panel picks them up; verify the panel's field-type support
before adding non-int fields, capability review §Gap 2 noted the `TFieldIterator<FIntProperty>`
limitation — **if a float threshold doesn't surface in the panel, that is a known panel limitation:
record it, don't widen the panel in this phase**).

### 6.3 Contributors + recommendations (pure)

- Per finding: rank census rows by (signal relevance × 1/distance × in-view), top K with distances;
  carry object paths for the UI verbs. Label: "near the measured cost — worth investigating, not
  proven cause" (verbatim string, spec-pinned so nobody softens it).
- Recommendations: static table per checkId — (expected gain band, effort band, ordered steps).
  Order the emitted list by gain-desc then effort-asc. Content authored this phase (10 entries),
  reviewed against each rule's engine reality; no generated prose.

### 6.4 Specs

1. `Ck.PerfLab.Score.*` — fixture sessions → exact expected scores (hand-computed); GPU-unavailable
   renormalisation; all-under-budget → 100-region + zero findings; `Unusable` exclusion.
2. `Ck.PerfLab.Checks.EvidenceGate` — census-rich but under-budget session → 0 findings (the moat,
   pinned); over-budget without signal → only rule 10.
3. `Ck.PerfLab.Checks.<each>` — one fixture each, asserting finding count, stable key, severity,
   evidence text naming position + value.
4. `Ck.PerfLab.Contributors.Ranking` — deterministic order incl. tie-break.
5. Determinism: same session analysed twice → identical results.
6. Fixture provenance: over-budget / census-rich fixtures are DERIVED from a real captured
   thin-map session (edit metric values, inject census rows) — never hand-authored from
   SCHEMA.md alone. One spec loads the raw captured session unmodified and asserts it decodes
   and analyses to zero findings. This is the drift guard: schema realism comes from a real
   capture; over-budget realism is layered onto it.
7. Budget-driven gate exercise (the suppression direction, on real data): analyse the captured
   thin-map session at `budgetMs = 0.5` → every position over budget, yet rules 1–8 stay ABSENT
   because the thin census carries no signal; only `Perf.General.OverBudgetUnattributed` may
   fire. **The analysis entry point therefore takes `budgetMs` as an override parameter rather
   than reading it solely from the session's request echo** — pure-side and cheaper than a second
   child run (Fable ruling, 2026-08-21).

## Expected observations — branches

| I will run | I expect | If instead | Response |
|---|---|---|---|
| `Ck.PerfLab` + `Ck.OptimizationDebugger` | New greens; 76/76 intact | An existing check spec breaks | You changed shared check plumbing — that is class-3 territory; re-read what you touched, restore, extend without modifying (`Build_Finding` is a call site for you, not an edit site) |
| Hand-computed score fixture | Exact match | Off by float noise | Pin with tolerance 1e-4 and document rounding in CLAUDE.md — never loosen past that to "make it pass" |

## Exit criteria — same commit

- [ ] All specs green vs baselines (both roots, final binary).
- [ ] `CkPerfLab/CLAUDE.md` §Analysis: the evidence-gate rule verbatim, the score table, the
      renormalisation contract, the contributor disclaimer string.
- [ ] PLAN.md row + Status header + PROGRESS.md entry (+ decision-log rows for any threshold-panel
      limitation found).
- [ ] `ck-change-control` class-3 gate if any existing OptimizationDebugger file changed
      (baseline diff + named old-contract speakers).

## Fences

- Analysis never re-reads the world — session data only (else host/child worlds diverge and the
  no-live-handle hazard returns).
- Do not add `Passed` to the severity enum (settled, resolution #4) — "measured clean" is a session
  state + status-strip fact (Phase 7) + export field (Phase 9).
- No new severity glyphs/colours — the shared axis only.
