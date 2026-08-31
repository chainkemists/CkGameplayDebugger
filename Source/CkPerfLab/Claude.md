# CkPerfLab

**Purpose:** the measurement half of the level-performance tooling. Owns the session model, the
sampling statistics, the JSON codec, the position planner, the in-child measurement runner, and the
host-side orchestration. It computes and records; it draws nothing.

**Type:** `DeveloperTool` — ships in packaged Development/DebugGame, excluded from Test and Shipping.
**Depends on:** `CkCore`, `CkEcs`, `CkLog`, `CkNavigation`, `CkProfile`, `Json`.
**Used by:** `CkOptimizationDebugger` (the Performance page and the perf check family).

---

## The dependency arrow points one way

`CkOptimizationDebugger` depends on **this** module. This module must never depend on it, and must
never gain a Slate dependency. The split mirrors `CkInsightsAnalyzer` (UI-free engine) and
`CkInsightsDebugger` (Slate shell) — the reason it matters here is that the runner half of this
module executes inside a separate `-game` process where no editor and no Slate exist at all.

Also fenced: **no dependency on `CkInsightsAnalyzer`**. Its statistics shapes were the reference for
this module's, but linking it would drag `TraceServices` into a process whose job is to measure.

---

## What's here

```
CkPerfLab/
├── Session/
│   ├── CkPerfLab_Session.h/.cpp          – the on-disk model: request, environment, positions, metrics
│   └── CkPerfLab_SessionCodec.h/.cpp     – JSON for request / heartbeat / session
├── Stats/
│   └── CkPerfLab_SampleStats.h/.cpp      – dwell samples -> metric summary, and the confidence rating
├── Planner/
│   └── CkPerfLab_Planner.h/.cpp          – where to stand, from a world survey (pure)
├── Runner/
│   ├── CkPerfLab_WorldSurvey_Builder.*   – what the level contains, and where it is navigable
│   ├── CkPerfLab_SettleDetector.h/.cpp   – when a dwell has stopped moving and may be sampled
│   └── CkPerfLab_Runner_Subsystem.*      – the in-child measurement loop
├── Host/
│   ├── CkPerfLab_Subprocess.h/.cpp       – launching and watching the -game child
│   └── CkPerfLab_SessionStore.h/.cpp     – sessions on disk
├── Analysis/
│   ├── CkPerfLab_Analysis.h/.cpp         – the score and its eight disclosed components (pure)
│   ├── CkPerfLab_Rules.cpp               – the evidence-gated finding rules
│   └── CkPerfLab_SessionCompare.h/.cpp   – two sessions, matched by position id (pure)
├── Export/
│   └── CkPerfLab_Export.h/.cpp           – HTML / CSV / JSON builders (pure, deterministic)
├── Heatmap/
│   └── CkPerfLab_HeatmapSlot.h/.cpp      – the published snapshot the EdMode draws from
└── Commandlet/
    └── CkPerfLab_ReportCommandlet.*      – -run=CkPerfLabReport, the headless CI entry
```

The field set and meanings are specified in the campaign's `SCHEMA.md`. That document is the
contract; this module is it expressed as types, and the codec is the only thing permitted to
translate between the two.

---

## Three contracts that are load-bearing

**A metric that was not measured is never a zero.** Every `FCk_PerfLab_MetricStats` carries an
`ECk_Stats_MetricAvailability` and a reason string, and its numbers mean nothing unless
`Get_IsAvailable()`. This exists because a GPU time of zero presented as data reads as an infinitely
fast GPU, and every figure derived from it would be wrong in the flattering direction. The same rule
is enforced one layer down in `CkProfile` and one layer up in the score, which renormalises its
weights over available components rather than scoring a missing one as zero.

**Outliers are counted, not erased.** `Reduce_Samples` classifies by median + k·MAD, then treats the
result asymmetrically on purpose: outliers are excluded from the average so typical behaviour is not
skewed, and retained in the worst case and the 1% low so bad frames keep their impact. The count is
reported either way. A reader who wants the unfiltered picture has it; a reader who wants the typical
one has that too; neither has to trust that something was quietly dropped.

**Encoding is deterministic.** Same object, same bytes: fixed field order, camelCase keys, enums
written by C++ identifier (not display text, not integer), map keys sorted before emission, and
timestamps carried on the object rather than read from a clock. Every export downstream inherits this,
which is what makes two sessions of the same map diffable. The determinism spec is not decoration —
it is the thing that stops a report showing phantom changes between runs that measured the same level.

---

## Anti-patterns

- **Do not add Slate, `UnrealEd`, or any editor-only include.** The runner half runs in `-game`.
- **Do not read a metric's value without checking its availability first.** The type makes this
  possible to get wrong; the specs make it expensive to get wrong.
- **Do not hand-roll a statistic at a call site.** `Mean`, `Median`, `Percentile` and
  `MedianAbsoluteDeviation` live in `ck::algo` (`CkCore/Algorithms/`); grow that library instead.
- **Do not invent a wrapper around `FJsonObject`.** There isn't one in the codebase, and both
  existing JSON producers use the engine type directly.
- **Do not let a `FCk_Handle` or any pointer into the session model.** It crosses a process boundary
  and is read by a module bound by the no-live-handle invariant; object paths are the only currency.

---

## See also

- `Plugins/CkGameplayDebugger/docs/campaigns/2026-08-21-PerfLab/SCHEMA.md` — the file contracts
- `Source/CkOptimizationDebugger/CLAUDE.md` — the consumer, and the no-live-handle invariant
- `Plugins/CkFoundation/Source/CkProfile/Claude.md` — where the timings come from
