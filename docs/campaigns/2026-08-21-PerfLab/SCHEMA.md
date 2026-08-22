# PerfLab — file contracts (SCHEMA.md)

> **Written:** 2026-08-21 (Phase 0). **Locked** — Phases 2, 4, 5, 6, 9 implement against this and do
> not re-derive it. A change here is a dated edit plus a `schemaVersion` bump plus a PROGRESS.md
> decision-log row.
> **This doc dies when:** the campaign ships; the schema's permanent home is
> `Source/CkPerfLab/CLAUDE.md` + the codec's own specs.

## Conventions (all three files)

- **camelCase** keys, fixed field order as written here, UTF-8, LF, 2-space indent.
- **Milliseconds** for every duration, suffix `Ms`. Distances in **centimetres** (UE units).
- **Timestamps** ISO-8601 UTC (`2026-08-21T14:03:11Z`), always **handed in** by the caller —
  never read from the clock inside a builder (determinism rule).
- `schemaVersion` is the **first** key of every file. Decode is **whole-or-nothing**: an unknown
  version is a typed failure, never a partial object (SnapshotCodec discipline).
- **No metric is ever written as `0` to mean "not measured".** Every optional metric is an object
  `{ "availability": "...", "reason": "...", "valueMs": <number> }` where `valueMs` is meaningful
  **only** when `availability == "available"`. This is locked decision D5, and it matches the
  engine's own `bHaveGPUData = RawGPUFrameTime > 0` test
  (`RESEARCH_EngineApis_Addendum.md` §1.1).
- Arrays are **sorted deterministically** with an explicit final tie-break (named per array below).

Session root on disk: `Saved/CkPerfLab/Sessions/<sessionId>/` containing `request.json`,
`heartbeat.json`, `session.json`.

`sessionId` format: `<UTC yyyyMMdd-HHmmss>-<8-hex of a hash of (map, mode, hostPid)>` — sortable,
collision-resistant, and stable enough to name a directory.

---

## 1. `request.json` — host → child

Written by the host before launch; read once by the child at `Initialize`. Immutable thereafter.

```jsonc
{
  "schemaVersion": 1,
  "sessionId": "20260821-140311-9f3ac1b0",
  "mapPath": "/Game/Maps/TestLevel.TestLevel",   // full object path; child verifies the loaded map matches
  "budgetMs": 16.67,                              // user's target frame budget; the score's denominator
  "seed": 1337,                                   // every random draw derives from this — reruns must match
  "requestingHostPid": 24188,                     // child may exit if the host died (orphan guard)
  "createdUtc": "2026-08-21T14:03:11Z",

  "mode": "standard",                             // "quick" | "standard" | "deep" — label only; params below rule
  "modeParams": {
    "positionBudget": 25,                         // max measurement positions the planner may emit
    "directionsPerPosition": 4,                   // yaw samples per position (quick 1 / standard 4 / deep 16)
    "repeats": 1,                                 // full re-measures per direction (deep uses >1)
    "minPositionSpacingCm": 800.0,                // planner rejects positions closer than this
    "eyeHeightCm": 160.0,                         // camera offset above the sampled ground point
    "censusRadiusCm": 3000.0,                     // actor-census capture radius around a position
    "warmSweep": true,                            // throwaway visit-all pass first (shader/PSO/DDC warm)
    "adaptiveRevisit": true                       // standard mode's survey-then-deepen behaviour
  },

  "settle": {
    "warmupFrames": 30,                           // discarded after every teleport, unconditionally
    "stabilityCv": 0.08,                          // rolling coefficient of variation gate on frame time
    "stabilityFrames": 20,                        // consecutive frames that must satisfy stabilityCv
    "streamingQuietFrames": 10,                   // consecutive quiescent frames required (addendum §4)
    "settleTimeoutSec": 8.0,                      // cap; on expiry sample anyway with degraded confidence
    "targetSampleCount": 120                      // accepted frames per direction
  },

  "outlier": {
    "madK": 3.5                                   // median + k*MAD classification threshold
  },

  "watchdog": {
    "childWallClockBudgetSec": 1800               // child self-aborts past this, flushing a partial session
  }
}
```

## 2. `heartbeat.json` — child → host (progress)

Rewritten by the child at every state change and at least every few seconds. Small on purpose: the
host polls this file, never `session.json`.

```jsonc
{
  "schemaVersion": 1,
  "sessionId": "20260821-140311-9f3ac1b0",
  "state": "measuring",     // booting | planning | warmSweep | measuring | writing | done
                            // | failed_BadRequest | failed_MapMismatch | failed_Timeout | failed_Internal
  "positionsTotal": 25,
  "positionsDone": 11,
  "currentPositionId": "p_0011",
  "message": "",            // human-readable detail; REQUIRED non-empty on any failed_* state
  "lastUpdateUtc": "2026-08-21T14:07:44Z"
}
```

Host-side timeout taxonomy (Phase 5) is distinct from the child's own: `aborted_HostTimeout` and
`aborted_UserCancel` are written by the **host** into `session.json`'s `state` when it kills the
child; `failed_Timeout` is the child self-aborting. The distinction is diagnostic — keep both.

## 3. `session.json` — the artifact

Written progressively by the child (whole-file rewrite after each position, so a killed run still
decodes), then **augmented host-side** with the `analysis` block by Phase 6. The child always writes
`analysis: null`.

```jsonc
{
  "schemaVersion": 1,
  "sessionId": "20260821-140311-9f3ac1b0",
  "state": "done",
  "request": { /* verbatim echo of request.json, minus its own schemaVersion */ },

  "environment": {
    "buildConfiguration": "Development",   // UCk_Utils_Stats_UE::Get_BuildConfig
    "instanceMode": "game",                // "game" | "editor" — comparability axis, never mixed silently
    "rhiName": "D3D12",
    "gpuBrand": "NVIDIA GeForce RTX 4080", // Get_GPUBrand
    "cpuBrand": "AMD Ryzen 9 5950X",       // Get_CPUBrand
    "cpuCoreCount": 16,
    "machineName": "DESKTOP-ADAM",
    "engineVersion": "5.6.0-angelscript",
    "perfLabVersion": "0.1.0",
    "statsEnabled": true,                  // STATS compiled in? affects nothing we read, but pin it
    "childBinary": "CkPluginsEditor-Cmd.exe",  // which executable ran; Unique-build-env projects use their OWN
    "viewportSizeActual": { "width": 1280, "height": 720 },  // ACTUAL backbuffer, not the requested -resx/-resy
    "sourceControlDisabled": true,         // child must not init an SCC provider (this project ships GitSourceControl)
    "shaderCompileActivity": {             // first-run-validity signal; feeds the confidence rating
      "jobsDuringWarmSweep": 412,
      "jobsDuringMeasurement": 0           // non-zero here degrades confidence — measured through a compile storm
    },
    "forcedCvars": {                       // honesty: the run states its own conditions
      "r.VSync": "0",
      "t.MaxFPS": "0",
      "bSmoothFrameRate": "false"
    },
    "startedUtc": "2026-08-21T14:03:15Z",
    "finishedUtc": "2026-08-21T14:21:02Z"
  },

  // Sorted by positionId ascending. positionId is stable across runs of the same map (Phase 3
  // quantised-location hash) — this IS the session-compare matching key.
  "positions": [
    {
      "positionId": "p_0011",
      "location": { "x": 1250.0, "y": -880.0, "z": 312.5 },
      "eyeHeightCm": 160.0,
      "revisited": false,                  // true if adaptive re-measure deepened this position

      // Sorted by yawDegrees ascending.
      "directions": [
        {
          "yawDegrees": 0.0,
          "pitchDegrees": 0.0,
          "sampleCount": 120,
          "outlierCount": 3,
          "metrics": {
            // avgMs excludes outliers; worstMs and onePercentLowMs retain them (locked, LPD parity)
            "frame":        { "availability": "available", "reason": "", "avgMs": 14.2, "worstMs": 31.7, "p99Ms": 28.4, "onePercentLowMs": 27.9 },
            "gameThread":   { "availability": "available", "reason": "", "avgMs": 9.1,  "worstMs": 22.0, "p99Ms": 19.8, "onePercentLowMs": 19.1 },
            "renderThread": { "availability": "available", "reason": "", "avgMs": 12.8, "worstMs": 27.3, "p99Ms": 25.1, "onePercentLowMs": 24.6 },
            "rhiThread":    { "availability": "available", "reason": "", "avgMs": 2.2,  "worstMs": 6.1,  "p99Ms": 5.4,  "onePercentLowMs": 5.2 },
            "gpu":          { "availability": "unavailable_NoGpuTimestamps", "reason": "RHIGetGPUFrameCycles() returned 0 cycles for every accepted sample", "avgMs": 0.0, "worstMs": 0.0, "p99Ms": 0.0, "onePercentLowMs": 0.0 }
          }
        }
      ],

      // Per-position rollup across its directions: worst direction wins for worst*, mean for avg*.
      "aggregate": {
        "frame":        { "availability": "available", "reason": "", "avgMs": 14.9, "worstMs": 33.1, "p99Ms": 29.0, "onePercentLowMs": 28.2 },
        "gameThread":   { /* same shape */ },
        "renderThread": { /* same shape */ },
        "rhiThread":    { /* same shape */ },
        "gpu":          { /* same shape */ }
      },

      "confidence": {
        "rating": "medium",                // high | medium | low | unusable
        "reasons": [                       // empty when rating == "high"; sorted ascending
          "settleTimedOut",                // stability gate never satisfied within settleTimeoutSec
          "streamingActive"                // quiescence not reached before sampling began
          // other legal values: "lowSampleCount", "highVariance", "outlierHeavy", "shaderCompileActivity"
        ]
      },

      // Actors near/in view of this position. Sorted by distanceCm ascending, tie-break objectPath.
      // Object PATHS only — no handles, no pointers (no-live-handle invariant crosses no boundary).
      "actorCensus": [
        {
          "objectPath": "/Game/Maps/TestLevel.TestLevel:PersistentLevel.SM_Rock_42",
          "className": "StaticMeshActor",
          "distanceCm": 640.2,
          "inViewYaws": [0.0, 90.0],       // which directions had it in frustum; [] = near but never in view
          "costProxies": {                 // every field optional; omit rather than write a fake 0
            "triangleCount": 184320,
            "materialSlotCount": 3,
            "lightCount": 0,
            "niagaraComponentCount": 0,
            "tickEnabled": false,
            "castsDynamicShadow": true,
            "hasCollision": true
          }
        }
      ]
    }
  ],

  // null until Phase 6 analysis runs host-side. Never written by the child.
  "analysis": null
}
```

### 3.1 `analysis` block (written host-side by Phase 6)

```jsonc
"analysis": {
  "analysedUtc": "2026-08-21T14:25:00Z",
  "budgetMs": 16.67,

  "score": {
    "value": 72,                           // 0-100, integer, the headline
    "componentsUsed": 7,                   // of 8; dropped components renormalise the rest
    "components": [                        // sorted by component key; weights sum to 1.0 over USED components
      { "key": "averageFrameAttainment",  "weight": 0.263, "value": 88, "included": true,  "reason": "" },
      { "key": "worstCaseAttainment",     "weight": 0.158, "value": 51, "included": true,  "reason": "" },
      { "key": "onePercentLowAttainment", "weight": 0.158, "value": 58, "included": true,  "reason": "" },
      { "key": "gameThreadHeadroom",      "weight": 0.105, "value": 74, "included": true,  "reason": "" },
      { "key": "renderThreadHeadroom",    "weight": 0.105, "value": 61, "included": true,  "reason": "" },
      { "key": "gpuHeadroom",             "weight": 0.0,   "value": 0,  "included": false, "reason": "gpu unavailable at every position" },
      { "key": "spatialConsistency",      "weight": 0.105, "value": 66, "included": true,  "reason": "" },
      { "key": "measurementConfidence",   "weight": 0.053, "value": 80, "included": true,  "reason": "" }
    ],
    "formula": "weighted harmonic mean of included component scores; per-position component = clamp(100 * budgetMs / actualMs, 0, 100)"
  },

  "measuredClean": false,                  // true ⇔ every position under budget ⇒ zero perf findings

  // Sorted by severity (enum is declared worst-first, so ASCENDING by value — the P18 lesson),
  // then checkId, then positionId.
  "findings": [
    {
      "checkId": "Perf.Gpu.TriangleDensity",
      "stableKey": "Perf.Gpu.TriangleDensity|p_0011|/Game/.../SM_Rock_42",
      "severity": "major",                 // critical | major | minor
      "positionId": "p_0011",
      "evidence": {                        // REQUIRED and non-empty — the evidence gate, spec-pinned
        "metric": "gpu",
        "measuredMs": 24.9,
        "budgetMs": 16.67,
        "overBudgetRatio": 1.49,
        "signal": "inViewTriangleCount",
        "signalValue": 2841100
      },
      "contributors": [                    // sorted by rank ascending; rank is deterministic
        {
          "rank": 1,
          "objectPath": "/Game/Maps/TestLevel.TestLevel:PersistentLevel.SM_Rock_42",
          "className": "StaticMeshActor",
          "distanceCm": 640.2,
          "note": "near the measured cost — worth investigating, not proven cause"
        }
      ],
      "recommendations": [                 // ordered gain-desc then effort-asc
        { "order": 1, "gainBand": "high", "effortBand": "low", "text": "Enable Nanite on SM_Rock_42 or author LODs; 184k tris at 6.4 m is the dominant in-view cost here." }
      ]
    }
  ]
}
```

### 3.2 Enum vocabularies (closed sets — codec rejects anything else)

- `availability`: `available` · `unavailable_NullRhi` · `unavailable_NoGpuTimestamps` ·
  `unavailable_NotYetSampled`
- `confidence.rating`: `high` · `medium` · `low` · `unusable`
- `confidence.reasons[]`: `settleTimedOut` · `streamingActive` · `lowSampleCount` ·
  `highVariance` · `outlierHeavy` · `shaderCompileActivity`
- `severity`: `critical` · `major` · `minor` (declared worst-first — sort ASCENDING by enum value)
- `gainBand` / `effortBand`: `high` · `medium` · `low`
- `state`: see heartbeat §2, plus host-written `aborted_HostTimeout` · `aborted_UserCancel`

### 3.3 Size control

`request.modeParams.retainRawSamples` (default **false**, absent from the example above because the
default path omits it) may be set true to append a `rawSamplesMs: [ ... ]` array per direction.
Off by default: 25 positions × 4 directions × 120 samples is 12 000 floats per metric. Phase 2's
codec must round-trip the field's presence *and* absence.
