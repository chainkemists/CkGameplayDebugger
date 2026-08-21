# Phase 2 — CkPerfLab module: session model, statistics, codec

> **Status:** ⏳ Pending
> **Depends on:** Phase 1 ✅
> **Estimate:** 1–2 sessions
> **Change class:** 2 (new module, additive)

## Goal

After this phase: `CkPerfLab` exists as a UI-free DeveloperTool module in CkGameplayDebugger whose
pure layer — session model structs, sampling statistics, confidence rating, and a versioned JSON
codec that round-trips `session.json`/`request.json`/`heartbeat.json` per SCHEMA.md — is fully
spec-tested with zero editor involvement.

## Entry criteria

- [ ] Load `CkOptimizationDebugger/CLAUDE.md` (constraints), `ck-macros-and-codegen`; read SCHEMA.md.
- [ ] Read one full small module for scaffold mimicry: `Source/CkInsightsAnalyzer/` layout +
      `CkInsightsAnalyzer.Build.cs` + its `Claude.md` (the UI-free DeveloperTool exemplar), and
      `Source/CkTimer/CkTimer_Log.h` (log template).
- [ ] Read `CkOptimizationDebugger_SnapshotCodec.{h,cpp}` (versioning/whole-or-nothing discipline)
      and `CkInsightsAnalyzer/Report/CkJsonReport.{h,cpp}` + `CkMultiFrameReport.h` (JSON + stats
      shapes).
- [ ] Branch `perflab/phase-2` in CkGameplayDebugger; baseline `--test --test-pattern Ck.` roots
      affected: record `Ck.OptimizationDebugger` (expect 76/76) — PerfLab has no specs yet.

## Work items

### 2.1 Module scaffold — NEW INFRASTRUCTURE (named as such)

- `Plugins/CkGameplayDebugger/Source/CkPerfLab/`: `CkPerfLab.Build.cs`, `CkPerfLab_Module.{h,cpp}`,
  `CkPerfLab_Log.{h,cpp}` (`namespace ck::perf_lab { CK_DEFINE_LOG_FUNCTIONS(CkPerfLab); }`),
  `Claude.md` stub.
- **Build.cs shape — CORRECTED 2026-08-21 (Phase 0), the original text here was wrong.** This
  plugin's modules DO inherit `CkModuleRules` across the plugin boundary; verified against
  `Source/CkOptimizationDebugger/CkOptimizationDebugger.Build.cs`, which opens
  `public class CkOptimizationDebugger : CkModuleRules`. Do **not** write a plain `ModuleRules`
  class, and do **not** copy GitLink's shape. Copy that file's shape exactly, including its
  **per-dependency justification comments** — a bare dep list will not pass review here.
- **`CkEcs` is mandatory, not optional.** That file's own comment states the rule verbatim:
  `"CkEcs",  // CkCore's SharedPCH instantiates global ECS registrations — every CK module must link CkEcs`.
  Omitting it produces link errors that look unrelated to your change.
- Deps (initial, minimal): `Core`, `CoreUObject`, `Engine`, `Json`, `CkCore`, **`CkEcs`**, `CkLog`,
  `CkProfile`, plus `RenderCore` + `RHI` if the Phase 1 reader's headers are included here rather
  than only in CkProfile. Add every later dep in the commit that consumes it, each with its
  justification comment (P7 hygiene rule).
- `CkDebugger.uplugin`: add the module entry matching the sibling shape exactly (verified against
  the `CkOptimizationDebugger` and `CkSaveDebuggerEditor` entries — tab-indented, keys in the order
  `Name`, `Type`, `LoadingPhase`, `WhitelistPlatforms`, platforms `Win64`/`Mac`/`Linux`):
  `{"Name":"CkPerfLab","Type":"DeveloperTool","LoadingPhase":"Default", "WhitelistPlatforms":[...]}`.
- **Fence:** no dependency on `CkInsightsAnalyzer` (drags TraceServices) and none on
  `CkOptimizationDebugger` (the debugger will depend on PerfLab, not vice versa — keep the DAG
  UI → engine).

### 2.2 Session model (`Public/CkPerfLab/Session/CkPerfLab_Session.h` + siblings)

Reflected USTRUCTs mirroring SCHEMA.md 1:1 (house encapsulation shape — `_Members`,
`CK_PROPERTY_GET`, `CK_DEFINE_CONSTRUCTORS`, formatter macro per enum):

- `FCk_PerfLab_Request`, `FCk_PerfLab_ModePreset`, `FCk_PerfLab_SettleParams`
- `FCk_PerfLab_Environment`, `FCk_PerfLab_MetricAvailability` (metric enum + reason — reuses
  `ECk_Stats_MetricAvailability` from Phase 1 where it fits)
- `FCk_PerfLab_DirectionSample` (yaw + summary), `FCk_PerfLab_PositionResult` (aggregates +
  confidence + availability + actor census rows), `FCk_PerfLab_ActorCensusRow`
- `FCk_PerfLab_Session` (root)
- Enums: `ECk_PerfLab_Mode {Quick, Standard, Deep}`, `ECk_PerfLab_RunState`,
  `ECk_PerfLab_Confidence {High, Medium, Low, Unusable}` — each with
  `CK_DEFINE_CUSTOM_FORMATTER_ENUM`.

### 2.3 Statistics (`Public/CkPerfLab/Stats/CkPerfLab_SampleStats.h/.cpp` — pure, no UObject)

- Input: `TArray<float>` of accepted per-frame ms for one dwell; output struct: avg (outlier-excluded),
  worst (outlier-included), p99, 1% low (mean of worst 1% — define exactly; LPD vocabulary), sample
  count, outlier count.
- Outlier rule: median + k·MAD (k from settings, default 3.5) — **implement `Median`, `Mad`,
  `Percentile` in `CkCore/Algorithms/CkAlgorithms.h` + `.inl.h` + README row** (doctrine: grow the
  library, never hand-roll — this touches CkFoundation on the `perflab/phase-2` branch there; keep
  the CkFoundation diff to exactly these additions + their specs).
- Confidence rating: pure function over (warmup converged?, streaming was quiet?, sampleCount vs
  target, coefficient of variation) → `ECk_PerfLab_Confidence` + per-component reason list. Thresholds
  as parameters, not constants.

### 2.4 Codec (`Public/CkPerfLab/Session/CkPerfLab_SessionCodec.h/.cpp`)

- `TryWrite_*` / `TryRead_*` for the three files; `TSharedPtr<FJsonObject>` directly (no wrapper —
  ruled). SnapshotCodec discipline: `schemaVersion` checked first, whole-or-nothing decode, unknown
  version → typed failure, never partial data.
- Determinism: fixed field order, camelCase, explicit sorts with final tie-break (position id),
  timestamps handed in — pinned by spec 2.5.4.

### 2.5 Specs (`Private/…​.spec.cpp`, root `Ck.PerfLab`)

1. `Ck.PerfLab.Stats.*` — known arrays → exact expected avg/worst/p99/1%-low/outliers (include: empty
   array, single sample, all-identical, one huge outlier, exactly-1% boundary).
2. `Ck.PerfLab.Confidence.*` — each degradation path flips the rating the spec names.
3. `Ck.PerfLab.Codec.RoundTrip` — build a session in memory → write → read → field-equal.
4. `Ck.PerfLab.Codec.Determinism` — write twice with the same handed-in timestamp → byte-identical;
   unknown schemaVersion → typed failure, no partial object.
5. `Ck.PerfLab.Codec.GpuUnavailable` — a session with GPU `Unavailable_*` round-trips the reason and
   never materialises a 0.0 "value used" flag.

## Expected observations — branches

| I will run | I expect | If instead | Response |
|---|---|---|---|
| `--build --target=Editor` after scaffold | Compiles; module loads (log line from `StartupModule`) | uplugin parse/module-not-found | Diff your uplugin entry against a sibling byte-for-byte |
| `--test --test-pattern Ck.PerfLab --discover-fresh` | New specs discovered + green | 0 tests discovered | You skipped `--discover-fresh` without `--build` (P15 note) — rerun per build-test skill |
| `--test-pattern Ck.OptimizationDebugger` | Still 76/76 | Δ | Your uplugin/Build.cs change is the prime suspect — A/B stash before debugging further; anything else → STOP, PROGRESS blockers |

## Exit criteria — same commit

- [ ] All 2.5 specs green; counts recorded (`Ck.PerfLab 0 → N green`); OptimizationDebugger still
      76/76 **on the final binary**.
- [ ] `CkAlgorithms` additions have their own specs + README row (CkFoundation side), gate
      `Ck.Core`-root pattern re-run there.
- [ ] `Source/CkPerfLab/CLAUDE.md` started: module contract, the UI-free rule, the no-TraceServices
      and no-OptimizationDebugger-dep fences, SCHEMA.md pointer.
- [ ] PLAN.md row + Status header + PROGRESS.md entry.

## Fences

- No sampling loops, no world access, no subprocess code in this phase — pure data + math + IO only.
- Reflected structs: enum+value availability modelling, NOT `TOptional` (adjudication A1 interim).
- No `FPlatformApplicationMisc` anywhere, ever (suite ban).
