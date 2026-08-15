# Jolt Debugger — World Viewport — PROGRESS.md (living log)

## Current state  <!-- supersedes everything below; update at EVERY phase boundary and session end -->
**As of 2026-08-15 (campaign close, user AFK):** **ALL FOUR PHASES DONE AND COMMITTED LOCALLY. SHIP
IS READY BUT WITHHELD** (outward action — needs the user). Nothing pushed; no superproject pointer
bumps. `[EDITOR-VERIFY]` for Phases 3–4 (+ `[PACKAGED-VERIFY]`) awaits the user's return.
**Local tips (all `dev`, all ahead of `origin/dev`):**
- CkFoundation: 2d0f71ced (Phase 4) ← 8e85733f1 (P3) ← 5b3ce3eb3 (P2) ← fb90e2f09 (branding) ← 5edf6c152 (P1)
- CkTests: 59e3d1d6 (P4) ← 99e8c9f9 (P3) ← d039513b (P2) ← a6903924 (branding) ← fbd99927 (P1)
- CkGameplayDebugger: [docs commit below] ← 66f1e75 (P4) ← 0271004/f646b4a (P3) ← 00b4801/c1cbd5f (P2) ← 5b954c7 (branding) ← c804e7b (P1 docs)
- Superproject: local `dev` @ fd13be1 (branding) on top of origin/dev f51209d; HEAD detached at fd13be1.
**Final gate of record (orchestrator, serial, final artifact):** full suite 1146/1150 with fails ⊆
baseline named set (one flaky sibling re-ran green in isolation); scoped Jolt+JoltDebugger 78/78 exit 0
(14 facility + 9 debugger specs); DebuggerLauncher census 3/3; Probe 27/27.
**Measured (P4 benchmark, 100k static + 1k awake):** steady 2.6 ms; revision re-run 23 ms (was 260);
selection change 24 ms (was 250); pick 14–16 ms; first pass ~63–70 ms.

## Ship — instructions for the user (NOT executed; cross-repo publish guard)
1. PIE-verify Phase 3 (7 steps, PHASE_3.md) + Phase 4 (5 steps, PHASE_4.md) + the static-probe line
   (destroy a Static probe with the debugger open → its instance disappears). Fix-forward anything red.
2. Publish in dependency order so no tip references an unpushed SHA: push CkFoundation `dev` first,
   then CkTests `dev`, then CkGameplayDebugger `dev` (each is a fast-forward of origin/dev — verify
   with `git status -sb`); THEN bump the three gitlinks in the superproject on a `feature/`-prefixed
   branch (`feature/jolt-debugger-world-viewport`) and push/PR per `ck-ship-dev` / `ck-ship-pr`.
   The superproject also carries the local branding commit fd13be1 on `dev` (attach with
   `git checkout dev` once the editor is closed).
3. `[PACKAGED-VERIFY]`: Development packaged build → open the Jolt debugger → both engine debug
   materials render (solid + wireframe). If not: switch wireframe to the CkUsf-generated look
   (P1-D13 branch b, mechanism in `CkJolt/CLAUDE.md`).
4. Downstream (BusterBlock) pointer bumps after the three pushes.
5. Next campaign (already scoped by the user): refactor the Crowd debugger onto this facility
   (carry over: ortho eye-offset fix `SCkCrowdDebugger_3dViewport.cpp:632-637`, preview-world
   target registration, outliner/selection contracts).

## Status board

| Phase | State |
|---|---|
| Docs/decisions | ✅ Done 2026-08-14 |
| 1 — CkJolt renderer facility | ✅ Done 2026-08-15, COMMITTED (5edf6c152 / fbd99927) |
| 2 — viewport shell | ✅ Done 2026-08-15, COMMITTED (c1cbd5f / 5b3ce3eb3 / d039513b); **`[EDITOR-VERIFY]` A–D 12/12 PASSED by user 2026-08-15** |
| 3 — outliner + selection | ✅ Done 2026-08-15, COMMITTED (8e85733f1 / 99e8c9f9 / f646b4a / 0271004); gate: full 1146/1150 fails ⊆ baseline (VatProxy red re-ran green = Zen DDC infra noise), scoped serial 75/75; `[EDITOR-VERIFY]` (7 steps in PHASE_3.md) pending user |
| 4 — scale + polish | ✅ Done 2026-08-15, COMMITTED (2d0f71ced / 59e3d1d6 / 66f1e75); measured; gate: full 1146/1150 ⊆ baseline, scoped 78/78; `[EDITOR-VERIFY]` (5 steps + static-probe) pending user |
| Ship | 🟢 READY — withheld for user (push/pointer bumps/PACKAGED-VERIFY); instructions in "Ship" section above |

## Decision log
| # | Date | Decision | Why | Revisit when |
|---|---|---|---|---|
| P0-D1 | 2026-08-14 | Preview `FPreviewScene` world + generalized world-targetable batched ISM renderer; NO private EcsWorld/registry | User-ruled; zero-precedent scheduler work avoided; all pieces proven | Crowd-refactor campaign needs entity-based rendering |
| P0-D2 | 2026-08-14 | Wireframe = material swap on same ISMs (solid unlit ⇄ wireframe-flag material) | User-ruled; color survives both modes, no geometry rebuild | never |
| P0-D3 | 2026-08-14 | Draw+list all four populations (JoltBody, baked static world, Probe sensors, Character capsules) | User-ruled | never |
| P0-D4 | 2026-08-14 | Scale bar = 100,000+ bodies; perf designed in from Phase 1, measured Phase 4 | User-ruled | never |
| P0-D5 | 2026-08-14 | Debug data/scaffolding in CkFoundation (CkJolt); debugger module = presentation only | User brief + suite doctrine | never |
| P0-D6 | 2026-08-14 | Fable orchestrates, Opus executes, Sonnet mechanical | User directive (Fable budget) | user says otherwise |
| P0-D8 | 2026-08-15 | **User AFK directive: "complete all phases of this task"** — orchestrator runs Phase 3 close + Phase 4 end-to-end autonomously; per-phase LOCAL commits authorized by the established pattern (user approved commits for Phases 1–2 and asked for completion while away); **push / pointer bumps / ship remain WITHHELD** (outward; cross-repo publish guard). `[EDITOR-VERIFY]` items accumulate for the user's return | User directive | user returns |
| P0-D6b | 2026-08-15 | REINFORCED by user: "aggressively use Opus 5 agents to save on Fable 5 usage" — ALL implementation, research, review, docs-weld, and triage-drafting units go to Opus; Fable turns limited to rulings on STOPs, gate verdicts, PROGRESS updates. Where a triage could be drafted by an Opus agent (e.g. review-finding severity ranking with proposed fixes), draft it there and have Fable ratify | User directive | user says otherwise |
| P0-D7 | 2026-08-14 | Campaign docs at `Plugins/CkGameplayDebugger/docs/campaigns/2026-08-14-JoltDebuggerWorldViewport/` | Suite precedent (4 sibling campaigns there); UI is the campaign's center of gravity | — |
| P1-D8 | 2026-08-14 | **Front-end/target split** (rules INV-E): ONE `JPH::DebugRenderer` subclass instance — VERIFIED constraint, `JoltPhysics/Jolt/Renderer/DebugRenderer.cpp:75` `JPH_ASSERT(sInstance == nullptr)` in ctor — renamed `FCk_Jolt_DebugRenderer`, owning only the world-agnostic geometry/batch cache. Per-world retained state (ISM buckets, MIDs, mode, palette) moves to a new `FCk_Jolt_DebugDrawTarget`; the subsystem's in-world draw becomes the default target; the debugger registers its own target bound to its preview world. Singleton lifetime moves out of per-subsystem ownership to a module-level holder torn down on `OnEnginePreExit` (executor proposes exact shape; STOP on GC entanglement) | Two simultaneous renderer instances assert; targets share the geometry cache (meshes are world-agnostic) | never — Jolt-version bump changes the assert |
| P1-D9 | 2026-08-14 | **Capture runs inside CkJolt's processor pipeline** (rules INV-D), in the async-safe window (RunAfter `FProcessor_JoltWorld_WaitForAsync` + SleepStateMirror, RunBefore `FProcessor_JoltWorld_Step`), pumping every registered+demanding target. Works identically in sync and async physics modes; the Slate/debugger side NEVER touches `JPH::PhysicsSystem`. Retained ISMs mean a skipped frame just shows last state | The Slate tick races the in-flight async step; the pipeline window is the only safe read point. Also satisfies P0-D5 (CkFoundation processors update debug data) | if Jolt gains a step-barrier read contract |
| P1-D10 | 2026-08-14 | **Two-pass capture with persistent per-body slots** (rules INV-B): static bodies (incl. baked static world) captured once per static-revision (level streaming / re-bake invalidates); per-frame work = active bodies (`GetActiveBodies` precedent `CkJoltWorld.cpp:249`) + sleep/activation transitions for recolor. Buckets hold persistent body→instance slots; no full desired-array rebuild per frame | O(active) per frame, not O(all bodies); the 100k bar (P0-D4) is unreachable with per-frame full walks | Phase-4 measurement contradicts |
| P1-D11 | 2026-08-14 | **Color = per-(geometry, color-class) buckets** (rules INV-A), palette ≤ ~10 classes (Static, Kinematic, Dynamic-Awake, Dynamic-Sleeping, Sensor, Character, BakedStatic, +dim variants). Per-instance custom data REJECTED for v1: needs authored materials wired for it, GPU updates are Movable-proxy-only, and bucket count is bounded by the small palette. Sleep flips move an instance between two same-geometry buckets — bounded by active count | Keeps the proven bucket code path; cheapest correct thing | palette grows user-configurable per-body colors |
| P1-D12 | 2026-08-14 | **ISM creation stays `Request_CreateNewObject` + `RegisterComponentWithWorld(TargetWorld)`** (rules INV-C) — proven in game worlds (`CkJolt_DebugRenderer.cpp:207-223`); preview-world compatibility must be proven by a spec BEFORE any Phase-2 UI work; fallback branch = `FPreviewScene::AddComponent`-equivalent registration | Smallest delta from the working path; the spec converts the assumption into evidence | spec fails → fallback branch, re-rule |
| P1-D14 | 2026-08-14 | **Debug-draw ISMs: plain `NewObject` + `RegisterComponentWithWorld`, owned via `TStrongObjectPtr` in the bucket; ObjectPooling wrapper DROPPED for these** (both game and preview worlds). Resolves the PreviewWorldCompat STOP: `EditorPreview` worlds have no ObjectPooling subsystem, and the wrapper's only value here was GC-pinning (RecyclePolicy was DestroyOnRelease = no recycling), which strong ownership provides without a subsystem dependency. Also closes the caller-owned-unrooted GC exposure the executor flagged | Doctrine: TStrongObjectPtr when the owner controls lifetime; smallest blast radius of the four options (no CkCore change, no consumer burden, no world-type compromise) | pooling ever gains real recycling value for debug ISMs |
| P1-D15 | 2026-08-14 | **BakedStatic classification = body UserData → entity `Has<FFragment_JoltStaticActor_Current>`** (executor deviation RATIFIED). The brief's "object layer" mechanism does not exist — baked statics and Static-motion JoltBodies share `ECk_Jolt_BodyDomain::Static` per-signature layers. Registry lookups run only in the revision-keyed pass, on the game thread (not a Jolt callback — module rule intact) | Only correct mechanism available; module's own attribution doctrine | a dedicated layer ever appears |
| P1-D16 | 2026-08-14 | **Sleeping-at-open fix**: the revision-keyed full pass includes ALL currently-inactive bodies (statics AND sleeping dynamics → sleeping set), and JoltBody Setup bumps the scene revision when a body is added asleep (`InitialSleepState::Asleep`). Closes the executor's deviation 2 (asleep-at-registration or asleep-spawned bodies drew nothing until woken) | "Draw ALL bodies" is a campaign hard requirement; revision bumps on asleep-spawn are rare and only cost when a target is demanding | Phase-4 measurement objects |
| P1-D17 | 2026-08-14 | **Multi-world PIE draw ratified**: every Jolt subsystem now gets its own default target, so server+client PIE worlds each draw under the same CVars (previously only the first world, an artifact of the sInstance gate). Accepted as a strict improvement; must be noted in the docs weld | Old behavior was a singleton artifact, not a design | user objects |
| P1-D13 | 2026-08-14 | **RULED (post-investigation): wireframe = branch (a)** — direct `LoadObject<UMaterial>` of `/Engine/EngineDebugMaterials/WireframeMaterial.WireframeMaterial` (NEVER via `GEngine->WireframeMaterial`, which is null when `RequiresCookedData()` — `ShaderCore.cpp:606-610`), MID with `Color` param per bucket, mode toggle = ISM material swap. Evidence: `Material.h:939-941` `Wireframe` flag is runtime-unconditional; asset has `bUsedAsSpecialEngineMaterial` (bypasses ISM usage checks, `Material.cpp:2008,2043`) + a `Color` vector param; wireframe fill-mode path is VF-agnostic (`MeshPassProcessor.cpp:1823-1828`). Packaged-cook inclusion of `/Engine/EngineDebugMaterials/` is UNPROVEN — but identically unproven for the EXISTING solid material, so no regression vs status quo; logged as `[PACKAGED-VERIFY]` covering both. Fallback if that verify fails: branch (b), a CkUsf-generated look (`_Wireframe` flag + trivial .ush + AS asset decl + regen commit — mechanism documented in the Unit-I report) | Smallest delta, mirrors the proven solid-path load at `CkJolt_DebugRenderer.cpp:231-244`, no human editor step on an unproven risk | packaged verification fails → switch to branch (b) |

## Open items
| Item | Status | Next step |
|---|---|---|
| INV-A color mechanism | RULED → P1-D11 | — |
| INV-B static-body zero-cost model | RULED → P1-D10 | — |
| INV-C ISM registration in preview world | RULED → P1-D12 (spec must prove it) | Phase-1 work item 5 |
| INV-D async-physics capture strategy | RULED → P1-D9 | — |
| INV-E renderer rename/architecture | RULED → P1-D8 | — |
| P1-D13 wireframe material source | RULED → decision log P1-D13 (branch (a)) | — |
| Baseline gate | DONE — 1146/1150, 4 named pre-existing fails recorded above | — |
| `[PACKAGED-VERIFY]` both debug materials (`M_SimpleUnlitTranslucent` + `WireframeMaterial`) load in a packaged Development build | OPEN (pre-existing risk, now named) | Verify during Phase-4 / next packaged acceptance pass; on failure switch wireframe to P1-D13 branch (b) |
**Rule: no completion claim may be written anywhere in this file while any row here is unresolved.**

## Follow-ups recorded, not chased
- `FCk_Handle::View() const` in CkEcs is broken (ensure-recovery branch returns non-const view;
  never instantiated until now) — executor worked around with a handle copy in
  `CkJoltDebugDraw_Capture.cpp`. Fix belongs to CkEcs, one line + a use; NOT this campaign.
- Dead-sleeping sweep is O(sleeping bodies) `TryGetBody` pointer checks per capture — measure in
  Phase 4 before optimizing.
- `Ck.Jolt.Body.Lifecycle.BatchFiveHundredSingleFrame` + `Ck.Jolt.Body.Benchmark.FrameCostMatrix`
  went red ONLY under the executor's auto-parallel scoped run, each with a single SQLite
  `Saved/Search/FileInfo.db` disk-I/O error line (known parallel-lane contention signature; both
  green in the serial baseline). Orchestrator's serial full gate is the arbiter.

## Dated entries (append-only, newest first)

### 2026-08-15 — Phase-4 fix-up landed (Opus); gate of record launched
- F1: `UCk_Jolt_Subsystem::Request_NoteBodyRemoved()` wrapper (no in-tree caller yet — probe uses
  the FJoltWorld context directly, the processor idiom); Probe EndPlay resolves/caches the world
  in a new `DoTick` and bumps body-removed ALWAYS + static-scene when the probe is Static (mirrors
  JoltBody EndPlay); funnel doc rewritten (JoltBody EndPlay, Probe destroy, StaticWorld removals;
  "any new destroy site must bump"). F2/F3: record gains `_MotionType`+`_IsSensor`, both
  accepted-gap rationales in its comment. F4: `Settings.ConstructRestoresPreferences` with RAII
  CDO guard. F5: `LostFocus` clears the pending pick; LMB-consumed documented. F6: both spec
  files use `Make_BodyKey`. F8/F10 docs. F7 deferred.
- Executor gates (final binary): Jolt 76/78 auto-lanes → the two lane-noise names 16/16 serial;
  14/14 DebugDraw incl. Benchmark; JoltDebugger 9/9; DebuggerLauncher 3/3; **Probe 27/27**
  (`SpatialQuery` pattern matches no tests — probe coverage is `Ck_AutoTest_Probe*`).
- New `[EDITOR-VERIFY]` line: destroy a Static probe with the debugger open → its sensor instance
  disappears (static-probe funnel half is reasoned, not headless-covered).

### 2026-08-15 — Phase-4 adversarial review: 10 findings; triage RATIFIED [P4-D37]
- CLEAN: content bounds, bucket pruning, record invalidation sites (except class), Release_BodySlots
  reachability, threading, stat split, both new specs discriminating, benchmark teardown, settings
  class precedent, outliner pin/sort, const& selection, modifier probe, doctrine, PACKAGED-VERIFY.
- **[P4-D37]** FIX-NOW: **F1** (HIGH — sweep gate lost coverage of bodies destroyed outside JoltBody
  EndPlay: Probe destruction at `CkProbe_Processor.cpp:1216` bumps nothing → a rested
  kinematic/dynamic probe's instance/overlay stays drawn indefinitely) → Probe EndPlay resolves
  `FJoltWorld` context and calls `Request_NoteBodyRemoved()`; add `UCk_Jolt_Subsystem` wrapper;
  fix the "ONE funnel" doc wording. **F2** (class absent from skip oracle) → RULING: record
  `IsSensor` + `MotionType` in the record (cheap body reads); the BakedStatic attribution axis is
  ACCEPTED as a documented gap (attribution flips only when a StaticActor entity dies, which
  already routes the bodies through the static-revision funnel — the flip is not observable in
  practice; note it in the record's comment). **F4** (settings spec tautological + restore not
  failure-safe — a failing prepass could leave test values on the CDO and a later user toggle
  would persist them to the REAL ini) → rename `ConstructRestoresPreferences`, drop the
  same-CDO assert, RAII-guard save/restore. **F5** clear `_PendingPickPress` on focus/capture
  loss; document LMB swallow. **F6** benchmark + reconcile spec use `Make_BodyKey`. **F8/F10**
  doc wording ("measurement with loose sanity gates"; "position + rotation + shape";
  `_BodiesCaptured` = drawn). ACCEPT-AS-DESIGNED: **F3** (BodyID recycle alias — record the
  assumption in the comment), **F9**. DEFER: **F7** (log the layer fallback), F5 DPI scaling.
- BEFORE column CONFIRMED by orchestrator: executor's report states both columns are verbatim
  log lines from a same-binary A/B (BEFORE run on the pre-D31/D32 binary, AFTER after) — the
  report is the evidence; the spec file itself is unchanged between the runs.

### 2026-08-15 — Phase 4 implementation unit done (Opus); MEASURED; review dispatched
- **Benchmark `Ck.Jolt.DebugDraw.Benchmark.ScaleMatrix` (headless, same binary A/B, ms):**
  | case | N=1k B→A | N=10k B→A | N=100k B→A |
  | first_full_pass | 1.67→1.54 | 4.93→6.52 | 57.6→69.8 (writes a record/body; accepted) |
  | steady_state_avg (1k awake) | 2.06→2.73 | 2.28→2.72 | 2.07→2.59 (flat, O(active)) |
  | revision_rerun | 5.63→2.83 | 22.7→3.93 | **260.5→22.9** |
  | pick_body | 0.31→0.32 | 1.08→1.79 | 13.4→14.0 |
  | highlight_rearmed_pass | 4.63→3.04 | 26.5→4.14 | **249.9→23.6** |
- Landed (uncommitted): P4-D31 incremental full pass (per-body pos/rot/shape record → skip
  unchanged, add unslotted, release gone), P4-D32 body-removed revision (`FJoltWorld::
  _BodyRemovedRevision`, EndPlay funnel for ALL motion types, sweep gated on it), P4-D33 #8/#10/
  #14(a)/#15/#17/#18, P4-D34 `UCkJoltDebuggerSettings` (Config=GameUserSettings; render mode,
  populations, camera preset; spec `Settings.RoundTripAndRestore`), docs welded in both
  CLAUDE.md incl. `[PACKAGED-VERIFY]` steps. Two existing assertions STRENGTHENED (idempotent
  re-run now asserts 0 updates + moved-static case). #14(b) character-overlay spec SKIPPED —
  fragment friends are production-only; stays `[EDITOR-VERIFY]`.
- Executor gates: Jolt 76/78 auto-lanes → both reds green serially (CameraPresets = SQLite lane
  noise; FrameCostMatrix = documented over-capacity solver ensure under 4-lane load); JoltDebugger
  9/9 serial; DebuggerLauncher 3/3; 14/14 DebugDraw specs.
- Inferred: residual ~23 ms at 100k = the walk itself (not profiled per-line); live PIE adds
  ECS attribution cost the headless fixture short-circuits.

### 2026-08-15 — Phase-3 fix-up + docs weld landed (Opus); gate of record launched
- All P3-D29 FIX-NOW items implemented (per-finding file:line table in executor report).
  Facility API added: `ck::jolt::debug_draw::Make_BodyKey(uint32)`, `FCk_Jolt_DebugDrawTarget::
  Get_HighlightedBodyLinearVelocity() -> TOptional<FVector>` (sampled by capture in the
  async-safe window; unset for characters / bodies not drawn this capture — documented).
  Debugger no longer calls any Jolt utils for live body state. Module `_DebuggerTabName` static
  replaced by `Get_DebuggerTabName()` → `SCkJoltDebuggerWindow::TabId` (avoids cross-TU FName
  init-order hazard). Specs: `Outliner.RowsSelectFilterAndSurviveRefresh`,
  `Detail.RowsReflectTheSelection`, `Jolt.DebugDraw.HighlightedBodyLinearVelocity`.
- Executor gates: JoltDebugger 8/8, DebuggerLauncher 3/3, Jolt 74/75 (12/12 DebugDraw; red =
  SQLite lane noise on `FrameCostMatrix`). Docs: `CkJoltDebugger/CLAUDE.md` (outliner, selection
  model, click-picking, detail, route/picker, pending target, known-costs worklist, 8 specs,
  editor-verify), `CkJolt/CLAUDE.md` (selection surface, Highlight class, 12 specs, 2 anti-patterns).
- Caveat: executor made ONE comment-only source edit after its gates → orchestrator gate of record
  (below) rebuilds + retests the final artifact and is the arbiter.
- Behavioral note for PIE: velocity row reads `--` for a static/sleeping body once its revision
  pass has passed, and always for characters (P3-D27 as reopened).

### 2026-08-15 — Phase-3 adversarial review: 20 findings; Opus-drafted triage RATIFIED [P3-D29]
- CLEAN: route order + non-open-only, shared predicate, picker lifecycle, echo suppression
  (triple-guarded), click-trap-free rows, single teardown path, pick math, character-key
  single-sourcing, slot-pair release, doctrine.
- **[P3-D29] Triage ratified as drafted, with one orchestrator ruling:** #2 (`Get_LinearVelocity`
  reads `PhysicsSystem` from Slate — forbidden, races async step) → **P3-D27 REOPENED: velocity
  is sampled by the CAPTURE PROCESSOR** for the highlighted body only (`FCk_Jolt_DebugDrawTarget::
  Get_HighlightedBodyLinearVelocity() -> TOptional<FVector>`, written in the async-safe window),
  the debugger reads that; no Jolt utils calls from the debugger for live body state.
  FIX-NOW: #1 pending-target steals selection + never expires; #2 (per ruling); #3 BodyKey 0
  aliasing → `TOptional<uint64>` + row lookups by Handle; #4 baked static picks resolve via a
  key→row map over ALL `_BodyIds`; #5 external selection must reach filtered-out rows (search
  unfiltered set, reveal); #6 pending target carries world identity; #7 identity key
  (Handle, Population); #9 doc the per-click full-pass cost; #12 `Make_BodyKey` public helper;
  #13 non-vacuous outliner/detail specs; #19 single TabId definition; #20 drop "v1" breadcrumbs.
  DEFER → Phase 4: #8, #10, #14, #15, #17, #18. ACCEPT-AS-DESIGNED (doc): #11, #16.

### 2026-08-15 — Phase 3 Unit IX (outliner/selection/picking/detail) done; review dispatched
- CkJoltDebugger (uncommitted): `Data/CkJoltDebugger_Types.h` + `DataCollector` (four fragments;
  StaticActor rows keyed by FIRST body id — v1), `SCkJoltDebugger_OutlinerPanel` (pointer-identity
  list, dual search, pill, copy menu), `SCkJoltDebugger_DetailPanel`, 3-way splitter, selection
  model with 5 sources / sinks exactly per P3-D25 (broadcast only for user-originated Outliner/
  Viewport), `Is_JoltDebuggerEntity` shared by module route + picker filter, route registered
  after spawner / unregistered before, Frame Selection + `F` restored (`Set_SelectionBounds` fed
  each Tick from `Get_HighlightedBodyBounds`), viewport plain-LMB pick via `TryPick_Body`.
  Build.cs +CkSpatialQuery. Specs +2 (Outliner/Detail construct). Executor gates: JoltDebugger
  6/6, DebuggerLauncher 3/3, Jolt 70/72 (both reds SQLite lane noise; all DebugDraw green).
- Not done by design: collector spec (no headless JoltBody fixture exists — only the net-PIE
  harness); docs weld → Unit X.
- Inferred: JoltBody/Probe key widening `static_cast<uint64>(GetIndexAndSequenceNumber())` —
  facility header states the convention only for characters. Confirm in review + editor.

### 2026-08-15 — Phase 3 Unit VIII (facility highlight/pick) done; Unit IX dispatched
- CkJolt (uncommitted): `Highlight` colour class (8th, static_assert holds), palette entry;
  `Set_/Get_HighlightedBody`, `Get_HighlightedBodyBounds` (reads the NORMAL instance → available
  instantly on selection), `TryPick_Body` (oriented-box slab test in instance space, parametric
  distance, skips hidden classes + Highlight), `ck::jolt::debug_draw::Make_CharacterBodyKey`;
  keyspace constants unified (`CharacterKeyBit=1<<40`, `HighlightKeyBit=1<<41`); public
  `Release_BodySlots` also releases the paired overlay; `Set_HighlightedBody` re-arms the full
  pass so static/asleep bodies highlight without waiting for a revision. **[P3-D28] all ratified.**
- Specs +3 (`HighlightAddsOverlayInstance`, `HighlightedBodyBounds`, `PickNearestBody`); scoped
  Jolt 69/70 (11/11 DebugDraw green; red = SQLite lane noise on yet another rotating test).
- Executor's spec-first red (bounds assumed to require re-capture) was a wrong assertion,
  corrected — recorded as honest process, not a facility bug.

### 2026-08-15 — Phase-2 `[EDITOR-VERIFY]` A–D: 12/12 PASSED (user, live PIE)
- VERIFIED by human: legacy in-world draw parity + live opacity/sleep-coloring + same-frame hide
  (A1), multi-world server+client draw (A2/P1-D17), sleeping-at-open + asleep-spawn draw
  immediately (B3–4, P1-D16 funnel live), PIE Stop survives + re-PIE repopulates (C5,
  OnWorldCleanup path), viewport shows same bodies/colors + stat rail (D6), all 7 presets + Frame
  All + orbit/pan/zoom/flight with NO ortho clipping (D7, P2-D20 #8), Frame All respects hidden
  populations (D8, P2-D19), wireframe toggle (D9), population toggles + legend (D10), in-world
  toggles game-viewport-only (D11), demand-off on tab switch (D12, P2-D20 #1).
- Every "inferred" line carried from Phase 1/2 is now closed. Remaining open: `[PACKAGED-VERIFY]`
  for the two engine debug materials (Phase 4).

### 2026-08-15 — Phase-2 fix-up (Unit VII) landed; [P2-D21] framing-movement assertion ruled OUT
- All 10 P2-D20 fixes implemented + evidence (executor report): demand push via
  `FGlobalTabmanager::OnTabForegrounded_Subscribe` (fires both directions; SDockTab has no
  deactivate delegate; ordering verified `SDockingTabWell.cpp:675→696`) + explicit demand-off
  before unregister in dtor; FrameSelection removed; lanes restructured (Primary = viewport
  render mode; "In-world draw" Context group holds the CVar toggles with game-viewport tooltips);
  `Count` sentinel + `static_assert(<=8)`; dead camera-preset getter removed; `HandleEndPie`
  DELETED (confirmed redundant: `CkDebuggerCommon_Module.cpp:64-67,109` broadcasts
  SessionInvalidated on EndPIE); ortho eye backed off `Radius*2` along −view; `[0]` guarded;
  ContentBounds spec off-origin + restore-on-unhide; viewport specs `CameraPresets` (7 presets,
  projection + direction) and `FrameAllWithoutContentIsInert`.
- Scoped gates (executor): JoltDebugger 4/4; Jolt 66/67 (8/8 DebugDraw; sole red = SQLite lane
  noise, `FrameCostMatrix`, 6th rotation); DebuggerLauncher 3/3. Build exit 0.
- **[P2-D21]** finding-7 second half (FrameAll moves the camera toward real content bounds)
  RULED OUT of headless spec coverage: option (a) breaks P1-D9 (debugger never touches
  `PhysicsSystem`), option (b) adds production API solely for a test. Preset math is
  spec-covered; framing-on-real-content and the ortho eye offset go to `[EDITOR-VERIFY]`.
- Crowd follow-up recorded (not chased): `SCkCrowdDebugger_3dViewport.cpp:632-637` ortho
  FrameBounds places the eye at the box center — carry the Jolt fix over during the Crowd
  refactor campaign.

### 2026-08-15 — Phase-2 adversarial review: 10 findings; triage ruled [P2-D20]
- CLEAN: destruction ordering (target dies before preview world), registration/no-dangle,
  preview world ticked (ISM render-state flushes), GC, Slate contracts, all 9 camera buttons
  map correctly, all 7 color classes toggleable, ContentBounds zero-instance guard, packaged
  safety, module lifecycle, doctrine.
- **[P2-D20] FIX NOW (Unit VII):** #1 demand latches ON when the tab is backgrounded (Tick stops
  → last-seen visible sticks; capture runs forever into an invisible world) — pull→push:
  drive demand from tab foreground/activation delegates in addition to Tick, and force
  demand-off in the window's `OnTabClosed`/destructor path; #2 dead FrameSelection: DELETE the
  preset, member, button and F-hotkey duplication for Phase 2 (Phase 3 re-adds them WITH a
  selection producer — recorded as a Phase-3 work item); #3 the Primary-lane CVar toggles gate
  the in-world draw, not the viewport — MOVE them to a Context group labelled "In-world draw"
  and give the Primary lane the viewport controls (render mode); #4 `_HiddenClassMask` uint8
  silent overflow → `static_assert` on class count via a `Count` sentinel; #5 delete dead
  `Get_CameraPreset`/`_CameraPreset`; #6 route `HandleEndPie` to `HandleSessionInvalidated`
  (which clears the selector) — or delete it, since DebugSessionLifecycle already broadcasts on
  EndPIE (executor confirms by reading the lifecycle source, no guessing); #7 viewport spec:
  add real assertions — ApplyPreset branches set expected projection/rotation, Set_Target +
  a synthetic bounds → FrameBounds moves the camera; #8 ortho FrameBounds camera at box CENTER
  (inherited from Crowd — solid ISMs will show half the bodies clipped): back the eye off along
  the view direction by the bounds' extent (fix here; note as a Crowd follow-up); #9 guard
  `InColorClasses[0]` with `CK_ENSURE_IF_NOT`; #10 ContentBounds spec asserts restore-on-unhide.
- Gate of record (launched before review) will be invalidated by the fix-up → re-run after.

### 2026-08-15 — Phase 2 Unit VI done (viewport shell); gate + adversarial review in flight
- CkJoltDebugger (uncommitted): `Viewport/SCkJoltDebugger_3dViewport.{h,cpp}` (Crowd shell,
  9 presets, no PDI draw), window migrated to `FCkDebuggerModel_WorldSelector`, SSplitter
  viewport 0.72 / stat rail 0.28, one target bound to preview world with ungated demand sync +
  subsystem re-registration on pointer change; `OnWorldChanged`/session-invalidated/`EndPIE` →
  one handler; module gained canonical `OnEnginePreExit` teardown; Build.cs +RenderCore, RHI,
  InputCore, UMG; new spec `Ck.JoltDebugger.Viewport.ConstructsWithoutEnsure`.
  Icons: camera = Crowd's; wireframe = `Grid`; populations = `Jolt`/`World`/`Probe`/`Person`.
- Executor evidence: builds exit 0 (one with --generate); JoltDebugger specs 2/2; launcher
  census 3/3. `Get_CameraPreset()` records FrameAll/FrameSelection verbatim (nothing consumes
  it yet — noted for Phase 3 if an active-button highlight lands).
- Orchestrator: full serial gate of record launched; fresh adversarial review dispatched.

### 2026-08-15 — Phase 2 Unit V done (facility extensions); Unit VI dispatched to fresh executor
- CkJolt (uncommitted): `Set_ClassVisibility`/`Get_IsClassVisible` (bucket-level SetVisibility,
  `_HiddenClassMask`, new buckets seed visibility from class), `Get_ContentBounds()` (JPH-free
  FBox via `CalcBounds` — the cached `Bounds` lags render-state update; real defect caught by the
  spec and fixed). Two new specs: `ClassVisibility`, `ContentBounds`. Scoped Jolt 63/64 (8/8
  DebugDraw green; sole red = 5th rotating SQLite lane-noise victim). Builds exit 0.
- **[P2-D19] RATIFIED executor semantics:** hidden classes are STILL captured (skipping would
  freeze slots → stale pose on unhide, and entangles sleep-diff/sweep); `Get_ContentBounds`
  EXCLUDES hidden classes (consumer is camera framing — never frame invisible content).
- Executor stopped at the Unit-VI boundary by choice (context ~810k tokens; no ungated Slate).
  Research banked: `CkJoltDebugger.Build.cs` needs `RenderCore, RHI, InputCore, UMG` for the
  viewport (⇒ `--generate` on the following build); world lookup migrates from hand-rolled
  `FindGameWorld` (`SCkJoltDebuggerWindow.cpp:59-81`) to `FCkDebuggerModel_WorldSelector`;
  Crowd camera shell transfers directly with `Get_AllFrameBounds` → `Target->Get_ContentBounds()`.
- `[EDITOR-VERIFY]` draft (16 steps, A–D) received; integrated into PHASE_2.md at phase exit.

### 2026-08-15 — gate of record, part 1 (full suite) PASSED; part 2 (serial Jolt) in flight
- Full no-pattern serial suite on final artifact: **1150 total / 1147 pass / 3 fail / 0
  contaminated** — failing set {Crowd_NavQueryFilter_ForceReplan, PathNetworkFollower_
  DesiredNavmeshClearanceMovesInward, PathNetworkFollower_ProjectsRibbonWaypointWithinNavQueryExtent}
  ⊆ baseline's 4; `Homing_Retarget_SwitchesPursuit` flipped green (flaky, matches baseline note).
  ZERO new failures. Docs weld (CkJolt/CLAUDE.md rewrite) landed during the run, markdown-only,
  spot-checked.
- Coverage fact learned: the no-pattern suite runs ONLY the `Project.*` root (AS autotests) —
  `Ck.*` C++ specs are never in it (baseline's 1150 likewise). The `Ck.Jolt.*` half of the gate
  is pattern-scoped; orchestrator serial re-run (`--test-pattern Jolt --discover-fresh
  --parallel 1`) launched to verify the executor's 61/62 first-hand and arbitrate the roaming
  SQLite lane-noise red.

### 2026-08-15 — Unit IV complete; multi-target census PROVEN; scoped run 61/62
- New spec `Ck.Jolt.DebugDraw.MultiTargetBatchPrune` green: shared batch survives one target's
  destruction (holder census correct), survivor re-captures with pure slot reuse (0/0/1), prune
  fires only when the last Jolt geometry ref drops. 6/6 DebugDraw specs green; build exit 0.
- Design fact recorded: the batch prune only ever applies to per-shape geometry
  (ConvexHull/Mesh/HeightField/TaperedCapsule) — primitives draw through the renderer's
  lifetime-shared unit geometry and are correctly never prunable (Jolt design, matches the
  original prune rationale). Spec uses ConvexHull for exactly this reason (why-comment in test).
- Executor self-corrected a stale-green: its first Unit-IV build failed (TSharedRef::Reset
  misuse), it had launched tests on stale binaries, stopped them unread, rebuilt, re-ran. All
  reported greens backed by task exit codes.
- Sole red again = the roaming SQLite lane-noise signature (4th run, 4th different victim test).

### 2026-08-15 — Unit III (review fix-up) complete; all 12 in-scope findings fixed
- All P1-D18 FIX-NOW items implemented (per-finding file:line table in executor report); build
  exit 0; scoped Jolt run 60/61 — 5/5 DebugDraw specs green WITH hardened assertions (slot-reuse
  counters added 0/removed 0/updated 2 on bumped revision; PreviewWorldCompat ensure-free); sole
  red = `Ck.Jolt.BakeExtraction.MobilityPolicy` with the SQLite lane-noise signature (a different
  test each run — lane-position dependence is the evidence; prior reds passed this run).
- Executor micro-decisions RATIFIED: opaque public `FImpl` declaration (capture TU needs the
  name; `_Impl` stays private); internal `CkJolt_DebugDrawTarget_Impl.h` beside siblings (CkJolt
  has no `Private/` dir — inclusion is the privacy boundary).
- Remaining inferred (not yet verified): `OnWorldCleanup` release on a real PIE end
  (`[EDITOR-VERIFY]`, Phase 2); multi-target holder census with two live targets → closing NOW
  via a dedicated spec (Unit IV) before the gate of record.

### 2026-08-14 (late) — adversarial review returned 16 findings; triage ruled [P1-D18]
- Review (fresh Opus, read-only) found: threading CLEAN, core slot model CLEAN, plus 14 issues.
- **[P1-D18] Triage:** FIX NOW (Unit III fix-up, same executor) = #1 demand-off leaves frozen
  instances (parity with legacy HideAll), #2 dead-instance-id fast path never self-repairs,
  #3 target roots a dying world (add OnWorldCleanup reaction), #6 full-pass key-set copy
  (MoveTemp), #7 double-draw on coincident full-pass+sleep-diff, #8 static Teleport misses the
  revision funnel, #9 Set_Palette no invalidation + color class added to bucket key (oracle
  correctness), #10 stale-batch prune broken multi-target (track bucket-holder count; prune when
  JPH refcount == holders), #11 _SlotCount decrement outside guard, #12 public header leaks JPH
  into consumers (pimpl the internals; JPH-free public surface per FCk_Jolt_QuerySession
  precedent — MUST land before Phase 2 consumes the header), #13 Technique pipeline for the
  5-phase capture + minors (rename FCk_ prefix inside ck:: namespace, root the static base
  materials via TStrongObjectPtr — the raw-static dangle is real, fix header-comment inaccuracy,
  ensure-shape why-comment), #14 spec hardening (kill vacuous loops, assert
  added/removed/updated counts in StaticPassIsIdempotent — the load-bearing persistent-slot
  assertion, named constexpr bools, kTestFlags rename).
  DEFERRED to Phase 4 (measure first, scale bar's home) = #4 full-pass per-instance
  AddInstanceById + per-body alloc + WP-streaming re-run cost (needs incremental static diff /
  batch API), #5 dead-sleeping sweep cadence. Both added to PLAN.md Phase-4 summary implicitly
  via this entry.
- Gate of record was stopped mid-run (would be invalidated by the fix-up); re-launched after
  Unit III lands.

### 2026-08-14 (late) — Phase-1 Unit II complete (Opus executor), pending orchestrator gate + review
- Implemented (uncommitted, CkFoundation/CkJolt + CkTests specs): front-end/target split
  (`FCk_Jolt_DebugRenderer` + `FCk_Jolt_DebugDrawTarget`), capture processor
  (`FProcessor_JoltDebugDraw_Capture`, RunAfter WaitForAsync+SleepStateMirror, RunBefore Step),
  two-pass capture w/ persistent slots + sleeping-at-open coverage (P1-D16), class palette,
  wireframe/solid material swap (P1-D13a), plain-NewObject+TStrongObjectPtr ISM ownership
  (P1-D14). Revision funnels: `CkJoltBody_Processor.cpp:368` (Setup, Static OR asleep-spawn),
  `:952` (EndPlay, Static), `CkJoltStaticWorld_Subsystem.cpp:826` (batch add), `:414` (remove).
- Confirmed (executor evidence, to be re-verified by orchestrator gate): build exit 0; scoped
  Jolt run 60/61 — five `Ck.Jolt.DebugDraw.*` specs green incl. PreviewWorldCompat with zero
  ensures; sole red = `Ck.Jolt.Body.Benchmark.FrameCostMatrix` with the single SQLite
  lane-contention line (its twin from the prior run went green this run — lane-position noise).
- Inferred (needs later verification): asleep-spawn revision funnel behaviorally exercised only
  by direct capture calls, not through a live PIE pipeline (`[EDITOR-VERIFY]` in Phase 2);
  MID GC survival via ISM outer-chain reasoned, not forced-GC-tested; BakedStatic + character
  passes uncovered by headless specs (need live registry — PIE-level spec later).

### 2026-08-14 — campaign opened (orchestrator: Fable; research: 4× Opus Explore agents)
- Research fan-out (Jolt integration, Crowd debugger anatomy, rendering survey, scaffolding
  pattern) completed; all claims file:line-cited in agent reports (session-local).
- Confirmed: CkJolt ships a batched `JPH::DebugRenderer` (`CkJolt_DebugRenderer.{h,cpp}`,
  ISM-bucketed, `BatchUpdateInstancesTransforms`, no-op on unchanged transforms) — the
  generalization target.
- Confirmed: existing `CkJoltDebugger` window is stats-only (no per-body rows/selection/viewport,
  no `EntityTargetRoute`) — `SCkJoltDebuggerWindow.cpp`.
- Confirmed: Crowd debugger owns `FPreviewScene`+`FSceneViewport`+`FUMGViewportClient` with 9
  camera presets — the shell to copy (`SCkCrowdDebugger_3dViewport.{h,cpp}`).
- Confirmed: no debugger owns a ticking `FEcsWorld`; ISM/ECS subsystems exclude `EditorPreview`
  worlds (`CkIsmSubsystem.cpp:143-149`, `CkEcsEditor_Subsystem.cpp:31-46`).
- User ruled P0-D1…D4 via question round; doc set authored (PROMPT/PLAN/PHASE_1/PROGRESS).
- Left untouched for their owning sessions: superproject dirty
  (`Config/DefaultGameplayTags.ini`, `Plugins/CkFoundation` gitlink, `Plugins/CkTests` gitlink,
  root continuation prompts, `_scratch/`); CkFoundation `Content/CkUsf/GeneratedLooks/*` churn
  (known CkUsf-lane artifact).

## Session log
| Date | Orchestrator | What moved | Routing used |
|---|---|---|---|
| 2026-08-14 | Fable 5 | Campaign opened: research, user rulings P0-D1…D7, doc set authored | 4× Opus Explore (research); no executors yet |
| 2026-08-14→15 | Fable 5 | Phase 1 executed end-to-end: baseline, rulings P1-D8…D18, facility implemented, adversarially reviewed, 12 fixes, multi-target spec, docs weld, gate of record green | 1× Opus Explore (wireframe investigation), 1× Opus executor (4 sequential units via resume), 1× Opus adversarial reviewer; gates run by orchestrator |
| 2026-08-15 (cont.) | Fable 5 | Phase 3 + Phase 4 executed end-to-end autonomously (user AFK, P0-D8): outliner/selection/picking/detail, benchmark + incremental pass + sweep revision + settings, 2 review rounds (30 findings, 22 fixed), all committed locally; ship withheld | 3× Opus executors (P3 units VIII/IX/fix-up, P4 impl + fix-up), 2× Opus adversarial reviewers (drafted triage), gates run by orchestrator |
| 2026-08-15 | Fable 5 | Phase 1 committed; Phase 2 executed end-to-end: facility extensions, viewport shell, review (10 findings) fixed, rulings P2-D19…D21, docs weld, gate of record green | standing Opus executor (Unit V), fresh Opus executor (Units VI/VII + docs), 1× Opus adversarial reviewer; gates run by orchestrator |
