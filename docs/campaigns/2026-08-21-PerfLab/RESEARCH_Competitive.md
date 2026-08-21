# PerfLab — competitive & prior-art research

> **Written:** 2026-08-21 (Fable planning session). Point-in-time market survey — re-verify prices/versions before quoting externally.
> **This doc dies when:** the campaign ships; permanent conclusions move to `Source/CkPerfLab/CLAUDE.md`.

Provenance: web research agent (2026-08-21), URLs cited inline. Claims about the target product were read
from its live Fab listing via browser. Nothing here was run in an editor.

## 1. The target: Level Performance Doctor (Matify Games)

Fab listing `87e7c04a-ddae-40a7-afb6-5c3d16efb265`. **Published 2026-08-21 — brand new, zero reviews,
no community feedback to mine.** $48–$110, UE 5.5–5.8, Windows only. Not from an established profiling
studio.

What it does (from its own listing):

- Separate editor instance loads the map; main editor stays usable. Auto-determined measurement
  positions; camera moved through them; records Frame / GameThread / RenderThread / GPU "from the same
  underlying engine performance counters used by stat unit".
- Three modes: **Quick** (fast sweep), **Standard** (survey pass → adaptive extra time where numbers
  justify it; claims 1.93× speedup at equal rule coverage vs exhaustive on a 25-position level),
  **Deep** (16 camera directions per position + repeats).
- Rigor story (its whole credibility pitch): warmup discard, camera settle, streaming settle,
  frame-stability criterion before accepting samples; outliers counted — excluded from averages,
  retained in worst-case/1%-low; per-location **confidence rating** (warmup stability, streaming
  activity, sample count, frame-time spread); GPU shown as **"Unavailable + reason", never silently 0**.
- Score: 0–100 from **8 weighted components, formula fully disclosed**.
- Viewport heatmap: colour + marker **shape** + marker **size** (explicit colourblind accommodation);
  no actors spawned; map never modified.
- 24 "evidence-based" rules (lights, static/skeletal meshes, materials, tick density, Niagara,
  collision, decals, streaming, world partition). "If the tool does not have evidence for a finding,
  it does not publish it."
- "Likely contributors" = actors near expensive spots, with select / focus-viewport / open-asset
  actions and an explicit proximity ≠ causation disclaimer. Recommendations ranked impact-vs-effort.
- Two-session comparison by position matching. Exports: HTML (single self-contained file), CSV
  (6 tables), JSON. Sessions in `Saved/LevelPerformanceDoctor`. Two modules uncooked-only + one
  editor-only. Explicit limitation text: measures the editor environment on the local machine, not a
  packaged build; the score is a relative/repeatable benchmark, not absolute.

**Open flanks (inferred — no reviews exist):** UI-only (no CLI/CI path mentioned), no Insights/trace
artifact, proximity-only attribution, Windows-only, unproven v1.0.0.

## 2. Market

| Tool | Approach | Take |
|---|---|---|
| **Unreal Performance Doctor** (StraySpark, $40/$130) — strayspark.studio | 28+ static rules, health score, baseline JSON compare, MD/JSON/HTML export | **Steal: CLI with fail-under thresholds + CI summaries.** Findings carry severity, confidence, difficulty, expected impact. No measurement pass. |
| **Ultimate Optimization Toolkit** (IchiQ, ~$7, 4.5★) — fab.com/listings/c137749f-… | ~22 checks + **auto-fixes** via undoable transactions | Rule engines without measurement sell for $7 — the moat is measured evidence, not rule count. Steal: finding jumps to the *owning* object (asset vs actor vs settings page). |
| **Performance Benchmark** (Warhead) / **PRO Benchmark System** (Dr. Bolel) | Player-facing benchmark modes from sequences | Different market. Confirms warm-up + 1%/0.1% lows are table-stakes vocabulary. |
| **MS UE4TelemetryVisualizer** (OSS) github.com/microsoft/UE4TelemetryVisualizer | Telemetry emit + in-editor heatmap viz | OSS reference for editor-viewport heatmap drawing. |
| **GauntletAutomationDemo** (OSS) github.com/S1Lazza/GauntletAutomationDemo | Gauntlet + CSV profiler + PerfReportTool HTML | The "DIY with engine tools" baseline to beat on convenience. |

**Market read:** nothing else on Fab combines measured flythrough timing + rules + in-viewport heatmap
+ scoring. Competitors are static scanners (no evidence) or player benchmarks (no analysis).
`CkOptimizationDebugger`'s own 2026-08-15 capability review reached the same conclusion for the static
half: it is already broader than every listing surveyed. **PerfLab's job is the measured half.**

## 3. Epic's own facilities — and why we differ

- **AutomatedPerfTest plugin** (experimental, 5.5+): BuildGraph + UAT + Gauntlet + CSV Profiler +
  PerfReportTool + Insights artifacts; Static Camera / Sequencer / Replay controllers; **cooked builds
  only**, CI mindset, Horde-leaning reporting, no in-editor UX, no rules, no attribution
  (forums.unrealengine.com/t/automated-performance-testing-plugin-guidance/2666136). Its weaknesses
  are exactly the convenience gap PerfLab fills; its strength (packaged-build ground truth) is the gap
  PerfLab must be honest about.
- **CSV Profiler + PerfReportTool**: per-frame CSV → templated HTML graphs
  (`Engine/Binaries/DotNET/CsvTools/PerfreportTool.exe`). Optional future compatibility target — not
  the v1 data path.
- **Unreal Insights headless**: `-trace=cpu,gpu,frame` capture works; but headless
  `UnrealInsights.exe -NoUI -AutoQuit` export is **reported flaky/broken on 5.5–5.7** (Epic KB thread
  forums.unrealengine.com/t/…-1882278). Do NOT build the primary data path on it. A `.utrace` as an
  optional deep-dive artifact (consumed by our own `CkInsightsAnalyzer`) is the right future shape.
- **fps charts / `FAutomationPerformaceHelper`**: legacy; superseded by CSV profiler. Skip.
- **AMD UE performance guide** (gpuopen.com/learn/unreal-engine-performance-guide/) explicitly
  recommends camera flythroughs for repeatable capture. Dwell-at-position sampling (LPD-style) avoids
  the fixed-timestep tick distortion of a moving flythrough.

## 4. Scoring & heatmap prior art

- **3DMark**: sub-score = constant × harmonic mean of test FPS; overall = weighted harmonic mean —
  harmonic punishes the weak component, appropriate when one slow area ruins the level
  (support.benchmarks.ul.com/…/44002136188). PerfLab adopts: per-position component scores vs the
  user's frame budget, harmonic-style aggregation across positions, **weights disclosed**.
- **Colour**: green/yellow/red fails deuteranopia. Use a perceptually-uniform colourblind-safe ramp —
  the suite already owns one: `ck::debug_axes::Get_HeatColor` (`CkDebuggerCommon/Styles/CkDebuggerAxes.h`).
  Redundant encoding (shape + size + label) is standard a11y practice and what LPD does.
- Diverging palettes only for A/B compare views (improvement vs regression around zero).

## 5. Design lessons adopted into the plan (ranked)

1. **Evidence-gated rules are the moat** — a finding must cite the measurement that produced it.
2. **GPU time is optional everywhere** — schema, score, UI all model "Unavailable (reason)";
   renormalise weights; never store 0.
3. **Disclosed weighted score vs a user-chosen budget, harmonic aggregation.** Relative benchmark, not
   absolute — say so in the UI and the export.
4. **Settle-then-sample**: warmup discard, streaming quiescence, variance stability criterion,
   outliers counted not erased, per-position confidence.
5. **Adaptive two-phase sampling** (survey → deep re-measure where bad); exhaustive as a toggle.
6. **Own the CI flank**: headless entry + JSON + meaningful exit code (LPD doesn't have one; the
   OptimizationDebugger capability review's Gap 4 already asks for `-run=CkOptimizationAudit`).
7. **Compose with Epic later** (CSV-profiler-compatible CSV, `.utrace` artifact) — not v1.
8. **Attribution honesty**: proximity ≠ causation, always labelled.
9. **Never modify the map**: PDI/EdMode drawing, no spawned actors, nothing dirtied.
10. **Comparison view makes it sticky** — matched-position A/B with regression flags.

## 6. Pitfalls the plan must fence

- **First-run pollution**: cold DDC, on-demand shader compiles, PSO precache misses contaminate pass 1
  → throwaway warm-up sweep of all positions before measuring; report shader-compile/DDC activity in
  confidence.
- **`-nullrhi` = no GPU (and no RT) timings** — headless fast mode measures game thread only; UI must
  say so, schema must record it.
- **Vsync / smoothing**: child must force `r.VSync 0`, `t.MaxFPS 0`, `bSmoothFrameRate=false` or every
  position measures the vsync interval.
- **Editor-vs-game overhead**: `ck-performance-and-analysis` §1.6 — Development-editor carries Ck debug
  overhead Development-game does not; numbers are comparable only within one config. Sessions record
  config + instance mode; compare view refuses cross-config diffs (warns, does not silently mix).
- **Streaming settle is genuinely hard**: no single "everything resident" signal exists; combine
  `IsAsyncLoading()`, streaming-manager in-flight counts, level streaming state; cap with a timeout and
  degrade confidence instead of blocking forever. WP cell-state APIs are version-volatile — verify.
- **UE 5.6.0 DX12/Vulkan timestamp assertion crash** existed
  (forums.unrealengine.com/t/…-2571766) — GPU timing path needs graceful degradation, tested on the
  project's actual RHI.
- **Child lifecycle**: hangs (own watchdog + host timeout kill), modal dialogs (`-unattended
  -nosplash`), `Saved/` contention (child writes only under its session directory).
- **Determinism**: GC, async compilation, background processes → repeats + outlier handling, not
  fixed timestep (changes tick load; rejected for v1).
