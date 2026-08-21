# Phase 0 — Verification spike & contract lock

> **Status:** ⏳ Pending
> **Depends on:** —
> **Estimate:** 1 session — re-date at entry; record actual at exit
> **Change class:** 1 (no shipped source edits; scratch code + campaign docs only)

## Goal

After this phase: every `[agent]`-tagged engine/API claim the later phases depend on is CONFIRMED or
corrected against the real engine + a real child launch, recorded in `RESEARCH_EngineApis_Addendum.md`;
the session JSON schema v1 and the request/heartbeat file contracts are locked; the full-suite test
baseline is captured.

## Entry criteria (run, don't assume)

- [ ] Read PROMPT.md, RESEARCH_EngineApis.md, RESEARCH_Codebase.md §1–2.
- [ ] Resolve the engine path: `powershell -NoProfile -File CkAuto/Get-ProjectEnginePath.ps1`
      (expected today: `D:\Repositories\UnrealEngine-Angelscript` — record what you get).
- [ ] Both submodules on `dev`, clean (`git -C Plugins/CkGameplayDebugger status`, same for
      CkFoundation). If dirty with files you did not author → STOP, record in PROGRESS.md blockers
      (another session may own them), end session.
- [ ] Create branches: `git -C Plugins/CkGameplayDebugger checkout -b perflab/phase-0`, same in
      `Plugins/CkFoundation`. Never push.
- [ ] Capture the campaign baseline: `CkAuto/UnrealToolbox.exe --test` (full suite), record
      pass/fail counts + every failing name in PROGRESS.md §Current state. Also record
      `--test --test-pattern Ck.OptimizationDebugger` (expect 76/76; if not → record the delta and
      the names; do NOT debug them — they are pre-existing, note prime-suspect per memory
      `unreal-toolbox-test-gotchas`).

## Work items

### 0.1 Engine source reads (no code)

For each, paste the actual declaration/expression into the addendum with file:line:

1. `FStatUnitData::DrawStat` in `<Engine>/Engine/Source/Runtime/Engine/Private/UnrealEngine.cpp` —
   extract the EXACT expressions producing the four `stat unit` numbers (Frame/Game/Render/GPU),
   including any use of `GGameThreadTime` vs `GGameThreadTimeCriticalPath`, `RawFrameTime`
   smoothing, and unit conversion. These expressions ARE the Phase 1 implementation spec.
2. `RenderTimer.h:105-129` — re-confirm the globals (planner-confirmed 2026-08-21; cheap re-check).
3. `RHIGetGPUFrameCycles` in `DynamicRHI.h` + its implementation — confirm 0-return semantics on
   unavailable timestamps, and check for a per-GPU/history API in this checkout (the deprecation
   comments reference a replacement).
4. Streaming-quiescence signals: `IsAsyncLoading()`, `GetNumAsyncPackages()`,
   `IStreamingManager::Get()` in-flight counts, `UWorld::GetStreamingLevels()` states — confirm each
   symbol exists and is readable from game code in this checkout; list what a `-game` instance can
   actually observe.
5. `UNavigationSystemV1::GetRandomPoint` / `ProjectPointToNavigation` and how to detect
   "map has no navmesh data" cheaply.
6. `UGameInstanceSubsystem` boot ordering vs map load in `-game` mode — confirm a subsystem can read
   `FCommandLine` at `Initialize` and defer its run until `PostLoadMap` (find the exact delegate:
   `FCoreUObjectDelegates::PostLoadMapWithWorld`).
7. Read `Plugins/GitLink/Source/GitLink/Private/GitLink_Subprocess.{h,cpp}` AND
   `<Engine>/Engine/Source/Runtime/Core/Public/Misc/MonitoredProcess.h`; record the Phase-5
   launcher decision (raw `CreateProc` per GitLink vs `FMonitoredProcess`) with one paragraph of
   rationale in the addendum.

### 0.2 Child-launch smoke (scratch only — no shipped code)

Executable spec = repro command with captured output:

1. Pick the cheapest map with content in the host project or CkTests (record which; if none suits, a
   default engine template map is acceptable for the smoke).
2. Run (PowerShell, paths from the resolved engine):
   ```
   & "<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<abs>\CkPlugins.uproject" <MapPath> -game -windowed -resx=1280 -resy=720 -unattended -nosplash -log -ExecCmds="t.MaxFPS 0, r.VSync 0"
   ```
   with a manual kill after ~60 s if it doesn't exit.
3. From its log, confirm: the map loaded; the process renders (no nullrhi); note boot-to-map wall
   time (informs host timeout defaults). **Also record what `-game` gives us to fly**: which
   GameMode/pawn/PlayerController spawns with this host's bare `GameModeBase`, whether a spectator
   pawn is obtainable (`ASpectatorPawn` / `PlayerController->ChangeState(NAME_Spectating)`), and
   which teleport+rotate path is viable — this observation is Phase 4's camera-rig decision input;
   write the chosen mechanism into the addendum so Phase 4 implements, not designs.
4. Repeat once with `-nullrhi` and record what dies (expected: rendering; GT still ticks).
5. Paste both command lines + the relevant log excerpts into the addendum.

**Decision gate:** if `-game` on this host project fails to load a map at all (plugin boot error,
AS compile gate, etc.) → STOP, record the exact log verdict in PROGRESS.md blockers, and surface to
Adam — D2 (child = `-game`) would need re-litigating and that is not the executor's call.

### 0.3 Lock the file contracts (docs only)

Write `SCHEMA.md` in this campaign folder pinning, as JSON examples with every field commented:

- `request.json` v1: map path, mode preset name + resolved parameters (positionBudget,
  directionsPerPosition, repeats, settle {warmupFrames, stabilityCV, stabilityFrames, timeoutSec},
  warmSweep on/off, adaptive on/off), budgetMs, seed, sessionId, requestingHostPid, schemaVersion.
- `heartbeat.json` v1: sessionId, state enum (Booting/Planning/WarmSweep/Measuring/Writing/Done/
  Failed), positionsDone/positionsTotal, currentPositionId, lastUpdateUtc, schemaVersion.
- `session.json` v1: schemaVersion, request echo, environment block (config, instanceMode, RHI name,
  GPU/CPU brand via `UCk_Utils_Stats_UE`, machine name, engine version, plugin build, map, UTC
  timestamps), positions[] {id, location, eyeOffset, directions[] {yaw, samples summary}},
  per-position aggregates {avgMs, worstMs, p99Ms, onePercentLowMs, outlierCount, sampleCount,
  confidence {rating enum + component reasons}}, per-metric availability {available|unavailable,
  reason enum}, actorCensus[] {objectPath, className, distance, inView, costProxies {triCount?,
  lightCount?, niagaraCount?, tickEnabled?}}, and an `analysis` block reserved (empty in v1 runner
  output; filled host-side).
- Key order/naming: camelCase, fixed field order, `FCk_JsonReport` conventions
  (RESEARCH_Codebase.md §3 row 1).

## Expected observations at the gate — branches

| I will run | I expect | If instead | Response |
|---|---|---|---|
| Engine greps/reads 0.1 | All symbols found; stat-unit expressions extracted | A symbol missing/renamed | Record the real shape; amend the affected PHASE file's signatures in the same session (plans snapshot conventions — code wins) |
| Smoke 0.2 step 2 | Map loads, renders, log shows normal tick | Boot failure | STOP per decision gate above |
| Smoke 0.2 `-nullrhi` | Runs; no RHI timings possible | Crashes | Record verdict; nullrhi fast-mode becomes a v1 non-goal note in PROMPT.md (dated edit) |
| Full-suite baseline | Counts recorded | Toolbox exit 75/77/79 | Follow build-test skill's exit-code table; do not bypass the toolbox |

## Exit criteria — ALL in the SAME commit (campaign docs commit on `perflab/phase-0`)

- [ ] `RESEARCH_EngineApis_Addendum.md` exists; every `[agent]` claim in RESEARCH_EngineApis.md is
      marked CONFIRMED (with file:line/log evidence) or CORRECTED (with the fix propagated into the
      affected PHASE files, edits dated).
- [ ] `SCHEMA.md` committed; later phases cite it instead of re-deriving.
- [ ] Baseline counts + failing names in PROGRESS.md §Current state; PROGRESS.md dated entry.
- [ ] PLAN.md status row + this file's Status header → ✅ Done (same commit).

## Fences

- No shipped source files. Scratch code goes to the session scratchpad, not the repo.
- Do not "fix" any pre-existing red test found in the baseline — record and move on.
- Do not install/enable engine plugins to make the smoke pass — a needed plugin is a finding, not an
  action.
