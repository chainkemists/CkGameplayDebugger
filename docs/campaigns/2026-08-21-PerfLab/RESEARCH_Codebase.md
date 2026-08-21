# PerfLab — codebase research: what exists, what to reuse, what is fenced

> **Written:** 2026-08-21 (Fable planning session, two very-thorough Explore agents + planner
> spot-checks). File:line cites were read that day. Re-verify on first touch — code moves.
> **This doc dies when:** the campaign ships; permanent content moves to module `CLAUDE.md`s.

Planner-verified anchors (read directly this session): `CkOptimizationDebugger/PLAN.md` P7/P9/P12/P18
rows and gate counts; `_Design/2026-08-15-capability-review-and-gaps.md` in full;
`CLAUDE.md:44` (§"The no-live-handle invariant"); campaign dir precedent
(`docs/campaigns/2026-08-10-PackagedGraphDebuggers`, `2026-08-14-JoltDebuggerWorldViewport`);
engine timing globals (see RESEARCH_EngineApis.md). Everything else: agent-read, path-cited.

## 1. CkOptimizationDebugger — the tool PerfLab augments

`Plugins/CkGameplayDebugger/Source/CkOptimizationDebugger/` — DeveloperTool Slate toolkit,
**gate `Ck.OptimizationDebugger` 76/76 green as of P18 (2026-08-21)**. Six window pages (Dashboard,
Level analysis, Memory, Profiling, Cleanup, Snapshots) in
`Public/CkOptimizationDebugger/Window/SCkOptimizationDebuggerWindow.cpp`. Load-bearing docs:
`CLAUDE.md` (1798 lines — the authoring doctrine, READ FIRST), `PLAN.md` (18 phases, 50 numbered
decisions), `_Design/2026-08-15-capability-review-and-gaps.md`,
`_Design/2026-08-21-fix-trust-and-audit-parity.md`.

Already shipped and directly reusable:

| Capability | Where |
|---|---|
| 28 static checks, 7 families, one contract `Run_Checks(Context, Thresholds, OutFindings)` | `Analysis/Checks/CkOptimizationDebugger_Checks_*.{h,cpp}` |
| One-gather scan context; stable finding keys via `Build_Finding` | `Analysis/CkOptimizationDebugger_ScanContext.h` |
| Pure deterministic HTML+MD findings export (landed P18, with determinism spec) | `Model/CkOptimizationDebugger_FindingsReport.{h,cpp}` |
| Pure deterministic snapshot HTML report (P12) | `Model/CkOptimizationDebugger_SnapshotReport.{h,cpp}` |
| Versioned binary session file (`.cksnap` v2: POV, `sg.*` context, build version, aux images) | `Model/CkOptimizationDebugger_SnapshotCodec.{h,cpp}` |
| 11 pure heatmap lenses + mandatory legend lines | `Model/CkOptimizationDebugger_SnapshotLens.h` |
| Session A/B delta keyed by asset (decision #48) | `Build_SnapshotDelta`, `Set_Summary` `_PreviousSummary` rotation |
| Viewport stat/viewmode driving (27-entry catalog incl. `stat unit`, `profilegpu`) — reads live, records nothing (decision #22) | `Commands/CkOptimizationDebugger_ProfileCommands.{h,cpp}` |
| Capture view/world resolution + `ViewOverride` ("recapture from here") | `Analysis/CkOptimizationDebugger_SnapshotCapture.h` |
| Suppression/mute (StableKey-keyed, visible count, `MUTED` chip) | `Model/CkOptimizationDebugger_Suppression.{h,cpp}` |
| Per-user reflection-driven thresholds | `Analysis/CkOptimizationDebugger_Thresholds.h` + settings |

**Binding constraints (from its CLAUDE.md/PLAN.md — violations are review rejections):**

1. **No-live-handle invariant** (`CLAUDE.md` §44): the module holds no `FCk_Handle`, opens no PIE,
   `SCkOptimizationDebuggerWindow` overrides no `Tick`; rebuilds are event-driven
   (`RegisterActiveTimer` is the sanctioned incremental idiom, per P17). Actor-targeted data is plain
   data (object-path strings) and is dropped at the PIE boundary via
   `ck::DebugSessionLifecycle::Get_OnSessionInvalidated()`. **PerfLab's per-tick sampling therefore
   lives in the child process / a separate module — never inside the window.**
2. **Exports land WITH a determinism spec or not at all** (explicit sorts + final tie-break, fixed
   camelCase field set, nothing time-/pointer-/environment-derived; timestamp handed in).
3. `Json`/`DesktopPlatform` dependency hygiene: add deps in the same commit as the consumer
   (P7 removed pre-declared ones).
4. Spec gate: adding specs needs `--discover-fresh` if not `--build` in the same toolbox invocation
   (P15 note). Current baseline: **76/76**.
5. Severity/tone/icon come off the shared axis (`ck::debug_axes::Get_SeverityTone/Get_ToneIconId/
   Get_HeatColor`) — never bespoke glyphs or colours.

**Known gaps its own review already filed (we are building several):** report export (landed P18 for
HTML/MD; JSON/CSV still open), score/trend (Gap 3 — "gated on the export", export now exists),
CI commandlet `-run=CkOptimizationAudit` (Gap 4), `Passed` outcome (resolution #4: revisit alongside
export).

## 2. The debugger suite plumbing (CkGameplayDebugger)

- **Launcher registration**: `CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.{h,cpp}`; recipe in
  `CkDebuggerLauncher/CLAUDE.md` ("Adding another standalone debugger": nomad tab spawner →
  `FCkDebuggerToolDescriptor` (owner module, tab ID, label, existing SVG basename, category,
  deterministic order, generation token) → unregister discipline → **add the tab ID to
  `CkDebuggerLauncherCatalog.spec.cpp`**). OptimizationDebugger = Tools / slot 40 / `Stopwatch`.
- **Shared widgets**: `CkDebuggerCommon/Widgets/` — `SCkDebug_StatPair`, `SCkDebug_MeterBar`,
  `SCkDebug_CountBadge`, `SCkDebug_StatusPill`, `SCkDebug_SearchBar`, `SCkDebug_InspectorPanel`,
  `SCkDebug_SectionHeader`, `SCkDebug_UnderlineTabs`, …
- **Axes**: `CkDebuggerCommon/Styles/CkDebuggerAxes.h` — `Get_HeatColor`, `Get_CategoricalColor`,
  `Get_ToneIconId`, `ECk_Tone`.
- **PIE-boundary invalidation**: `CkDebuggerCommon/Lifecycle/` —
  `ck::DebugSessionLifecycle::Get_OnSessionInvalidated()`.
- **Clipboard**: `ck::DebugCopyMenu` (`CkDebuggerCommon/Utils/`) — `FPlatformApplicationMisc` banned.
- **Module-type policy** (PLAN.md decision #2, planner-confirmed via agent): `DeveloperTool` for
  tools (ships packaged Development/DebugGame, excluded Test/Shipping); an `Editor` module is
  justified **only to host a reflected EdMode** — which the heatmap is, so
  `CkOptimizationDebuggerEditor` is consistent with (not a reversal of) the policy.
- **Editor-viewport 3D drawing precedent**:
  `CkSaveDebuggerEditor/{Public,Private}/…/Visualizer/CkSaveDebugger_VisualizerEdMode.{h,cpp}` —
  hidden auto-discovered `UBaseLegacyWidgetEdMode`, `Render(FSceneView*, FViewport*,
  FPrimitiveDrawInterface*)` + `HandleClick` + `HCkSaveDebuggerViz_HitProxy`; **stateless: draws from
  an immutable published snapshot via a slot, clicks push back through the slot**. This is the heatmap
  EdMode's template (it itself copies `CkVoxelNavPreview_EdMode`).
- **Timing-over-time UI precedent**: `CkSchedulerDebugger` (`Data/CkSchedulerDebugger_Types.h` —
  `MainPassTimeMs`, `TimingHistory`; `Mockups/mockup_frame_history.html`).
- **Suppression gate**: any drawing honours `ck::debug_draw::Is_SuppressedForStreamerMode()`
  (`CkCore/Debug/`).

## 3. CkFoundation — reuse shortlist (top 10, ranked by spec removed)

| # | Utility | Path | Buys |
|---|---|---|---|
| 1 | `FCk_MultiFrameStats` (avg/min/max/P95/P99, sorted durations, worst/hot frames) + `FCk_FrameReport` + `FCk_JsonReport` (schema-versioned camelCase JSON) | `Source/CkInsightsAnalyzer/Public/CkInsightsAnalyzer/Report/` | Statistics + JSON shape precedents. **Mimic the shapes; do not link CkInsightsAnalyzer from the recorder** (it drags TraceServices; Phase 2 entry decision). |
| 2 | `UCk_Utils_Stats_UE` — FPS, frame ms, frame count, RAM/VRAM, CPU/GPU brand, build config, memory pressure | `Source/CkProfile/Public/CkProfile/Stats/CkStats_Utils.h` | The recorder's environment block + the home for the new GT/RT/GPU readers (Phase 1). |
| 3 | `FCk_Eqs_Algorithm::DoGenerate_{SimpleGrid,Grid,Donut,Cone}` + budgeted resumable `DoRunTests` | `Source/CkEqs/Public/CkEqs/Query/CkEqs_Algorithm.h` | Candidate-point generation shapes for the position planner. |
| 4 | `UCk_Utils_DebugDraw_UE` (~35 primitives) / `CkDebugScene` (retained geometry + picking) | `Source/CkCore/Public/CkCore/Debug/CkDebugDraw_Utils.h`, `Source/CkDebugScene/` | Cheap-path drawing; the EdMode PDI path is primary for the heatmap. |
| 5 | `FCk_Chrono` + `FCk_Time` + `UCk_Utils_Time_UE::Get_WorldTime` (**never `World->GetTimeSeconds()`** — README) | `Source/CkCore/Public/CkCore/{Chrono,Time}/` | Settle windows, warmup timers, per-sample durations. |
| 6 | `ck::algo` (Filter/Sort/Transform/FindIf/CountIf/…) + `ck::Technique` (named-step pipeline) | `Source/CkCore/Public/CkCore/Algorithms/CkAlgorithms.h`, `…/Technique/CkTechnique.h` | Sample-set math; the runner's sample→settle→record→analyze pipeline. **Doctrine: missing algos (Percentile, MAD, StdDev) get ADDED to CkAlgorithms + README, never hand-rolled at call sites.** |
| 7 | `ck::Format_UE` + `CK_DEFINE_LOG_FUNCTIONS` + `CK_ENSURE_IF_NOT` | `Source/CkCore/Public/CkCore/{Format,Log,Ensure}/`; template: `Source/CkTimer/CkTimer_Log.h` | Non-negotiable compliance; per-module log namespace. |
| 8 | `UCk_Utils_OwningActor_UE` (actor↔entity; note: **there is no `EntityBridge` class** — older docs lie) | `Source/CkEcs/Public/CkEcs/OwningActor/CkOwningActor_Utils.h` | Only if entity attribution is ever needed; v1 contributors are actor-path based, no ECS. |
| 9 | `FCk_ValueRange<T>` (+ `CkCurve_Utils`, `CkGeometry_Utils`) | `Source/CkCore/Public/CkCore/Math/ValueRange/CkValueRange.h` | Heatmap normalisation, budget bands, radii. "Bounded range? → ValueRange." |
| 10 | `CkAssetExporter` dispatch/summary/commandlet + CSV (`DataTableExporter`); `CkSnapshot` SlotMeta/Sha256/Diff | `Source/CkAssetExporter/Public/…/{Dispatch,Commandlet,DataTableExporter}/`, `Source/CkSnapshot/…/{SaveGame,Inspection}/` | Batch-job → per-entry results + summary → files + nonzero exit; session metadata/hash/diff shapes. |

JSON: **no Ck FJsonObject wrapper exists** — both producers use `<Dom/JsonObject.h>` directly with
`"Json"` in Build.cs. Follow that; do not invent a wrapper.

## 4. Gaps — genuinely net-new (nothing to copy)

1. **GT/RT/GPU timing readers** — zero repo hits for `GGameThreadTime`/`GRenderThreadTime`/
   `RHIGetGPUFrameCycles`. `Get_FrameTimeMs()` is wall-clock only. → Phase 1.
2. **Process launch** — zero `FPlatformProcess::CreateProc` in CkFoundation; the in-house exemplar is
   `Plugins/GitLink/Source/GitLink/Private/GitLink_Subprocess.{h,cpp}`. → Phase 5 (+ PROMPT.md
   adjudication row on where the utility lives).
3. **Position planner / camera rig / settle machinery** — no path/waypoint generator over a level
   exists anywhere. → Phases 3–4.
4. **Numeric score** — severity counts exist; score does not (`Passed` as a severity was REJECTED,
   resolution #4 — the score lives beside the export, not in the severity enum). → Phase 6.
5. **CSV + JSON findings export** — only HTML/MD exist. → Phase 9.
6. **Measured-evidence check family** — all 28 checks are static. → Phase 6.
7. **`CkOptimizationDebuggerEditor` module** (EdMode host). → Phase 8.

## 5. Build/test/coding-standard pointers (executor loads these, this doc does not restate them)

- Skills (all under `Plugins/CkFoundation/.claude/skills/`): `ck-methodology` (campaign discipline),
  `ck-change-control` (gates per change class), `ck-performance-and-analysis` (+
  `references/measurement-entry-points.md`), `ck-plan-handoff`, `ck-debugging-playbook`,
  `ck-failure-archaeology`, `ck-macros-and-codegen`. Tests: CkTests'
  `ck-tests-authoring-and-running`.
- Style doctrine of record: `Plugins/CkFoundation/CLAUDE.md` + `Source/CLAUDE.md`. Highest-frequency
  rules for this campaign: trailing returns (except UFUNCTION decls), `CK_ENSURE_IF_NOT` with the
  body as the failure path, `ck::IsValid`/`NOT`, `_Member`+`CK_PROPERTY_GET`+`CK_DEFINE_CONSTRUCTORS`,
  request-struct pattern, no UFUNCTION overloads, `CK_DEFINE_CUSTOM_FORMATTER_ENUM` on every UENUM,
  fmt-style logging, no anonymous namespaces (named `ck_<file>` namespaces), CRLF, Allman,
  no what-comments/process breadcrumbs + comment audit before done.
- **TOptional in reflected surfaces is OPEN fork A1** (`.claude/reports/ADJUDICATIONS.md`) — interim:
  enum-mode + value pair in reflected structs (`ECk_EnableDisable` etc.); `TOptional` fine in
  non-reflected C++. PerfLab's "GPU unavailable" therefore models as
  `ECk_PerfLab_ComponentAvailability` + reason enum + value, not `TOptional<float>`, in USTRUCTs.
- Build/tests run ONLY via UnrealToolbox (`/build-test` skill;
  `CkAuto/.claude/skills/build-test/SKILL.md`). Spec pattern rows are plugin-name-rooted
  (`Ck.OptimizationDebugger`, future `Ck.PerfLab`). Exit codes: 75 busy / 76 AS compile / 77 editor
  open / 78 contaminated / 79 config flip.
- AngelScript: **PerfLab is C++-only** (AS has no Slate/FJsonObject/FPlatformProcess/commandlets).
  Any BPFL exposed must be named `*_UE` (AS namespace generator strips suffixes; §16.1 of
  `Script/CLAUDE.md`).
- Versioning: CkGameplayDebugger has no plugin-wide version log; GitLink's three-place pattern applies
  only to GitLink. Module `CLAUDE.md` updates are the required doc artifact here.
