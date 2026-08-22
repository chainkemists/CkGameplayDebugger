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
**Phase 0 gate: CLOSED.** D2 challenged and reinstated on evidence (DECISIONS_PENDING_REVIEW D-005).
**Phase 1 gate: CLOSED.** `Get_ThreadTimings()` shipped; targeted gate 2/2 → 7/7 green.
**⚠ Use the CORRECTED baseline for all later diffs: 1244 total / 1241 passed / 3 failed** (the Phase 0
figure of 1235/1232/3 was captured with AngelScript partially regenerated — see the Phase 1 entry).
**Branches (local only, never pushed):** CkGameplayDebugger `feature/perf-lab`, CkFoundation
`feature/perf-lab`, CkTests `feature/perf-lab`.
**Next action:** Phase 2 per [PHASE_2.md](PHASE_2.md) — scaffold the `CkPerfLab` module (session
model, statistics, codec). Note PHASE_2's Build.cs section was corrected in Phase 0: modules here
inherit `CkModuleRules` and must link `CkEcs`.
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

### 2026-08-21 — Phase 1 complete: thread-timings surface in CkProfile

**Shipped** (CkFoundation `feature/perf-lab`, CkTests `feature/perf-lab`):
- `ECk_Stats_MetricAvailability` + `FCk_Stats_ThreadTimings` + `UCk_Utils_Stats_UE::Get_ThreadTimings()`
  in `Source/CkProfile/Public/CkProfile/Stats/CkStats_Utils.{h,cpp}`; `RenderCore` + `RHI` added to
  `CkProfile.Build.cs` with justification comments. `CkProfile/Claude.md` gains a **Thread timings**
  section carrying both contracts (raw-not-smoothed; availability-never-zero).
- 5 specs in `Plugins/CkTests/.../UnitTests/CkProfile/Test_Profile_ThreadTimings.cpp`.

**Gate — `--test-pattern CkProfile`:** baseline **2/2** → **7/7 green, exit 0**. Build clean.

**Full-suite regression check (same invocation as the Phase 0 baseline):**
`1244 total / 1241 passed / 3 failed`. **No regression attributable to this change**, established as:
1. The two deterministic reds are byte-identical to baseline (both PathNetworkFollower).
2. The third red — `Ck_AutoTest_ScriptProcessor_PumpStopsAfterMarkerDrain` — **passes in isolation
   (1/1, exit 0)**, and the change is additive C++ with no call site anywhere near the ECS scheduler
   or script pump. Classified **flake**, with the reason, not waved away.
3. Baseline's third red (`Angelscript.CppTests.AngelscriptCodeCoverage.IntegrationTest`) now PASSES.
   The suite's documented behaviour is exactly one rotating extra failure per run; this run's shape
   matches the baseline's.

**⚠ Correction to the Phase 0 baseline — it was under-counted.** The total moved 1235 → 1244 while
this phase added only 5 tests. The other 4 are pre-existing AngelScript autotests
(`Ck_AutoTest_Queue_*`, committed long before this campaign — `5ffbcc79`, `7f043ced`, `9902cfe9`)
that the **Phase 0 run failed to register**. That run also had the AngelScript coverage test failing.
Read together: the Phase 0 baseline was captured with AngelScript in a partially-regenerated state,
and this phase's clean rebuild healed it. **The honest baseline going forward is this run's
1244/1241/3, not Phase 0's 1235/1232/3** — later phases must diff against the former. CkTests'
working tree held nothing but my new file throughout (`git status` = one `??` entry), so no
uncommitted sibling work explains the difference.

**Confirmed:** build clean (0 errors); 7/7 on the targeted gate; isolation run of the flake.
**Inferred (unconfirmed):** that the pump test's flake is timing/lane contention rather than a latent
ordering bug — it passed alone and its failure predates nothing I touched, but I did not root-cause
it. **`[EDITOR-VERIFY]` owed:** the BP node and the AngelScript `utils_stats::Get_ThreadTimings()`
binding (rows added to VALIDATION.md; agents cannot boot the editor to confirm either).

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
