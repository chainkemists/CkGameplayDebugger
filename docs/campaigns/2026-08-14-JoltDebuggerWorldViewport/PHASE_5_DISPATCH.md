# Phases 5–8 — dispatch plan (orchestrator working doc)

> Volatile; superseded line-by-line as units complete. Units are dispatched by the orchestrator
> (Fable) to Opus executors per P0-D6b. Executors follow the PHASE_N.md contract + this file; they
> **STOP** on any unenumerated observation, any design fork, or two failed attempts on one step.
> Unit numbering continues from Phase 3 (which ended at Unit X).

## Scheduling rule — read before parallelising anything

**Builds are serialised machine-wide.** The Unreal Toolbox holds a machine-wide build lock and the
editor cannot be running during a build (`CkAuto/Check-UnrealNotRunning.ps1` hard-blocks
`Build.bat` while the editor holds this project's log). Therefore:

- **Implementation units run STRICTLY SERIALLY**, even across the two repos — CkFoundation and
  CkGameplayDebugger compile into the same editor target, so "different repo" buys nothing.
- **Read-only units may run in parallel** with a build or with each other: research fan-outs,
  adversarial reviews, docs drafting, triage drafting.
- **Never run a scoped gate with `--parallel > 1` when the verdict matters** — the SQLite
  `Saved/Search/FileInfo.db` lane-contention signature has produced a different false red on
  essentially every multi-lane run of this campaign. The orchestrator's serial run is the arbiter.
- **Never edit source (or `Script/`) while a build or a test run is in flight** — it poisons the run
  and produces a stale-green.

## Phase 5 — CkJolt draw channels, flags, colour modes

### Unit XI — channels + flags + in-world re-host (Opus, fresh) — **READY TO DISPATCH**
**Contract:** PHASE_5.md work items 1–7 (P5-D38, P5-D39) **as amended by P5-D61 (S3, S12)**.
**Files:** `CkJolt/Public/CkJolt/Subsystem/CkJolt_DebugDrawTarget.{h,cpp}`,
`CkJolt_DebugDrawTarget_Impl.h`, `CkJolt_DebugRenderer.{h,cpp}`, `CkJoltDebugDraw_Capture.cpp`,
`CkJolt_Subsystem.cpp` (Tick draw block `:514-608`, `DrawSettings` block `:530-578`);
`CkTests/.../UnitTests/CkJolt/Test_JoltDebugDraw_TargetReconcile.cpp`;
**collateral only** — `CkJoltDebugger/.../SCkJoltDebuggerWindow.cpp` call sites (`:1029-1068`,
`:1099-1138`) if a signature moves.
**Steps:** (1) three channels on the target — line batcher + labels + **retained NAMED External
sub-channels** (`Draw_External*(Name,…)` accumulates, `Clear_External(Name)` empties; JPH lines and
labels clear each capture, External sub-channels are **re-emitted without clearing**); (2) re-point
`DrawLine`/`DrawTriangle`/`DrawText3D` (`Renderer.cpp:259-312`) at the active target's channels;
(3) `FCk_Jolt_DebugDrawFlags` + `Set_/Get_DrawFlags`, **`SleepStats` DROPPED (S4)**; (4) flag-gated
per-body extras in `Draw_Body` (`Capture.cpp:191-207`) using the **`…Unchecked()` accessors** —
`JPH_ENABLE_ASSERTS` is on in every config; (5) `DrawConstraints`/`DrawConstraintLimits`/
`DrawConstraintReferenceFrame` under the constraint flags; (6) **re-host the in-world draw** onto
the same flags (delete the `DrawSettings` block; CVars stay the in-world source of truth; gate,
opacity, `HideAll()` and `NextFrame()` unchanged); (7) docs defect fix — `CkJolt/Claude.md:508-523`
attributes `Benchmark.ScaleMatrix` to the wrong file, and the debugger's cross-refs say
`CkJolt/CLAUDE.md` where the file is `Claude.md`.
**New specs:** `Ck.Jolt.DebugDraw.LineAndLabelChannels` (incl. the S3 retention leg),
`Ck.Jolt.DebugDraw.DrawFlagsGatePerBodyExtras`.
**Prereq reading:** PHASE_5.md in full; `CkJolt/Claude.md` §"Debug draw + stats" (`:274-523`) and
§"Anti-patterns" (`:617-634`); the current `DrawSettings` block (`CkJolt_Subsystem.cpp:530-578`).
**Gate:** `--test-pattern Jolt --parallel 1`, then `--test-pattern JoltDebugger --parallel 1`
(collateral call sites must still compile).
**STOP triggers:** `ULineBatchComponent` unusable in an `EditorPreview` world (→ PDI fallback,
D38's named branch, S11); in-world draw parity cannot be held after the re-host.

### Unit XII — contacts, highlight, colour modes (Opus, may be the same executor resumed)
**Contract:** PHASE_5.md work items 8–14 (P5-D40, P5-D41, P5-D42) **as amended by P5-D61 (S1, S6)**.
**Files:** the Unit-XI set, **plus `CkJolt/Public/CkJolt/World/CkJoltWorld.{h,cpp}`** (S1: the
record scope goes inside `DoPhysicsUpdate`, **not** in the Step processor),
**`Subsystem/CkJoltDebugDraw_Processor.{h,cpp}`** (S1: the double-buffered contact buffer is
consumed on the game thread by the capture processor and replayed into demanding targets),
**`CollisionLayer/CkJoltCollisionLayerTable.{h,cpp}`** (S6: read-only reverse lookup for the
ObjectLayer legend names), `Test_JoltDebugDraw_Benchmark.cpp` (re-run) and the `FScopedJoltWorld`
fixture (needs a new `Step()` helper — the reconcile fixture never steps today).
**Order is load-bearing:** D42's mask widening (uint8 → uint64, `static_assert` 8 → 64) lands
BEFORE D41's Hover class, which would otherwise be the 9th of 8.
**Gate:** `--test-pattern Jolt --parallel 1`; benchmark numbers captured from the log (default
flags AND all-flags).

### Unit XIII — Phase-5 review + fix-up + docs weld (fresh Opus reviewer, then an executor)
Adversarial review (read-only, may run in parallel with the orchestrator's gate) → Opus-drafted
triage → orchestrator ratifies → fix-up unit → gate of record on the FINAL artifact → docs weld
(`CkJolt/Claude.md`, PLAN, PROGRESS) → local commit.

## Phase 6 — CkJolt sim control, inspection, drag

### Unit XIV — pause, step-once, step duration, stats (Opus, fresh)
**Contract:** PHASE_6.md work items 1–5 (P6-D43, P6-D48).
**Files:** `CkJolt/Public/CkJolt/World/CkJoltWorld.{h,cpp}`, `CkJoltWorld_Processor.cpp`,
`CkJolt_Subsystem.{h,cpp}` (incl. `CkContactListener` at `:70` for the pair counter);
`CkTests/.../UnitTests/CkJolt/Test_JoltWorld_FixedTimestep.spec.cpp` (sibling for the new specs).
**Gate:** `--test-pattern Jolt --parallel 1`.
**STOP triggers:** the accumulator bursts on resume; the async branch makes the step-duration write
race-unsafe in a way a relaxed atomic does not cover.

### Unit XV — samples, selection contacts, multi-select, isolate (Opus)
**Contract:** PHASE_6.md work items 6–9 (P6-D44, P6-D45, P6-D46).
**Files:** `CkJolt_DebugDrawTarget.{h,cpp}`, `_Impl.h`, `CkJoltDebugDraw_Capture.cpp`;
`Test_JoltDebugDraw_TargetReconcile.cpp`. Collateral: the two `CkJoltDebugger` call sites of
`Get_HighlightedBodyLinearVelocity`.
**Gate:** `--test-pattern Jolt`, `--test-pattern JoltDebugger`.

### Unit XVI — mouse-drag facility (Opus, fresh — this one is a genuine unknown)
**Contract:** PHASE_6.md work items 10–12 (P6-D47).
**Files:** new drag processor beside `CkJoltDebugDraw_Processor.{h,cpp}`, `CkJolt_Subsystem.{h,cpp}`
(requests + **`Get_DebugInternalBodyIds()`**), `CkJoltCollisionLayerTable.{h,cpp}` (lazy
ignore-everything layer registration **on first drag**), `CkJoltDebugDraw_Capture.cpp` +
`TryPick_Body` (**skip the internal ids**); a new `Test_JoltDebugDrag.cpp` in `UnitTests/CkJolt/`.
**Recipe is RATIFIED (P5-D61/S2)** — default-constructed `FCk_Jolt_CollisionSignature`
(`_ResponseMask = 0`, `_Domain = Dynamic`), raw JPH kinematic anchor with **no** entity.
**Why fresh:** no vendored Samples reference (verified), so the constraint tuning is designed here;
budget an iteration. **STOP triggers:** the ignore-everything layer does not behave as reasoned;
the anchor body leaks across world teardown; the anchor becomes drawable/pickable despite the
internal-id skip.

### Unit XVII — Phase-6 review + fix-up + docs weld + FULL SERIAL GATE
Same shape as XIII, but the **full no-pattern serial suite** runs here (end of the CkFoundation
half) in addition to the scoped gates. Local commit.

## Phase 7 — CkJoltDebugger camera, lanes, detail, selection, drag UI

### Unit XVIII — camera rewrite (Opus, fresh)
**Contract:** PHASE_7.md work items 1–6 (P7-D49).
**Files:** `CkJoltDebugger/Public/CkJoltDebugger/Viewport/SCkJoltDebugger_3dViewport.{h,cpp}` ONLY;
`Private/Tests/CkJoltDebuggerViewport.spec.cpp`.
**Explicitly out of scope:** `SCkCrowdDebugger_3dViewport.{h,cpp}` — identical code today, and it
stays that way until the Crowd campaign.
**Gate:** `--test-pattern JoltDebugger --parallel 1`.

### Unit XIX — Draw lane, sim controls, stats rail, settings (Opus)
**Contract:** PHASE_7.md work items 7–12 (P7-D50, P7-D51).
**Files:** `Settings/CkJoltDebuggerSettings.h`, `Window/SCkJoltDebuggerWindow.{h,cpp}`;
`Private/Tests/CkJoltDebuggerSettings.spec.cpp`.
**Prereq reading:** `CkDebuggerCommon/CLAUDE.md` §"Common window chrome" (`:549-590`) — lanes stay
one physical line, no `SCheckBox`.
**Gate:** `--test-pattern JoltDebugger`, `--test-pattern DebuggerLauncher`.

### Unit XX — detail panel, multi-select, isolate, follow, drag UI (Opus, fresh)
**Contract:** PHASE_7.md work items 13–19 (P7-D52, P7-D53, P7-D54).
**Files:** `Data/CkJoltDebugger_Types.h`, `Window/SCkJoltDebugger_DetailPanel.{h,cpp}`,
`Window/SCkJoltDebugger_OutlinerPanel.{h,cpp}`, `Window/SCkJoltDebuggerWindow.{h,cpp}`,
`Viewport/SCkJoltDebugger_3dViewport.{h,cpp}`; the Detail and Outliner spec files.
**Prereq reading:** `CkDebuggerCommon/CLAUDE.md` §"List / tree rows" (`:152-226`) — the click-trap
and pointer-identity contracts; there is **no** multi-select doctrine, so this unit writes it.
**Gate:** all three scoped patterns.

### Unit XXI — Phase-7 review + fix-up + docs weld
Same shape as XIII. Local commit.

## Phase 8 — constraints, probes, health, labels, grid, close-out

### Unit XXII — constraints + probe results (Opus, fresh)
**Contract:** PHASE_8.md work items 1–6 (P8-D55, P8-D56).
**Files:** `CkJolt/Public/CkJolt/Constraint/CkJoltConstraint_Utils.{h,cpp}` (the ONE narrow
CkFoundation addition — `Get_BodyA`/`Get_BodyB` per P5-D61/S8; executor STOPs if the
doctrine-conformant spot is unclear rather than widening the friend list),
`CkJoltDebugger/Data/*`, `Window/SCkJoltDebuggerWindow.cpp`.
**Gate:** `--test-pattern Jolt`, `JoltDebugger`, `Probe`.

### Unit XXIII — health checks, labels, hover (Opus)
**Contract:** PHASE_8.md work items 7–10 (P8-D57, P8-D58).
**Files:** `CkJolt_DebugDrawTarget.{h,cpp}` + `CkJoltDebugDraw_Capture.cpp` (problem-body pass),
`CkJoltDebugger/Data/*`, `Window/SCkJoltDebugger_OutlinerPanel.{h,cpp}`,
`Viewport/SCkJoltDebugger_3dViewport.{h,cpp}` (first `OnPaint` in the suite).
**Gate:** `--test-pattern Jolt`, `JoltDebugger`.

### Unit XXIV — grid, gizmo, bookmarks (Sonnet-eligible — now fully mechanical)
**Contract:** PHASE_8.md work items 11–13 (P8-D59) **as amended by P5-D61 (S3, S7)**: the grid is a
one-time push into a retained named External sub-channel, and the gizmo **reuses
`SCkDebug_OrientationCube`** in an overlay slot — **no hand-drawn `OnPaint` gizmo**, so this unit no
longer depends on Unit XXIII's `OnPaint` work.
**Gate:** `--test-pattern JoltDebugger`.

### Unit XXV — campaign close-out (Opus reviewer + orchestrator)
**Contract:** PHASE_8.md work items 14–17 (P8-D60). Final adversarial review → fix-up → **full
serial gate + all four scoped gates on the final artifact** → both CLAUDE.md files → PLAN/PROGRESS
→ local commits → **ship withheld**, with the whole Phase 3–8 `[EDITOR-VERIFY]` backlog collected
into one list for the user.

## Sequencing summary

```
XI → XII → XIII(review‖gate) → XIV → XV → XVI → XVII(review‖gate + FULL)
   → XVIII → XIX → XX → XXI(review‖gate) → XXII → XXIII → XXIV → XXV(FULL, close)
```
`‖` = the read-only review agent may run concurrently with the orchestrator's gate; everything else
is serial because it builds.
