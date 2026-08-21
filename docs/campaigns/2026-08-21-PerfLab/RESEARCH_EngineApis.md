# PerfLab — engine API research (UnrealEngine-Angelscript checkout)

> **Written:** 2026-08-21. Engine facts verified against `D:\Repositories\UnrealEngine-Angelscript`
> (resolve fresh via `CkAuto/Get-ProjectEnginePath.ps1` — never hardcode).
> **This doc dies when:** Phase 0 lands its verification addendum; then that addendum supersedes this.

Legend: **[CONFIRMED-planner]** = read in the engine source by the planning session on 2026-08-21.
**[agent]** = reported by the research agent from docs/forums — Phase 0 re-verifies before use.

## 1. Stat-unit timing sources — the recorder's data

**[CONFIRMED-planner]** `Engine/Source/Runtime/RenderCore/Public/RenderTimer.h:105-129`:

```cpp
extern RENDERCORE_API FInputLatencyTimer GInputLatencyTimer;   // :105
extern RENDERCORE_API uint32 GRenderThreadTime;                // :108
extern RENDERCORE_API uint32 GRenderThreadWaitTime;            // :111
extern RENDERCORE_API uint32 GRHIThreadTime;                   // :114
extern RENDERCORE_API uint32 GGameThreadTime;                  // :117
extern RENDERCORE_API uint32 GGameThreadWaitTime;              // :120
extern RENDERCORE_API uint32 GSwapBufferTime;                  // :123
extern RENDERCORE_API uint32 GGameThreadTimeCriticalPath;      // :126
extern RENDERCORE_API uint32 GRenderThreadTimeCriticalPath;    // :129
```

All are **cycle counts** — convert with `FPlatformTime::ToMilliseconds(Cycles)`.

**GPU: [CONFIRMED-planner]** the global function `RHIGetGPUFrameCycles(uint32 GPUIndex = 0)`
(`Engine/Source/Runtime/RHI/Public/DynamicRHI.h`, `RHI_API`). The old global `GGPUFrameTime` and
`FRHICommandListImmediate::GetGPUFrameCycles` are **`UE_DEPRECATED(5.6, ...)`** in this checkout
(`RHIGlobals.h`, `RHICommandList.h`) — **fence: use `RHIGetGPUFrameCycles()`, never the deprecated
global**, or the build breaks on deprecation-as-error.

Frame time: `FApp::GetDeltaTime()` / the existing `UCk_Utils_Stats_UE::Get_FrameTimeMs()`
(`Plugins/CkFoundation/Source/CkProfile/Public/CkProfile/Stats/CkStats_Utils.h`).

Caveats to encode as *unavailability reasons*, not zeros:
- `RHIGetGPUFrameCycles()` returns 0 when the RHI/driver provides no timestamps (nullrhi, some
  drivers, vsync/VR interference) — **0 means Unavailable, never "infinitely fast"** [agent].
- Timestamp accuracy degrades with vsync on → child forces `r.VSync 0`, `t.MaxFPS 0`,
  `GEngine->bSmoothFrameRate=false` [agent].
- Where exactly `stat unit` reads its four numbers: `FStatUnitData::DrawStat` in
  `Engine/Source/Runtime/Engine/Private/UnrealEngine.cpp` [agent] — **Phase 0 reads this function**
  and mirrors its exact source expressions so PerfLab's numbers match `stat unit` by construction.

## 2. Driving the child instance

- Launch shape [agent, matches CkAuto's own Gauntlet internals]:
  `<Engine>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe <Project>.uproject <MapPath> -game -windowed
  -resx=1280 -resy=720 -unattended -nosplash -log [-nullrhi]` — `-game` avoids full-editor overhead
  and most `Saved/` contention. **Rejected alternatives** recorded in PROMPT.md.
- No process-launch precedent exists inside CkFoundation (verified: zero `FPlatformProcess::CreateProc`
  hits). The in-house pattern to copy is
  **`Plugins/GitLink/Source/GitLink/Private/GitLink_Subprocess.h/.cpp`** [agent — executor reads it
  before writing the launcher]. Engine also offers `FMonitoredProcess` (`Core/Public/Misc/MonitoredProcess.h`)
  — Phase 0 decides between raw `CreateProc` (GitLink shape) and `FMonitoredProcess` after reading both.
- Handoff: host writes a **request JSON**; child gets `-CkPerfLab-Request=<abs path>`; child writes
  progress heartbeat + result JSON under `Saved/CkPerfLab/Sessions/<SessionId>/`; host polls files.
  (LPD does the equivalent with `Saved/LevelPerformanceDoctor` [listing].) Remote Control API rejected
  — heavier than file polling buys.
- Exit: child calls `RequestEngineExit` after final write; host enforces its own wall-clock timeout →
  `TerminateProc` + session marked `Aborted_Timeout`.

## 3. Position auto-determination

- **NavMesh-seeded** (strongest precedent — Simplygon samples cameras from navmesh
  [documentation.simplygon.com …/navmeshvisibilitysampling.html]):
  `UNavigationSystemV1::GetRandomPoint` / `ProjectPointToNavigation`, or iterate `FRecastNavMesh`
  tiles. Fails when the map has no navmesh → fallback required. [agent — Phase 0 verifies API shapes]
- **Bounds-grid fallback**: union of actor bounds → uniform grid → line-trace down to find ground →
  reject cells with no actors within radius. Always works.
- **In-house candidate generators already exist**:
  `FCk_Eqs_Algorithm::DoGenerate_{SimpleGrid,Grid,Donut,Cone}` +
  budgeted resumable `DoRunTests` (`Plugins/CkFoundation/Source/CkEqs/Public/CkEqs/Query/CkEqs_Algorithm.h`)
  [agent] — mimic the shapes; whether to depend on CkEqs directly or mirror the grid math locally is a
  Phase 3 entry decision (dep weight vs mimicry).
- World Partition cell centres: deferred (API version-volatile) — non-goal v1.
- Eye height: position + configurable eye offset; multi-yaw directions per position (Quick=1 forward,
  Standard=4, Deep=16 — mode presets, not separate code paths).

## 4. Settle / warmup / stability

- Streaming quiescence: combine `IsAsyncLoading()`, `GetNumAsyncPackages()`, level-streaming states,
  `IStreamingManager::Get().StreamAllResources(...)` nudge + texture streaming in-flight count
  [agent — Phase 0 verifies each symbol]. **No single signal exists**; use "all quiet for N frames OR
  timeout T, timeout degrades confidence".
- Warmup: discard first W frames after teleport; then require rolling frame-time coefficient of
  variation below threshold for S consecutive frames before sampling window opens; else accept with
  degraded confidence after cap.
- Outliers: median + k·MAD classification; excluded from avg, retained in worst-case and 1% low;
  count reported. (Percentile machinery precedent: `FCk_MultiFrameStats` P95/P99 in
  `Plugins/CkFoundation/Source/CkInsightsAnalyzer/Public/CkInsightsAnalyzer/Report/CkMultiFrameReport.h`.)
- First-sweep pollution: optional (default-on) throwaway warm sweep visiting every position before the
  measured pass — shader/PSO/DDC warm; record `psocache`/shader-compile activity if cheaply readable
  [Phase 0 verifies what's readable].

## 5. Comparability rules (from `ck-performance-and-analysis`, binding)

- Session records: build config, editor-vs-game, RHI name, GPU/CPU brand
  (`UCk_Utils_Stats_UE::Get_GPUBrand/Get_CPUBrand`), machine name, engine + plugin versions, map,
  mode, seed, target budget ms.
- A perf claim needs baseline + N runs + numbers-with-units + one-variable — the compare view enforces
  same-map, warns on differing config/machine/RHI, and never silently mixes.
- `STATS` is ON in Debug/Development, OFF in Test/Shipping (`Engine/Source/Runtime/Core/Public/Misc/Build.h`)
  — PerfLab targets Development; the timing globals themselves are not STATS-gated [agent — Phase 0
  confirms the globals update without STATS… they are updated by the renderer, but confirm].
