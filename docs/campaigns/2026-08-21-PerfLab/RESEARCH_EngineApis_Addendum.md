# PerfLab — Phase 0 engine-API verification addendum

> **Written:** 2026-08-21 (Phase 0, executor session 1). **Supersedes**
> [RESEARCH_EngineApis.md](RESEARCH_EngineApis.md) wherever the two disagree — that doc was written
> from agent/forum research; this one is read from the engine source in this checkout.
> **Engine:** `D:\Repositories\UnrealEngine-Angelscript` (resolved via
> `CkAuto/Get-ProjectEnginePath.ps1`; re-resolve, never hardcode).
> **This doc dies when:** Phase 1 lands the reader and `CkProfile/Claude.md` carries the contract.

Legend: **CONFIRMED** = read in engine source this session, file:line quoted.
**CORRECTED** = the prior research doc was wrong; the truth is recorded here.
**OPEN** = still unverified; the phase that needs it must verify before use.

---

## 1. `stat unit` — the exact source expressions (Phase 1's implementation spec)

**CONFIRMED.** All four numbers are produced in `FStatUnitData::DrawStat`,
`Engine/Source/Runtime/Engine/Private/UnrealClient.cpp:350`. The prior doc guessed
`UnrealEngine.cpp` — **CORRECTED: it is `UnrealClient.cpp`.**

```cpp
// UnrealClient.cpp:352-368 — frame time source
float DiffTime;
if (FApp::IsBenchmarking() || FApp::UseFixedTimeStep())
{
    const double CurrentTime = FPlatformTime::Seconds();
    if (LastTime == 0) { LastTime = CurrentTime; }
    DiffTime = CurrentTime - LastTime;
    LastTime = CurrentTime;
}
else
{
    // "Use the DiffTime we computed last frame, because it correctly handles the end of frame
    //  idling and corresponds better to the other unit times."
    DiffTime = FApp::GetCurrentTime() - FApp::GetLastTime();
}

RawFrameTime          = DiffTime * 1000.0f;                                          // :370
RawGameThreadTime     = FPlatformTime::ToMilliseconds(GGameThreadTime);              // :374
RawRenderThreadTime   = FPlatformTime::ToMilliseconds(GRenderThreadTime);            // :381
RawRHITTime           = FPlatformTime::ToMilliseconds(GRHIThreadTime);               // :387
RawInputLatencyTime   = FPlatformTime::ToMilliseconds64(GInputLatencyTime);          // :390
for (uint32 GPUIndex : FRHIGPUMask::All())                                           // :396
{
    RawGPUFrameTime[GPUIndex] = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(GPUIndex)); // :399
}
```

### 1.1 Three findings that change Phase 1

1. **Read RAW, never smoothed.** Every metric is stored twice: `RawX`, and a smoothed
   `X = 0.9 * X + 0.1 * RawX` exponential moving average (`:371, :375, :382, :388, :391, :400`).
   `stat unit` displays the smoothed value unless `bShowRawUnitTimes`. **PerfLab must consume the
   RAW expressions** — an EMA would destroy the percentile, 1%-low, and outlier statistics Phase 2
   computes. Record this in `CkProfile/Claude.md`: our numbers equal `stat unit` in *source*, and
   equal `stat unit -raw` in *value*.
2. **The engine's own GPU-availability test is `> 0`** — `UnrealClient.cpp:514`:
   ```cpp
   bHaveGPUData[GPUIndex] = RawGPUFrameTime[GPUIndex] > 0;
   ```
   This independently validates locked decision **D5**: zero GPU cycles means *no data*, not "zero
   cost". Phase 1's `Unavailable_NoGpuTimestamps` branch is exactly the engine's own rule. Same
   pattern for input latency (`bHaveInputLatencyData = InputLatencyTime > 0`, `:517`).
3. **Frame time is last frame's delta, deliberately.** The engine comment is explicit that
   `FApp::GetCurrentTime() - FApp::GetLastTime()` is used because it "correctly handles the end of
   frame idling and corresponds better to the other unit times". Phase 1 mirrors this, NOT
   `FApp::GetDeltaTime()`. The benchmarking/fixed-timestep branch is irrelevant to us (PROMPT.md
   rejects fixed timestep) but Phase 4 must ensure `FApp::UseFixedTimeStep()` is false in the child,
   or the frame-time source silently changes.

## 2. Timing globals

**CONFIRMED** (`Engine/Source/Runtime/RenderCore/Public/RenderTimer.h`):

| Symbol | Line | Type |
|---|---|---|
| `GRenderThreadTime` | 108 | `extern RENDERCORE_API uint32` |
| `GRenderThreadWaitTime` | 111 | " |
| `GRHIThreadTime` | 114 | " |
| `GGameThreadTime` | 117 | " |
| `GGameThreadWaitTime` | 120 | " |
| `GSwapBufferTime` | 123 | " |
| `GGameThreadTimeCriticalPath` | 126 | " |
| `GRenderThreadTimeCriticalPath` | 129 | " |
| `GInputLatencyTimer` | 105 | `extern RENDERCORE_API FInputLatencyTimer` |

All are **cycle counts** → `FPlatformTime::ToMilliseconds()`. Module dep required: `RenderCore`.

## 3. GPU timing

- **CONFIRMED:** `RHI_API uint32 RHIGetGPUFrameCycles(uint32 GPUIndex = 0);` —
  `Engine/Source/Runtime/RHI/Public/DynamicRHI.h:1301`; implementation
  `Engine/Source/Runtime/RHI/Private/GPUProfiler.cpp:2722`. Module dep: `RHI`.
- **CONFIRMED fence:** the old global and member accessor are deprecated in this checkout —
  `RHIGlobals.h`: `UE_DEPRECATED(5.6, "Direct use of GGPUFrameTime is deprecated. Call the global
  scope RHIGetGPUFrameCycles() function instead.")`; `RHICommandList.h`:
  `UE_DEPRECATED(5.6, "GetGPUFrameCycles is deprecated...")`. **Never reference `GGPUFrameTime`.**
- **CONFIRMED:** null-RHI detection is `GUsingNullRHI`, a macro alias —
  `RHIGlobals.h:840`: `#define GUsingNullRHI GRHIGlobals.UsingNullRHI`.
- Multi-GPU: the engine iterates `FRHIGPUMask::All()`. **Phase 1 decision:** read index 0 only
  (`RHIGetGPUFrameCycles()` default arg) — this is a level-perf tool on a workstation, and a
  multi-GPU split would need a whole reporting axis. Recorded as a v1 simplification; if an mGPU
  machine ever matters, the struct grows an array, not a new reader.

## 4. Streaming quiescence — **CORRECTED**

The prior doc listed `IsAsyncLoading()` as a signal. **That free function does not exist under that
name in this checkout** (searched `Engine/Source/Runtime/CoreUObject/` — no `bool IsAsyncLoading()`
declaration). Confirmed alternatives:

| Signal | Location | Meaning |
|---|---|---|
| `GetNumAsyncPackages()` | `CoreUObject/Public/UObject/UObjectGlobals.h:1119` (`COREUOBJECT_API int32`) | `== 0` ⇒ no packages in flight |
| `IAsyncPackageLoader::IsAsyncLoadingPackages()` | `CoreUObject/Public/Serialization/AsyncPackageLoader.h:226` (virtual) | interface-level equivalent |
| `IStreamingManager::GetNumWantingResources()` | `Engine/Public/ContentStreaming.h:316` | texture/resource streaming backlog |
| `IStreamingManager::GetNumWantingResourcesID()` | `ContentStreaming.h:327` | change-detection id for the above |
| `IStreamingManager::StreamAllResources(float TimeLimit)` | `ContentStreaming.h:204` | blocking nudge to force residency |

**Phase 4 settle rule (locked here):** quiescent ⇔ `GetNumAsyncPackages() == 0` **and**
`GetNumWantingResources() == 0` **and** no level-streaming transitions pending, held for N
consecutive frames; else timeout → proceed with degraded confidence and the reason recorded. No
single "everything resident" signal exists — this composite IS the answer, and it is what the
confidence rating exists to qualify.

## 5. Navigation — CONFIRMED

`Engine/Source/Runtime/NavigationSystem/Public/NavigationSystem.h`:

- `UNavigationSystemV1::GetRandomReachablePointInRadius(const FVector& Origin, float Radius,
  FNavLocation& ResultLocation, ANavigationData* NavData = NULL,
  FSharedConstNavQueryFilter QueryFilter = NULL) const` — **:677** (native; prefer over the K2 forms)
- K2 forms for reference: `K2_ProjectPointToNavigation` **:492**,
  `K2_GetRandomReachablePointInRadius` **:497**, `K2_GetRandomLocationInNavigableRadius` **:502**.
- Module dep: `NavigationSystem`.
- **OPEN (Phase 3):** the cheap "this map has no navmesh at all" test. Candidate:
  `UNavigationSystemV1::GetMainNavData()` returning null / `GetNavigationSystem()` null. Phase 3
  verifies before the fallback branch is written; absence must be a legal quiet branch, not an
  ensure.

## 6. Child-instance lifecycle — CONFIRMED

- `FCoreUObjectDelegates::PostLoadMapWithWorld` — `CoreUObject/Public/UObject/UObjectGlobals.h:3402`
  (`static COREUOBJECT_API FPostLoadMapDelegate`). This is Phase 4's arming point.
- `FMonitoredProcess` — `Core/Public/Misc/MonitoredProcess.h`, ctors at `:48` and `:59`
  (`InURL, InParams, [InWorkingDir,] InHidden, InCreatePipes = true`), virtual dtor `:62`.

### Phase 5 launcher decision — **FMonitoredProcess**, not raw `CreateProc`

Rationale, recorded so Phase 5 implements rather than re-decides: `FMonitoredProcess` already gives
output pipes, a cancel path, a completion delegate and a monitored thread — every piece the phase
would otherwise hand-roll around `FPlatformProcess::CreateProc`. GitLink's `GitLink_Subprocess`
remains the *house-style* reference for shaping the wrapper's API (naming, ensure discipline,
teardown ordering), but not for the mechanism: GitLink shells short-lived `git` invocations and
needs synchronous stdout capture, whereas PerfLab supervises a multi-minute child it must poll and
be able to kill. Phase 5 wraps `FMonitoredProcess` in a `ck`-flavoured type and still files
adjudication **A-PerfLab-1** for where that type ultimately lives.

## 7. Still OPEN — must be verified by the phase that consumes it

| Item | Needed by | Note |
|---|---|---|
| No-navmesh cheap test | Phase 3 | §5 above |
| What `-game` spawns with this host's bare `GameModeBase`; spectator/teleport path | Phase 4 | Phase 0 §0.2 smoke answers this — see PROGRESS.md for the captured run |
| Whether `FApp::UseFixedTimeStep()` is false by default in `-game` | Phase 4 | §1.1 finding 3 makes this load-bearing |
| Shader-compile / DDC activity readable from game code | Phase 4 | for the confidence rating's pollution component |
| Subprocess launch from a commandlet context | Phase 9 | the documented split-branch trigger |
