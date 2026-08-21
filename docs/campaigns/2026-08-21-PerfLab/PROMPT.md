# PerfLab — mission brief (PROMPT.md)

> **Written:** 2026-08-21 by the Fable planning session. STABLE content only — current state lives in
> [PROGRESS.md](PROGRESS.md). Phase contracts live in `PHASE_0.md` … `PHASE_9.md`, indexed by
> [PLAN.md](PLAN.md).
> **This doc dies when:** the campaign ships; permanent contracts move to
> `Source/CkPerfLab/CLAUDE.md` + `Source/CkOptimizationDebugger/CLAUDE.md`. On death, replace the body
> with one tombstone line.

## Problem statement

Finding why a level drops frames is a manual walk with `stat unit` open. The commercial benchmark is
**Level Performance Doctor** (Fab, 2026-08-21): pick a map + target frame rate, press Start; a
separate instance flies a camera through auto-chosen positions, records Frame/GT/RT/GPU timings with
settle/warmup/outlier/confidence rigor, scores the level 0–100 with a disclosed formula, draws a
viewport heatmap, attributes findings to nearby actors, and exports HTML/CSV/JSON with two-session
compare. Full teardown: [RESEARCH_Competitive.md](RESEARCH_Competitive.md).

Our suite already owns the *static* half — `CkOptimizationDebugger` (76/76 gate, 28 checks, HTML/MD
exports, snapshot compare, heatmap lenses) is broader than every surveyed listing on analysis depth.
What it cannot do is **measure**: no GT/RT/GPU readers exist anywhere in the repo, no position
planner, no child-instance orchestration, no score, no measured-evidence rules.
Inventory: [RESEARCH_Codebase.md](RESEARCH_Codebase.md).

## Goal

After this campaign: from the CkOptimizationDebugger window's new **Performance page**, a user picks a
map, a frame budget, and a mode (Quick/Standard/Deep), presses Run; a separate `-game` instance of the
project measures the level and exits; the page then shows a 0–100 score with its disclosed formula,
per-position results with confidence ratings, measured-evidence findings with likely contributors
(select / focus / open-asset), and a level-viewport heatmap (colour+shape+size); two sessions of the
same map compare position-by-position; results export to HTML, CSV, and JSON deterministically; a
headless CI entry point runs the same analysis and exits non-zero past a threshold — all with zero
Test/Shipping footprint and zero modification of any map or asset.

## Success criteria (each an observation)

1. `Ck.PerfLab.*` + `Ck.OptimizationDebugger.*` spec gates green; **no regression vs the recorded
   baseline** (OptimizationDebugger baseline at campaign start: 76/76, P18 2026-08-21; full-suite
   baseline captured at Phase 0 entry).
2. A session run against a test map produces a `session.json` whose per-position records contain
   Frame/GT/RT ms, GPU ms **or an explicit unavailability reason (never 0-as-data)**, avg + worst +
   1% low + outlier count + confidence — verified by reading the artifact.
3. Running the same session twice on the same machine/map/mode yields position sets that match by
   position id, and the compare view renders a per-position delta — `[EDITOR-VERIFY]`.
4. The heatmap draws over the level viewport with no actor spawned, no package dirtied (`git status`
   clean of content, editor dirty-state clean) — `[EDITOR-VERIFY]`.
5. Every published finding names the measurement (position id + metric + value vs budget) that
   produced it; a level with all positions under budget publishes zero perf findings and the page says
   "measured clean", not nothing.
6. HTML/CSV/JSON exports are deterministic (two exports of one session differ only in the handed-in
   timestamp) — pinned by specs, per the module's export doctrine.
7. The headless entry (`-CkPerfLab-Request=` runner + `-run=CkPerfLabReport` reporter) completes with
   exit 0 on a passing session and non-zero past the fail threshold, with no human interaction.
8. Main editor remains responsive during a run (child is a separate process) — `[EDITOR-VERIFY]`.
9. Test/Shipping packages contain no PerfLab module (module types DeveloperTool/Editor only).

## Constraints & locked decisions

| # | Decision | Choice | Why (rejected alternatives below) |
|---|---|---|---|
| D1 | Augment vs new plugin | **Augment**: new UI-free module `CkPerfLab` (DeveloperTool) + new `CkOptimizationDebuggerEditor` (Editor, EdMode host) in **CkGameplayDebugger**; Performance page inside the existing window; timing readers added to **CkFoundation/CkProfile** | OptimizationDebugger already owns rules/exports/compare/UI plumbing; the InsightsAnalyzer(core)/InsightsDebugger(UI) split is the house pattern for "UI-free engine + Slate shell" |
| D2 | Child instance | **The host's own editor executable** — `FPlatformProcess::ExecutablePath()`, verbatim (here `CkPluginsEditor-Cmd.exe`; `UnrealEditor.exe` on a shared-build-environment consumer) — run as `<proj> <map> -game -windowed -resx/-resy -unattended -nosplash -SCCProvider=None -abslog=<sessiondir>\child.log`; file-based handoff (request JSON in, heartbeat + session JSON out under `Saved/CkPerfLab/Sessions/<id>/`) | Keeps main editor usable; `-game` minimizes editor overhead + `Saved/` contention. *Dated edit 2026-08-21 (Phase 0): the child must be the host's OWN binary — the engine's `UnrealEditor-Cmd.exe` cannot resolve this project's `TargetBuildEnvironment.Unique` module manifests in ANY mode (reproduced with and without `-game`). With the correct binary, `-game` boots and runs with zero Game-target artifacts on disk. Overturn-and-reinstate recorded in DECISIONS_PENDING_REVIEW.md D-005.* |
| D3 | Timing source | Engine stat-unit globals read live per frame: `RenderTimer.h` cycles + `RHIGetGPUFrameCycles()` (**`GGPUFrameTime` is deprecated 5.6 — fenced**), mirroring `FStatUnitData`'s expressions | Same counters as `stat unit` (parity claim is literal); no dependency on flaky headless Insights |
| D4 | Modes | Quick/Standard/Deep are **parameter presets** over (position budget, directions/position, repeats, settle strictness, adaptive re-measure on/off) — one code path | LPD's modes reduce to presets; avoids three runners |
| D5 | Unavailable metrics | Reflected structs model availability as enum + reason + value (**not `TOptional` — open fork A1**); score renormalises weights over available components; never 0-as-data | ADJUDICATIONS A1 interim stance; LPD's "Unavailable, never silently 0" is correct |
| D6 | Score | 0–100, 8 disclosed weighted components vs user budget, harmonic-style aggregation across positions, weights in settings + printed in UI/exports | 3DMark precedent; harmonic mean stops one good area averaging away one terrible one |
| D7 | Rules | New `Perf` check family reusing the `Run_Checks(Context, Thresholds, OutFindings)` contract; **v1 catalog = 10 rules**, every rule evidence-gated (fires only with a measured over-budget metric at a position + the static signal near it) | Evidence-gating is the moat; the family contract makes growth to 24+ mechanical |
| D8 | Contributors | Child records, per position, an actor census (object path, class, cost proxies) within radius + in-view; host resolves paths against the open level for select/focus/open-asset; proximity ≠ causation labelled everywhere | Object paths are stable across instances of the same map; no `FCk_Handle` crosses the boundary (no-live-handle invariant) |
| D9 | Heatmap | EdMode + PDI markers, `CkSaveDebugger_VisualizerEdMode` pattern (stateless slot-published snapshot, hit-proxy click-through), colour via `ck::debug_axes::Get_HeatColor` + shape (severity) + size (magnitude) | The one sanctioned editor-viewport 3D path; Editor module for a reflected EdMode is exactly what decision #2 permits |
| D10 | Persistence | Versioned JSON session schema (`schemaVersion` field, whole-or-nothing decode, SnapshotCodec discipline); exports HTML (self-contained), CSV (fixed tables), JSON — each landing WITH its determinism spec | Module export doctrine is binding |
| D11 | Branching | One local topic branch **`feature/perf-lab`** per touched submodule; commit progressively; **never push**; superproject gitlinks untouched — merge/pointer-bumps are Adam's post-audit step | House rule (LiveTune precedent). *Dated edit 2026-08-21 (Phase 0): originally specified `perflab/phase-N`; Adam had already created `feature/perf-lab` in CkGameplayDebugger and committed the campaign docs to it (`c1afdc4`), so the repo won. See DECISIONS_PENDING_REVIEW.md D-001.* |
| D12 | Build/test | UnrealToolbox only (`/build-test` skill); new specs need `--discover-fresh` or `--build` in the same invocation | Standing instruction |

### Rejected approaches (do not re-litigate)

| Rejected | Kill reason |
|---|---|
| Standalone new plugin | Duplicates rules/export/compare/UI plumbing the debugger already gates at 76/76; violates mimicry doctrine |
| Measure inside the host editor (PIE or viewport) | Blocks the editor; editor Slate/tick pollutes numbers; violates the window's no-Tick/no-PIE invariant |
| Second full **editor** child instance | Editor overhead pollutes numbers further; `Saved/` lock contention; slower boot. *Re-litigated and re-rejected 2026-08-21: the Phase-0 boot failure that prompted the challenge was engine-vs-project binary resolution, not a `-game` limitation. With the correct binary, `-game` keeps real game tick, BeginPlay and GameMode — so GameThread numbers stay valid instead of becoming editor-environment claims — and adds no editor chrome to GPU numbers.* |
| CSV Profiler or headless Unreal Insights as primary data path | Headless Insights export is reported broken/flaky on 5.5–5.7 (Epic KB); CSV profiler adds a parse layer for numbers the globals give directly. `.utrace` artifact = v2 flank |
| Driving the child via CkAuto `UnrealToolbox --gauntlet-visual` | Toolbox is a dev-environment tool, not shipped with the plugin; `--gauntlet-visual` disables watchdogs; the product path must be self-contained (toolbox remains OUR build/test path) |
| Remote Control API as the host↔child channel | Heavyweight vs file polling; adds a network surface to a batch job |
| Fixed timestep for determinism | Changes tick load — measures a different game. Dwell-at-position + repeats + outlier handling instead |
| `TOptional<float>` for unavailable GPU in reflected structs | Open adjudication A1; interim stance is enum-mode + value |
| Spawning marker actors for the heatmap | Dirties the map; "safe by design" is a hard requirement |
| `Passed` as a fourth severity | Already rejected (capability review resolution #4); "measured clean" is a status-strip statement + export field |

## Non-goals (v1)

- **Packaged/cooked-build measurement** — editor-environment `-game` only; UI + exports state this
  limitation verbatim (LPD does too). AutomatedPerfTest remains the cooked-build answer.
- **`.utrace` capture/ingest** (CkInsightsAnalyzer composition) — v2 flank; don't link TraceServices.
- **World Partition cell-aligned planning** — API volatile; grid+navmesh covers v1.
- **Auto-fixes from perf findings** — recommendations only; the existing fix engine stays untouched.
- **AngelScript surface** — C++-only tool (AS cannot reach Slate/JSON/process APIs).
- **Rule-count parity (24)** — v1 ships 10 evidence-gated rules on a growable family contract.
- **Cross-machine score normalisation** — the score is explicitly machine-relative.
- **Raw per-frame sample retention by default** — summary + percentiles + outlier records persist;
  raw arrays behind a default-off setting (session size).

## Executable spec (per ck-plan-handoff)

Each phase leads with red specs: house `.spec.cpp` pattern under the owning module, run via
`UnrealToolbox --test --test-pattern Ck.PerfLab` (see each PHASE file's exact invocation + expected
counts). Phase 0's spec is a repro-command-with-captured-output (child-launch smoke). UI/editor
behaviour that specs cannot reach is enumerated as `[EDITOR-VERIFY]` in [VALIDATION.md](VALIDATION.md).

## Pre-flagged adjudication (surface to Adam, do not self-rule)

- **A-PerfLab-1 — where does the subprocess utility live?** Recommendation: private to `CkPerfLab`
  (`CkPerfLab_Subprocess.{h,cpp}`, GitLink_Subprocess as the pattern), flagged as an extraction
  candidate to `CkCore` when a second consumer appears. Rationale: non-negotiable #9 favours
  extraction, but #6 says an unwritten norm fork goes to the maintainer — a process-launch primitive
  in T1 CkCore is a policy call. **Executor: implement the recommendation; file the ADJUDICATIONS row
  in Phase 5; do not move it to CkCore unilaterally.**

## Glossary

- **Position** — one measurement location (XYZ + eye offset) with N yaw directions; **sample** — one
  accepted frame's timing record; **dwell** — the settle+sample period at one direction.
- **Confidence** — per-position rating derived from warmup stability, streaming activity during
  sampling, sample count vs target, and frame-time spread.
- **Evidence-gated** — a finding publishable only when a measured metric at a position exceeded
  budget AND the rule's static signal exists near that position.
- **Session** — one complete run: request + environment + positions + samples + analysis; stored
  under `Saved/CkPerfLab/Sessions/<SessionId>/`.
- **Child / runner** — the spawned `-game` process executing the measurement; **host** — the editor
  running the debugger window.
- **Gate** — a toolbox spec run with recorded counts vs baseline (`ck-change-control`).

## Reading list (load-when-directed; each PHASE file says when)

1. `Plugins/CkGameplayDebugger/Source/CkOptimizationDebugger/CLAUDE.md` — before ANY phase touching
   that module (5–9).
2. `Plugins/CkFoundation/CLAUDE.md` + `Source/CLAUDE.md` — style doctrine, before Phase 1.
3. Skills: `ck-change-control` (every phase exit), `ck-methodology` (campaign rituals),
   `ck-performance-and-analysis` (Phases 1, 4, 6), `ck-macros-and-codegen` (Phases 1–2),
   `ck-tests-authoring-and-running` (every phase), `ck-debugging-playbook` (on any red),
   `ck-failure-archaeology` (before novel infra).
4. This campaign's RESEARCH_*.md — Phase 0 first task is spot-checking their load-bearing claims.

## Things ruled out — do not re-investigate

| Ruled out | Why | Evidence |
|---|---|---|
| A repo-existing GT/RT/GPU reader | None exists; `Get_FrameTimeMs` is wall-clock only | Repo-wide search 2026-08-21 (RESEARCH_Codebase.md §4.1) |
| An existing process launcher in CkFoundation | Zero `CreateProc` hits | Same, §4.2 |
| `GGPUFrameTime` as the GPU source | `UE_DEPRECATED(5.6)` in our engine checkout | `RHIGlobals.h` / `RHICommandList.h`, planner-read 2026-08-21 |
| `CK_ENABLE_MEMORY_TRACKING` for memory data | Off in every config; enabled path doesn't compile | `ck-performance-and-analysis` §1.8 |
| "EntityBridge" as an integration point | No such class/module exists | RESEARCH_Codebase.md §3 row 8 |
| Gauntlet manifest reuse for the product path | Host has no `GauntletTests.json`; Gauntlet is cooked/CI-shaped | RESEARCH_Codebase.md; toolbox skill |
