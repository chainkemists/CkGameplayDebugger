# Phase 4 — 100k scale hardening, measurement, polish, docs

> **Status:** ✅ Done 2026-08-15 (committed locally; ship withheld pending user; `[EDITOR-VERIFY]` pending)
> **Depends on:** Phase 3 ✅
> **Estimate:** 1 session (autonomous — user AFK; commits per phase authorized, ship withheld)

## Goal

After this phase: the facility's per-frame cost at 100,000 mostly-static bodies is MEASURED
(headless benchmark spec, numbers recorded), the known scale traps are closed or measured-and-
accepted, the deferred review findings are resolved, per-user debugger preferences persist, and
the module docs are the permanent record. Campaign is ship-ready pending user PIE pass + push.

## Design rulings (orchestrator, binding)

| ID | Ruling |
|---|---|
| P4-D30 | **Measure first, in a headless benchmark spec** modeled on `Ck.Jolt.Body.Benchmark.FrameCostMatrix` (CkTests precedent — read it): scoped Game world + Jolt world, N bodies (N ∈ {1k, 10k, 100k}; 100k static boxes sharing one shape + 1k awake dynamics), a bound target, time (a) the first full pass, (b) steady-state capture with 1k active, (c) a scene-revision re-run (full pass with persistent slots), (d) `TryPick_Body`, (e) `Set_HighlightedBody` (re-armed full pass). Report ms per case in the log at Display and assert only sanity bounds generous enough to never flake (e.g. steady-state 100k < 50 ms in a headless Debug/Development editor); the NUMBERS go into PROGRESS + module docs. |
| P4-D31 | **Phase-1 #4 (full pass per-instance `AddInstanceById` + per-body alloc + WP-streaming re-runs)** → implement the incremental static diff: on a scene-revision bump, walk all bodies but touch only keys not already slotted (add) and slotted keys no longer present (release) — NOT release-and-rebuild; batch adds per bucket via `AddInstances(TArray<FTransform>, …)` returning ids where the engine API allows, else keep per-instance but skip already-slotted. Measure before/after with the P4-D30 spec; record both numbers. |
| P4-D32 | **Phase-1 #5 (dead-sleeping sweep O(sleeping)/frame)** → cadence: run the sweep every N captures (N=8 default, constant) OR drive it from a body-removed hook if CkJolt already exposes one (JoltBody EndPlay funnel already bumps revision for statics — extend: any body EndPlay bumps a `_BodyRemovedRevision`, sweep only when it changed). Prefer the revision-driven form; fall back to cadence if the funnel set is ambiguous (report). |
| P4-D33 | **Phase-3 deferred:** #8 (filtered-out selected row → keep the selected row pinned in `_ItemSource` regardless of filter, marked dimmed); #10 (`Release_SlotsForKey` stat pollution → count-into-stats flag); #14 (specs: destroying a highlighted body releases both slots; character overlay via `Make_CharacterBodyKey`); #15 (detail panel: `Get_Selection` returns `const TOptional<...>&`); #17 (stable sort: population then display name; order-change detection); #18 (`F` requires no modifier; pick on `IE_Released` with press-position drag threshold). |
| P4-D34 | **Per-user preferences persist** via a `UCkJoltDebuggerSettings : UDeveloperSettings` (`Config=GameUserSettings`, `GetContainerName()="Editor"`, precedent `CkCrowdDebuggerSettings.h`): render mode, population visibility, camera preset (perspective/ortho), splitter ratios optional. Loaded at window construct, saved on change. |
| P4-D35 | **`[PACKAGED-VERIFY]` stays open** — cannot be closed headlessly; documented in both CLAUDE.md files with the exact acceptance step (Development packaged build, open Jolt debugger, both materials render) and the branch-(b) fallback. |
| P4-D36 | **Ship prep only, no push:** final adversarial review, final full serial gate, module CLAUDE.md finalized, campaign PROGRESS "Ship" row = ready; superproject pointer bumps + pushes WAIT for the user (cross-repo publish guard: never publish a tip referencing unpushed SHAs). |

## Work items
1. Benchmark spec (P4-D30) — BEFORE any perf change; record baseline numbers.
2. Incremental static diff (P4-D31) + sweep cadence/revision (P4-D32); re-run benchmark; record.
3. Deferred fixes (P4-D33) + specs.
4. Settings persistence (P4-D34) + spec that settings round-trip (if the suite has a settings-spec precedent; else construct-only).
5. Docs: both CLAUDE.md files (numbers, worklist closed/accepted lines, packaged-verify step); PLAN/PROGRESS.
6. Adversarial review (Opus, drafts triage) → fix-up → orchestrator gate of record (full serial + scoped serial) → commit.

## Fences
- No user-facing behavior change beyond P4-D33/D34; no new populations; no PhysicsSystem reads from the debugger.
- Benchmarks assert sanity bounds only — never tight numbers (flake ban).
- Never regress the 12+ facility specs or the 8 debugger specs; census untouched.

## `[EDITOR-VERIFY]` (for the user, later)
1. Preferences persist across editor restart (render mode, populations, camera).
2. Selecting a row filtered out by search keeps it visible (dimmed) in the outliner.
3. `Ctrl+F`/`Alt+F` do NOT frame; drag-then-release does NOT pick; click picks.
4. Baked static click on a NON-first body of an actor selects the actor row.
5. `[PACKAGED-VERIFY]` (P4-D35).

## Exit criteria — same commit as last work item
- [x] Benchmark numbers recorded (before/after) in PROGRESS + `CkJolt/CLAUDE.md` (100k: revision re-run 260.5→22.9 ms, selection pass 249.9→23.6 ms, steady 2.1→2.6 ms; reproduced under the serial gate 23.4 / 25.5 / 2.7 ms)
- [x] Full serial suite 1146/1150, fails ⊆ baseline (Homing_ClearTarget red re-ran green in isolation — known-flaky family); scoped serial 78/78; census 3/3; Probe 27/27
- [x] Docs finalized; PLAN/PROGRESS updated; commits landed (local: CkFoundation 2d0f71ced, CkTests 59e3d1d6, CkGameplayDebugger 66f1e75); ship instructions in PROGRESS "Ship" section
