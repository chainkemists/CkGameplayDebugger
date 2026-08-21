# PerfLab — PROGRESS.md (living log)

## Current state  <!-- supersedes everything below; update at EVERY gate and session end -->
**As of 2026-08-21 (executor session 1, CkGameplayDebugger `feature/perf-lab` @ `c1afdc4` + uncommitted Phase 0 docs):**
Phase 0 work items 0.1 / 0.2 / 0.3 COMPLETE and verified. Phase 0 not yet committed.
**Baseline being diffed against (captured 2026-08-21, freshly built Editor binary):**
- **Full suite (no pattern):** Total **1235**, Passed **1232**, Failed **3**, Skipped 0, Contaminated 0, 6m06s. Exit 1 (the toolbox's normal way of reporting test failures).
- **Failing names (pre-existing — this session changed zero source files):**
  1. `Angelscript.CppTests.AngelscriptCodeCoverage.IntegrationTest`
  2. `Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_PathNetworkFollower_ProjectsRibbonWaypointWithinNavQueryExtent`
  3. `Project.Functional Tests.CkTests.AutoTests.AutoTests_CkTests_Level.Ck_AutoTest_PathNetworkFollower_DesiredNavmeshClearanceMovesInward`
  (The two PathNetworkFollower reds match a known-flaky area recorded in prior session notes. Not investigated — out of scope, per Phase 0 fences.)
- **`Ck.OptimizationDebugger` (targeted run):** **76/76 green, exit 0, 49s.** The campaign's claimed module baseline is CONFIRMED by direct run, not inherited from PLAN.md.
**Phase 0 gate: CLOSED.** All three work items done; D2 challenged and reinstated on evidence
(DECISIONS_PENDING_REVIEW D-005); two Fable consultations completed.
**Next action:** Phase 1 per [PHASE_1.md](PHASE_1.md) — add the thread-timings reader to CkProfile.
**Blocked on:** nothing.

## Decision log
| Date | Decision | Why | Revisit when |
|---|---|---|---|
| 2026-08-21 | D1–D12 locked in PROMPT.md | See PROMPT.md table + rejected-approaches | A kill reason stops being true |
| 2026-08-21 | Campaign branch is `feature/perf-lab`, not `perflab/phase-N` | Adam had already created it and committed the docs there; repo beats plan | — |
| 2026-08-21 | **D2 CONFIRMED, not amended** — child stays `-game` | Smoke passed once the binary was corrected; the failure was never about `-game` | — |
| 2026-08-21 | **Child binary = the PROJECT's `Binaries/Win64/CkPluginsEditor-Cmd.exe`**, never the engine's `UnrealEditor-Cmd.exe` | `CkPluginsEditor.Target.cs:18` sets `BuildEnvironment = TargetBuildEnvironment.Unique`, so modules are `CkPluginsEditor-*.dll` in the project's own Binaries | Phase 5 must RESOLVE this, not hardcode — and must handle consumer projects that are NOT Unique |
| 2026-08-21 | Every gate uses an EXPLICIT `--test-pattern`; "full suite" is not trusted as full | The no-pattern run executed 1235 of the 4427 tests the editor discovered, and excluded all 76 OptimizationDebugger specs | If the toolbox's no-pattern semantics change |
| 2026-08-21 | Validation strategy: fixtures + budget-manipulation, no authored heavy content | Fable ruling, DECISIONS_PENDING_REVIEW D-003 | Downstream BusterBlock verify passes |

## Dated entries (append-only, newest first)

### 2026-08-21 — Phase 0 executed (executor session 1)

**Ran / confirmed:**
- `UnrealToolbox --build --target=Editor --test` (full suite, auto-sized 3 lanes + net lane) → build clean; counts above. Editor was closed for the build path per the pre-flight table.
- `UnrealToolbox --test --test-pattern OptimizationDebugger --discover-fresh` → **76/76, exit 0**.
- **Engine reads** (0.1) → all recorded with file:line in
  [RESEARCH_EngineApis_Addendum.md](RESEARCH_EngineApis_Addendum.md), which SUPERSEDES the
  agent-sourced RESEARCH_EngineApis.md.
- **Child-launch smoke** (0.2) → PASSED on the 4th attempt; the three failures were diagnostic, see below.
- **File contracts** (0.3) → [SCHEMA.md](SCHEMA.md) written and locked.

**Confirmed (evidence named):**
- `stat unit`'s four numbers come from `FStatUnitData::DrawStat`, **`UnrealClient.cpp:350`** — the prior
  research doc said `UnrealEngine.cpp`; CORRECTED. Raw expressions at `:370, :374, :381, :387, :399`.
- The engine's own GPU-availability test is `bHaveGPUData = RawGPUFrameTime > 0` (`UnrealClient.cpp:514`) —
  independently validates locked decision D5 (never 0-as-data).
- Every metric is stored raw AND EMA-smoothed (`X = 0.9*X + 0.1*RawX`). **PerfLab must read RAW**, or
  the smoothing destroys Phase 2's percentiles/outliers. This was not in the plan; it is now.
- `RHIGetGPUFrameCycles()` is the live API (`DynamicRHI.h:1301`); `GGPUFrameTime` is `UE_DEPRECATED(5.6)`.
  `GUsingNullRHI` is a macro alias (`RHIGlobals.h:840`).
- `IsAsyncLoading()` does NOT exist under that name in this checkout — CORRECTED; the confirmed
  quiescence signals are `GetNumAsyncPackages()` (`UObjectGlobals.h:1119`) +
  `IStreamingManager::GetNumWantingResources()` (`ContentStreaming.h:316`).
- **Child smoke, final run:** project binary + `-game -windowed -resx=1280 -resy=720 -unattended -nosplash`
  + `-ExecCmds="t.MaxFPS 0, r.VSync 0"` → ran the full 220 s bound (exit 124 = we killed it; it never died).
  `LogRHI: Using Default RHI: D3D12` (real rendering). `LoadMap: /CkTests/TestGyms/TestGyms_CkTests_Level`
  took **1.02 s**. `Bringing World ... up for play (max tick rate 0)` — the cvar took effect.
  **Zero** plugin-load failures, **zero** `LogWindows: Error`. Boot→map ≈ 10 s; total ≈ 35 s.
  Log: `Saved/Logs/PerfLabSmoke4.log`.
- **Game class is `CkTests_Gym_Base_GameMode`** — the host is NOT the bare `GameModeBase` the plan
  assumed. Phase 4's camera rig has a real GameMode (and presumably a pawn/controller) to work with;
  Phase 4 must read that class before choosing its teleport path.

**The misdiagnosis, recorded in full (it nearly changed a locked decision):**
Smoke attempt 1 (`-game`, engine's `UnrealEditor-Cmd.exe`) died in ~20 s with
`Plugin 'CkFoundation' failed to load because module 'CkIskmRendererVF' could not be found.`
I hypothesised `-game` resolved against a stale Game target (`CkPlugins.target` is 15 days old and
contains zero `CkIskmRendererVF`) and escalated to a Fable advisor to re-litigate D2.
**That hypothesis was wrong.** Attempt 2 dropped `-game` and failed identically — which killed the
theory. Root cause: `Source/CkPluginsEditor.Target.cs:18` sets
`BuildEnvironment = TargetBuildEnvironment.Unique`, so this project builds its OWN
`Binaries/Win64/CkPluginsEditor-Cmd.exe` and its own 747 `CkPluginsEditor-*.dll` modules; the engine's
stock binary cannot see them. Attempt 3 (correct binary) reached `LoadMap` but the package path came
through as `C:/Program Files/Git/CkTests/...` — Git Bash POSIX path mangling, a shell artifact, fixed
with `MSYS_NO_PATHCONV=1`. Attempt 4 passed. **D2 needed no amendment.** The advisor was sent the
correction before it could rule on the false premise.

**Inferred (unconfirmed):**
- That the two PathNetworkFollower reds are flaky rather than deterministic — not re-run in isolation
  (out of scope; they are simply pinned as baseline names).
- That `FApp::UseFixedTimeStep()` is false in the child — not directly asserted; Phase 4 must check,
  because the frame-time source silently changes if it is true (addendum §1.1).

**Follow-ups recorded, not chased:**
- The no-pattern `--test` covering only 1235 of 4427 discovered tests is worth understanding, but the
  campaign does not depend on it now that every gate names its pattern.
- `ck::debug_axes::Get_ToneIconId` and `ECk_Tone` were not found in `CkDebuggerAxes.h` by a quick grep
  (only `Get_HeatColor` at :283 and `Get_CategoricalColor` at :298/:301). Phase 8 must locate them.

## Open items
| Item | Status | Next step |
|---|---|---|
| Phase 0 commit | Open | Commit the campaign docs + addendum + SCHEMA on `feature/perf-lab` |
| Adjudication A-PerfLab-1 (subprocess placement) | Open — interim stance active | File row in Phase 5; Adam rules |
| ~~Phase 5 binary resolution for NON-Unique consumer projects~~ | **RESOLVED 2026-08-21** | `FPlatformProcess::ExecutablePath()` verbatim; recorded in PHASE_5 + D-005 |
| All `[EDITOR-VERIFY]` items | Open | Accumulate in VALIDATION.md; Adam runs at Phase 9 |

**Rule: no completion claim may be written anywhere in this file while any row here is unresolved.**
