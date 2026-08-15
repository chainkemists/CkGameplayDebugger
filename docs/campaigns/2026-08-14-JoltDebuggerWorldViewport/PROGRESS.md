# Jolt Debugger — World Viewport — PROGRESS.md (living log)

## Current state  <!-- supersedes everything below; update at EVERY phase boundary and session end -->
**As of 2026-08-15 (late):** **PHASE 2 DONE, verified, UNCOMMITTED** — awaiting user commit
approval. Phase 1 committed earlier (CkFoundation 5edf6c152, CkTests fbd99927, CkGameplayDebugger
c804e7b; local, unpushed). Phase-2 gate of record (orchestrator, serial): full suite 1146/1150
failing set == baseline's 4 names (delta-zero); scoped Jolt+JoltDebugger 67/67 exit 0 (8/8
facility + 4/4 debugger specs). Two adversarial review rounds this campaign, 22 findings fixed
total. `[EDITOR-VERIFY]` A–D (12 steps) in PHASE_2.md awaits a human PIE pass.
**Commit scope (ours):** CkFoundation `Source/CkJolt/Subsystem/{CkJolt_DebugDrawTarget.h,.cpp,
_Impl.h, CkJolt_DebugRenderer.cpp}` (Unit V + P2 fix #4); CkTests the same spec file (2 new
specs + hardening); CkGameplayDebugger `Source/CkJoltDebugger/**` (Build.cs, Module.{h,cpp},
Window/SCkJoltDebuggerWindow.{h,cpp}, new Viewport/SCkJoltDebugger_3dViewport.{h,cpp}, new
Private/Tests/CkJoltDebuggerViewport.spec.cpp, new CLAUDE.md) + `docs/campaigns/…/{PHASE_2.md,
PLAN.md, PROGRESS.md}`. NOT ours: unchanged list from Phase 1.
**Next:** user commit approval → Phase 3 (outliner + selection sync + picking + detail; re-add
FrameSelection with a real producer).
**Commit scope (ours only):** CkFoundation `Source/CkJolt/*` (12 M + 6 new), CkTests
`Source/CkTests/Private/UnitTests/CkJolt/Test_JoltDebugDraw_TargetReconcile.cpp` (new),
CkGameplayDebugger `docs/campaigns/2026-08-14-JoltDebuggerWorldViewport/*` (new). NOT ours (never
stage): CkFoundation `Content/CkUsf/GeneratedLooks/*`, `docs/campaigns/request-completion-delegates/
CONTINUATION_PROMPT_Gate00CloseAndShip.md`, `docs/reviews/2026-05-08-CkNavigation-CTO-review.md`,
`docs/superpowers/`; superproject's pre-existing dirt.
**Baseline being diffed against:** full suite 2026-08-14: **1150 total / 1146 pass / 4 fail /
0 contaminated** — failing-set (all pre-existing, non-Jolt): Crowd_NavQueryFilter_ForceReplan,
Homing_Retarget_SwitchesPursuit, PathNetworkFollower_DesiredNavmeshClearanceMovesInward,
PathNetworkFollower_ProjectsRibbonWaypointWithinNavQueryExtent. Snapshot:
`C:\Users\sulfu\.claude\reports\baseline_20260814-214144.md`.
**Next action:** Unit II (CkJolt implementation, Opus, dispatched 2026-08-14 ~22:15) returns →
orchestrator re-runs full gate vs baseline → adversarial review → accept/bounce.
**Blocked on:** Unit II completion (in flight). Unit I DONE (report absorbed into P1-D13 ruling).

## Status board

| Phase | State |
|---|---|
| Docs/decisions | ✅ Done 2026-08-14 |
| 1 — CkJolt renderer facility | ✅ Done 2026-08-15, COMMITTED (5edf6c152 / fbd99927) |
| 2 — viewport shell | ✅ Done 2026-08-15 (uncommitted; 67/67 serial + full suite delta-zero; `[EDITOR-VERIFY]` pending) |
| 3 — outliner + selection | ⏳ Pending |
| 4 — scale + polish | ⏳ Pending |
| Ship | ⏳ Pending |

## Decision log
| # | Date | Decision | Why | Revisit when |
|---|---|---|---|---|
| P0-D1 | 2026-08-14 | Preview `FPreviewScene` world + generalized world-targetable batched ISM renderer; NO private EcsWorld/registry | User-ruled; zero-precedent scheduler work avoided; all pieces proven | Crowd-refactor campaign needs entity-based rendering |
| P0-D2 | 2026-08-14 | Wireframe = material swap on same ISMs (solid unlit ⇄ wireframe-flag material) | User-ruled; color survives both modes, no geometry rebuild | never |
| P0-D3 | 2026-08-14 | Draw+list all four populations (JoltBody, baked static world, Probe sensors, Character capsules) | User-ruled | never |
| P0-D4 | 2026-08-14 | Scale bar = 100,000+ bodies; perf designed in from Phase 1, measured Phase 4 | User-ruled | never |
| P0-D5 | 2026-08-14 | Debug data/scaffolding in CkFoundation (CkJolt); debugger module = presentation only | User brief + suite doctrine | never |
| P0-D6 | 2026-08-14 | Fable orchestrates, Opus executes, Sonnet mechanical | User directive (Fable budget) | user says otherwise |
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
| 2026-08-15 | Fable 5 | Phase 1 committed; Phase 2 executed end-to-end: facility extensions, viewport shell, review (10 findings) fixed, rulings P2-D19…D21, docs weld, gate of record green | standing Opus executor (Unit V), fresh Opus executor (Units VI/VII + docs), 1× Opus adversarial reviewer; gates run by orchestrator |
