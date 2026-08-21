# Phase 4 — In-child measurement runner

> **Status:** ⏳ Pending
> **Depends on:** Phase 3 ✅
> **Estimate:** 1–2 sessions (the campaign's highest-unknown phase — NEW INFRASTRUCTURE throughout)
> **Change class:** 2

## Goal

After this phase: launching the project `-game` with `-CkPerfLab-Request=<file>` runs the whole
measurement autonomously — plan, warm sweep, per-position/direction settle+sample via the Phase 1
readers, census, progressive session write, heartbeat, self-timeout — and exits, leaving a valid
`session.json` the Phase 2 codec reads back.

## Entry criteria

- [ ] Phase 0 addendum: subsystem boot ordering, `PostLoadMapWithWorld`, streaming signals, smoke-run
      boot time. Read `CkPieLayoutEditor_Subsystem.h` (`FCk_Chrono`-over-`FTSTicker` retry idiom —
      the closest house precedent for "tooling that drives frames").
- [ ] Branch `perflab/phase-4`; baseline `Ck.PerfLab` counts.

## Work items

### 4.1 Entry + lifecycle (`Public/CkPerfLab/Runner/CkPerfLab_Runner_Subsystem.h/.cpp`)

- `UCk_PerfLab_Runner_Subsystem : UGameInstanceSubsystem` (DeveloperTool module — exists in `-game`
  Development). `Initialize`: parse `-CkPerfLab-Request=`; absent → subsystem stays inert (zero cost
  in normal runs — spec-pinnable via a `Get_IsArmed` accessor). Present → validate request via codec
  (bad file → write `Failed` heartbeat + `RequestEngineExit`; never a silent hang).
- Arm on `FCoreUObjectDelegates::PostLoadMapWithWorld`; verify the loaded map is the requested one
  (mismatch → typed failure, exit).
- Force the measurement environment once armed: `r.VSync 0`, `t.MaxFPS 0`,
  `GEngine->bSmoothFrameRate=false`, screen percentage pinned, `ck.Debug.StreamerMode` untouched.
  Record every cvar it forced into the session's environment block (honesty: the run states its own
  conditions).
- Self-watchdog: absolute wall-clock budget from the request; exceeded → flush partial session with
  `state=Failed_Timeout` + exit. (The host adds its own outer timeout in Phase 5 — belt and braces.)

### 4.0 Environment assertion — refuse to measure, never hope (NEW, Fable-raised 2026-08-21)

Before the first sample, the runner asserts its measurement environment and **refuses to run** (typed
failure + heartbeat reason) if any check fails. A throttled or chrome-polluted child reports success
while producing quietly garbage numbers — that is the single highest-severity silent-corruption risk
in this campaign, and it must be a gate, not an assumption. Assert and log on the first heartbeat:

1. Real RHI — `NOT GUsingNullRHI` (`RHIGlobals.h:840`); GPU timestamps returning non-zero, or the
   GPU metric is marked unavailable up front rather than mid-run.
2. **No background/idle throttling.** The child window is unattended and usually unfocused. The
   governing cvar is **`t.IdleWhenNotForeground`** (`Core/Private/HAL/ConsoleManager.cpp:4159-4160`),
   read by `FEngineLoop::ShouldUseIdleMode()` (`LaunchEngineLoop.cpp:5263-5271`, applied at `:5755`).
   **Its engine default is 0** (no idle throttling) — verified Phase 0 — so the common case is safe,
   but a consumer project's ini can set it to 1 and every number would then measure the throttle
   rather than the level. **Assert the resolved value is 0 in-process; do not merely pass it on the
   command line.** Record it in `environment.forcedCvars`.
3. Frame-rate uncapped: `t.MaxFPS 0`, `r.VSync 0` — read back in-process, not assumed (a consumer
   ini can re-set them).
3b. **Frame-rate smoothing OFF — the easy one to miss.** `UEngine::bSmoothFrameRate` (`Engine.h:1609`)
   clamps frame times into `SmoothedFrameRateRange` (`:1621`) and **applies in `-game`**, silently
   flooring fast frames and corrupting every statistic downstream. The gate is
   `UnrealEngine.cpp:11875`:
   `FPlatformProperties::AllowsFramerateSmoothing() && bSmoothFrameRate && !bForceDisableFrameRateSmoothing && !IsRunningDedicatedServer()`.
   **Prefer setting `GEngine->bForceDisableFrameRateSmoothing = true`** — it is the runtime lever that
   wins regardless of config — and assert the whole expression evaluates false.
3c. **Window must not be minimized.** A minimized window makes Slate skip rendering entirely;
   unfocused-but-visible is fine. Never minimize the child; assert via `GEngine->GameViewport`.
4. `FApp::UseFixedTimeStep()` is false (addendum §1.1 — the frame-time source silently changes if
   true).
5. Source control provider disabled in the child via **`-SCCProvider=None`** — a real switch,
   parsed at `Developer/SourceControl/Private/SourceControlSettings.cpp:58-60` (verified Phase 0).
   **Not moot:** `GitSourceControl.uplugin` declares its module `UncookedOnly`, not `Editor`, so it
   *does* load in a `-game` run on an editor binary. An SCC init inside a measurement process is
   pure noise and a startup-time hazard.

Record all five into `environment` (SCHEMA.md) so a reader can audit the conditions after the fact.

### 4.2 The run state machine (`Private/CkPerfLab/Runner/CkPerfLab_Runner_StateMachine.{h,cpp}`)

Model as `ck::Technique` named steps (doctrine: replaces phase comments):
`Survey → Plan → WarmSweep? → [per position: Teleport → Settle → per direction: Sample] →
AdaptiveRevisit? → Finalize`.

- **Camera**: a spectator-style camera or direct `APlayerController` view target with
  `SetControlRotation`/`SetActorLocation` teleports — Phase 0 smoke informs which is cleanest in
  `-game` with no game module (this host's GameMode is a bare `GameModeBase`). No actor spawned
  beyond the player the mode already spawns; nothing saved.
- **Settle** (per SCHEMA.md params): discard `warmupFrames`; then require rolling CV of frame time <
  `stabilityCV` for `stabilityFrames`, gated on streaming-quiet per Phase 0 addendum signals; cap by
  `timeoutSec` → proceed with degraded confidence inputs (recorded, not fudged).
- **Sample**: per accepted frame read `UCk_Utils_Stats_UE::Get_ThreadTimings()` into the dwell
  buffer; dwell ends at target sample count; Phase 2 stats reduce it. GPU unavailable → carry the
  reason; **never write 0 into the value path**.
- **Census** (per position, once): actors within radius + a view-frustum flag per direction; object
  path, class name, cost proxies (reuse the 3.2 survey data — do not re-iterate the world per
  position; filter the survey by distance).
- **Adaptive (Standard mode)**: after the sweep, positions whose worst/avg exceed budget or whose
  confidence < Medium get revisited with Deep-style direction count + repeats, results merged (flag
  `revisited=true`). Exhaustive toggle = skip the filter.
- **Progressive write**: after each position, rewrite `session.json` (whole-file, codec) + heartbeat.
  Crash-safe: a killed child leaves a readable partial session with `state` telling the host how far
  it got.
- **`budgetMs` is carried, never baked**: the runner records the request's `budgetMs` into the
  session but performs no budget comparison itself — all budget evaluation is Phase 6, host-side,
  and takes an override. This is what lets one captured session be re-analysed at any budget
  (VALIDATION.md rows 6/6a), which is how the evidence gate gets exercised on real data without
  authoring expensive content.

### 4.3 Specs + harness evidence

Pure parts spec-tested as usual: state-machine transition table (feed synthetic tick/timing inputs —
design the machine to take an injected clock+timings interface precisely so specs can drive it),
settle acceptance/degradation branches, adaptive-revisit selection, census distance/frustum filter.

Process-level evidence (agents cannot render): a **repro command** in this file's exit report —
the Phase 0 smoke command plus `-CkPerfLab-Request=<scratch request>` against the chosen test map,
run via Bash with timeout, then `Ck.PerfLab.Codec` read of the produced session in a follow-up spec
run (`Ck.PerfLab.Runner.SessionArtifact` — a spec that loads a checked-in *fixture* session produced
this phase and asserts invariants; the live run itself is the executor's captured-output evidence,
pasted into PROGRESS.md).

## Expected observations — branches

| I will run | I expect | If instead | Response |
|---|---|---|---|
| Live child run against test map | Exit 0 ≤ budget; `session.json` decodes; positions == plan; heartbeat reached `Done` | Hangs > budget | Your own watchdog failed first — fix that before anything else; kill, read the log tail, classify (modal dialog? AS compile gate? map load?) |
| Same, `-nullrhi` | Completes; GT numbers present; RT/GPU marked unavailable per Phase 1 semantics | RT reads plausible values under nullrhi | Phase 0 claim was wrong — correct the availability detection, date-note the addendum |
| Run twice, same request | Position ids identical; timings differ within noise | Position sets differ | Determinism leak in survey (actor iteration order?) — sort before planning; this breaks the compare contract, do not proceed to Phase 5 with it red |
| Runner specs | Green | Transition-table spec hard to write | The machine is insufficiently injected — refactor toward the injected clock/timings seam; do NOT drop the spec |

## Exit criteria — same commit

- [ ] All `Ck.PerfLab` specs green vs baseline; live-run evidence (both command lines + log verdicts
      + a session excerpt) pasted into PROGRESS.md dated entry.
- [ ] A fixture session (small, anonymised map ok) checked into
      `Source/CkPerfLab/Fixtures/` for downstream phases' specs.
- [ ] `CkPerfLab/CLAUDE.md` §Runner: the state machine, the injected-clock seam, the crash-safety
      contract, the forced-cvar honesty rule.
- [ ] PLAN.md row + Status header + PROGRESS.md entry.

## Fences

- Nothing is saved/modified in the map or any asset by the child — `git status` after a run shows
  only `Saved/` artifacts (verify and record).
- No editor-module includes (`UnrealEd` etc.) — this module runs in `-game`.
- Do not "stabilise" flaky timings with sleeps — settle criteria + outlier machinery are the design;
  a sleep is the banned quick-fix (`meta-root-cause-discipline`).
- Two failed attempts at any one mechanism → stuck protocol (`ck-methodology` §5), no silent third.
