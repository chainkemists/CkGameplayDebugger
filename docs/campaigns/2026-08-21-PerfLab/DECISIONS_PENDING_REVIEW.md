# PerfLab — decisions pending Adam's review

> Every judgment call the executor made **without** a Fable ruling, plus every Fable ruling it did
> get. Adam reviews this file; each row must be reversible from what is written here alone.
> Append newest at the bottom of the table. Do not delete rows — mark them `Reviewed: ✔`/`Reverted`.

Row format: **What was asked** · **Options weighed** · **Decision + why** · **Advisor**
(`Fable` / `self — Fable unavailable`) · **Blast radius** · **How to reverse**.

---

### D-001 — Which branch does campaign work land on?

- **Asked:** PHASE_0 entry says create `perflab/phase-0` in both submodules. Reality: Adam had
  already created **`feature/perf-lab`** in CkGameplayDebugger and committed the campaign docs to it
  (`c1afdc4`, 18 files, 1 commit ahead of `dev`, 0 behind). CkFoundation/CkTests are clean on `dev`.
- **Options:** (a) adopt `feature/perf-lab` as the campaign branch in every touched submodule;
  (b) create `perflab/phase-0` as planned and leave `feature/perf-lab` orphaned; (c) create
  per-phase branches stacked on `feature/perf-lab`.
- **Decision: (a).** Adam named the branch after this exact campaign and committed the plan to it —
  the repo is the authority over a plan doc written before it existed (`ck-methodology`: code wins).
  Per-phase branches (c) buy nothing here because nothing is pushed and phases land sequentially.
  CkFoundation gets the same branch name when Phase 1 first touches it.
- **Advisor:** self — decision is low-blast, reversible, and matches an explicit signal from Adam;
  a Fable consult would not change it.
- **Blast radius:** naming only. No history rewritten; no push.
- **Reverse:** `git -C Plugins/CkGameplayDebugger branch -m feature/perf-lab perflab/phase-0`
  (or branch off at any point). PLAN.md/PHASE_* mention the branch name in prose only.

### D-002 — Baseline captured with `--build --test`, full suite, auto-sized lanes

- **Asked:** Phase 0 requires a full-suite baseline. Build first or test the existing binary? Serial
  or parallel lanes?
- **Options:** (a) `--test` only against whatever binary exists — fast, but a stale binary makes the
  baseline non-comparable to later phases' runs; (b) `--build --test` serial (`--parallel 1`) —
  correct but ~23 min of test time; (c) `--build --test` auto-sized lanes — correct and ~2.4× faster.
- **Decision: (c).** The build-test skill's caveat against lanes under `--build --test` is
  specifically about codegen regen races when a build *changes* codegen; Phase 0 changes no source
  at all, so no regen occurs and the race cannot trigger. Building first is required because later
  phases diff against this number on freshly built binaries.
- **Advisor:** self — mechanical, and the skill's own reasoning settles it.
- **Blast radius:** the baseline number itself. If it proves noisy, re-run.
- **Reverse:** re-run `./CkAuto/UnrealToolbox.exe --build --target=Editor --test --parallel 1` and
  replace the baseline recorded in PROGRESS.md.

### D-003 — What content validates the evidence gate? **(Fable-ruled)**

- **Asked:** The host project is deliberately content-minimal — zero maps in `Content/`, only two
  thin CkTests levels. Nothing here can make a perf rule legitimately fire, so the evidence gate
  (the product's moat) would ship never having seen real measured data.
- **Options:** (a) validate on the thin gym maps only; (b) author a deliberately expensive
  validation map; (c) fixtures in specs + thin-map smoke + real-content validation deferred to an
  `[EDITOR-VERIFY]` against a downstream game project; (d) hybrid.
- **Decision: (c)+(a), amended — plus a budget-manipulation pass.** `budgetMs` is a *request
  parameter*, not a property of the content: re-analysing a real thin-map capture at
  `budgetMs = 0.5` makes every position genuinely over budget with **real measured data**, while
  the thin census means rules 1–8 must stay silent and only
  `Perf.General.OverBudgetUnattributed` may fire. That exercises capture → decode → gate →
  suppression → finding emission end-to-end, with zero content authored. Paired with the
  generous-budget run (score ≈100, zero findings), both directions of the moat now run on real
  data in this repo.
- **Advisor: Fable** (consulted 2026-08-21; ruling applied verbatim).
- **Why (b) lost:** primary disqualifier is *nondeterminism*, not repo pollution — rule-firing on
  a checked-in map depends on machine-relative cost, so the acceptance gate it exists to serve
  could not be deterministic. Repo pollution is the second strike.
- **What the executor had wrong:** I treated "real end-to-end evidence of the gate" as
  unobtainable without heavy content, and would have shipped with measured data never flowing
  through the rule evaluator — the exact false-confidence failure this question was raised about.
- **Honest residual (Adam should know):** even amended, the *positive* conjunction (over budget
  AND signal present → a specific rule fires) never runs end-to-end inside this repo. It is
  spec-verified only until the downstream BusterBlock `[EDITOR-VERIFY]` row in VALIDATION.md §B
  passes. That row now states this explicitly instead of leaving it implicit.
- **Blast radius:** test strategy for Phases 4/6/9; no production-code shape changes beyond making
  the analysis entry point take a `budgetMs` override (pure-side).
- **Reverse:** drop VALIDATION.md rows 6a/6b and PHASE_6 spec items 6–7; the analysis budget
  override is harmless if unused.
- **Amendments applied:** VALIDATION.md §A rows 6/6a/6b + §B post-campaign row; PHASE_6.md §6.4
  items 6–7; PHASE_4.md runner "budget is carried, never baked" bullet.

### D-004 — Plan-vs-code corrections found in Phase 0 (no ruling needed, recorded for audit)

- `PHASE_2.md` claimed CkGameplayDebugger modules use plain `ModuleRules`. **False** —
  `Source/CkOptimizationDebugger/CkOptimizationDebugger.Build.cs` opens
  `public class CkOptimizationDebugger : CkModuleRules`, inheriting across the plugin boundary.
  Corrected in PHASE_2.md and PHASE_8.md.
- Every CkGameplayDebugger module must link `CkEcs` (that file's own comment: "CkCore's SharedPCH
  instantiates global ECS registrations"). Added to both phase files. **Nuance recorded:**
  `CkProfile` does NOT and must NOT link CkEcs — it is tier T1 and CkEcs is T2, and deps may never
  point to a higher band. The rule applies to the debugger-tier modules, not to Phase 1's work.
- `RESEARCH_EngineApis.md` located `stat unit` in `UnrealEngine.cpp`; it is actually
  `UnrealClient.cpp:350`, and `IsAsyncLoading()` does not exist under that name in this checkout.
  Both corrected in `RESEARCH_EngineApis_Addendum.md`, which supersedes.
