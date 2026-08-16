# Jolt Debugger — World Viewport — PROGRESS.md (living log)

## Current state  <!-- supersedes everything below; update at EVERY phase boundary and session end -->
**As of 2026-08-15 — CAMPAIGN CLOSED. All 8 phases are DONE and COMMITTED LOCALLY on `dev` in each
of the three submodules.** Phases 1–4 delivered the world-targetable facility, the preview viewport,
the outliner/selection stack and the 100k hardening; phases 5–8 turned it from a shape viewer into a
physics debugger (draw channels + flags + colour modes; sim control + inspection + drag facility;
Unreal-scheme camera + command lanes + detail + drag UI; constraints/probes/health/labels/grid +
close-out). Rulings **P0-D1 … P8-D74** are in the decision log below; phase contracts are in
`PHASE_1.md` … `PHASE_8.md`.

**NEXT — user only: (1) the `[EDITOR-VERIFY]` pass (section below, 66 numbered steps + the retained
Phase-3/4 pointers), then (2) ship** per the instructions that follow. Nothing else is queued.

**SHIP IS WITHHELD (P0-D8 / P4-D36 — outward action, user-gated).** Nothing pushed; no superproject
pointer bumps; no downstream bumps.

**Local tips (all `dev`, all strictly ahead of `origin/dev`, all fast-forwards):**
- **CkFoundation `495214021`** — 495214021 (final fix-up + `TryPick_BodyHit`) ← ee073f2c7 (P8A) ←
  1df23d699 (P6 fix-up) ← 2bef8ef5c (P6B) ← 1242ba703 (P6A) ← 4e8b1c5a6 (P5) ← 2d0f71ced (P4) ←
  8e85733f1 (P3) ← 5b3ce3eb3 (P2) ← fb90e2f09 (branding) ← 5edf6c152 (P1)
- **CkTests `536d6013`** — 536d6013 ← 0accfe9c ← 551fe5f0 ← ecacf8c5 ← 756e7260 ← f866da75 (P5) ←
  59e3d1d6 (P4) ← 99e8c9f9 (P3) ← d039513b (P2) ← a6903924 (branding) ← fbd99927 (P1)
- **CkGameplayDebugger `6bde863`** — 6bde863 (final fix-up) ← 25e0c91 (P8B + Phase-7 fix-up) ←
  5fff942 (P8A) ← 4bdd694 (P7B) ← 1300de4 (P7A) ← d905760 ← a8cf391 (P5 docs) ← 1e24bfe (P5
  collateral) ← 688583b (P4 docs) ← 66f1e75 (P4) ← 0271004/f646b4a (P3) ← 00b4801/c1cbd5f (P2) ←
  5b954c7 (branding) ← c804e7b (P1 docs); **plus the docs close-out commit on top.**
- Superproject: local `dev` @ fd13be1 (branding) on top of origin/dev f51209d; HEAD detached at
  fd13be1. **No gitlink bumps made.**

**GATE OF RECORD (orchestrator re-read the logs; serial; FINAL artifact — the one built from
`6bde863` / `495214021` / `536d6013`):** full suite `Saved/Logs/BuildTest_Final_Full.log`
(2026-08-15 17:55) — **Total 1150 / Passed 1146 / Failed 4 / Contaminated 0**. The failing set is
EXACTLY the Phase-4 baseline set — **zero new, zero gone**:
`Ck_AutoTest_Crowd_NavQueryFilter_ForceReplan`, `Ck_AutoTest_Homing_Retarget_SwitchesPursuit`,
`Ck_AutoTest_PathNetworkFollower_DesiredNavmeshClearanceMovesInward`,
`Ck_AutoTest_PathNetworkFollower_ProjectsRibbonWaypointWithinNavQueryExtent`.
Scoped: `Jolt` **98/98** (`Test_Final_Jolt.log`, 18:04), `DebuggerLauncher` **3/3**, `Probe` **27/27**.

**Measured (final artifact, 100k default — MEASURED, NOT GATED):** steady **1.73 ms**, revision
**15.8 ms**, highlight **16.8 ms**, pick **12.5 ms**, first pass **48.6 ms**; all draw flags on
≈ **290–306 ms** (retained-lines optimization is a recorded follow-up, not chased).

**Final fix-up ([P8-D74]) landed:** F1–F7 (first half), F10, and P8-D73. **F6's second half — clearing
the hover when the hovered body leaves the outliner set — was SKIPPED** under the P5-D65 speed policy
and is documented as a **Known gap** in `CkJoltDebugger/CLAUDE.md` (the hover clears on the next mouse
move). F8 and F9 accepted no-change.

## Ship — instructions for the user (NOT executed; cross-repo publish guard)
> **Covers the WHOLE 8-phase campaign in one pass.** Every step below is outward and user-gated;
> the orchestrator executed none of them. Run the `[EDITOR-VERIFY]` pass FIRST — a red step is
> cheaper to fix-forward before the tips are published than after.

**0. Pre-check (do this before anything else).** For each submodule confirm it is *ahead only* —
never diverged, never behind:

```
git -C Plugins/CkFoundation        status -sb    # expect: ## dev...origin/dev [ahead N]
git -C Plugins/CkTests             status -sb    # expect: ## dev...origin/dev [ahead N]
git -C Plugins/CkGameplayDebugger  status -sb    # expect: ## dev...origin/dev [ahead N]
```

Any `behind` or `diverged` means someone else moved `origin/dev`: STOP and rebase per `ck-ship-dev`
before pushing. All three are expected to be clean fast-forwards of `origin/dev`.

**1. Push the submodules in dependency order** — CkGameplayDebugger consumes CkFoundation, and CkTests
carries the specs for it, so nothing may be pushed that references an unpushed SHA:

```
git -C Plugins/CkFoundation        push origin dev    # → 495214021
git -C Plugins/CkTests             push origin dev    # → 536d6013
git -C Plugins/CkGameplayDebugger  push origin dev    # → 6bde863
```

**Cross-repo publish guard: never bump a superproject pointer to a SHA that is not yet on `origin`.**
All three pushes must be green before step 2 begins.

**2. Superproject pointer bumps.** Close the editor first (the hook blocks `git checkout` while it
holds engine assets), then:

```
git checkout dev                                                        # attaches the local branding commit fd13be1
git checkout -b feature/jolt-debugger-world-viewport                    # branch tripwire: feature/ or bugfix/ ONLY
git add Plugins/CkFoundation Plugins/CkTests Plugins/CkGameplayDebugger  # stage ONLY the three gitlinks
git commit -m "chore(submodule): bump CkFoundation, CkTests, CkGameplayDebugger — Jolt debugger campaign (phases 1–8)"
```

⚠ **Stage only those three paths.** A blanket `git add .` would sweep up whatever else is dirty in
the working tree and silently attach another session's work to this commit.
⚠ **`feature/` or `bugfix/` only** — the build machine ignores every other prefix and the branch
fails silently: the push succeeds, the PR opens, and CI never runs. This work is additive →
`feature/jolt-debugger-world-viewport`.

**3. Push + PR** via `ck-ship-dev` / `ck-ship-pr`.

**4. `[PACKAGED-VERIFY]`** — package a **Development** build (DeveloperTool modules are excluded from
Test/Shipping), open the Jolt debugger, and confirm **both** engine debug materials render: bodies
solid, then wireframe on the toggle — not untinted-default and not invisible. A `Failed to load
WireframeMaterial` line in the log is a FAIL even if the window looks fine (the facility degrades to
Solid and ensures). Exact acceptance steps in `CkJolt/Claude.md` § "Colour + wireframe". On failure the
fallback is P1-D13 branch (b), a CkUsf-generated wireframe look.

**5. Downstream (BusterBlock) pointer bumps** — only after all three pushes in step 1 are on `origin`.

**6. Next campaign (already scoped by the user):** refactor the Crowd debugger onto this facility
(carry over: ortho eye-offset fix `SCkCrowdDebugger_3dViewport.cpp:632-637`, preview-world target
registration, outliner/selection contracts, and now the Unreal-scheme camera from P7-D49).

## Status board

| Phase | State |
|---|---|
| Docs/decisions | ✅ Done 2026-08-14 |
| 1 — CkJolt renderer facility | ✅ Done 2026-08-15, COMMITTED (5edf6c152 / fbd99927) |
| 2 — viewport shell | ✅ Done 2026-08-15, COMMITTED (c1cbd5f / 5b3ce3eb3 / d039513b); **`[EDITOR-VERIFY]` A–D 12/12 PASSED by user 2026-08-15** |
| 3 — outliner + selection | ✅ Done 2026-08-15, COMMITTED (8e85733f1 / 99e8c9f9 / f646b4a / 0271004); gate: full 1146/1150 fails ⊆ baseline (VatProxy red re-ran green = Zen DDC infra noise), scoped serial 75/75; `[EDITOR-VERIFY]` (7 steps in PHASE_3.md) pending user |
| 4 — scale + polish | ✅ Done 2026-08-15, COMMITTED (2d0f71ced / 59e3d1d6 / 66f1e75); measured; gate: full 1146/1150 ⊆ baseline, scoped 78/78; `[EDITOR-VERIFY]` (5 steps + static-probe) pending user |
| 5 — draw channels, draw flags, colour modes (CkJolt) | ✅ Done 2026-08-15, COMMITTED (CkFoundation 4e8b1c5a6 / CkTests f866da75 / CkGameplayDebugger 1e24bfe); gate Jolt 83/83, Launcher 3/3, Probe 27/27; `[EDITOR-VERIFY]` pending; Island mode dropped in P6 (P5-D66) |
| 6 — sim control, inspection, drag facility (CkJolt) | ✅ Done 2026-08-15, COMMITTED (CkFoundation 1242ba703/2bef8ef5c/1df23d699 · CkTests 756e7260/ecacf8c5/551fe5f0 · CkGameplayDebugger d905760); gate Jolt 91/91 + JoltDebug 10/10 (post-reboot re-run); `[EDITOR-VERIFY]` pending |
| 7 — camera, lanes, detail, selection, drag UI (CkJoltDebugger) | ✅ Done 2026-08-15, COMMITTED (CkGameplayDebugger 1300de4 (7A) / 4bdd694 (7B); fix-up folded into 25e0c91 + 6bde863); gate at landing JoltDebug 11/11 (`BuildTest_P7B_1.log` 16:20) + Launcher 3/3 (16:22); re-proven on the FINAL artifact by the gate of record (full 1146/1150 delta-zero, `Jolt` 98/98, Launcher 3/3, `Probe` 27/27); review 12 findings → [P7-D71]; `[EDITOR-VERIFY]` pending user |
| 8 — constraints, probes, health, labels, grid, close-out | ✅ Done 2026-08-15, COMMITTED (CkFoundation ee073f2c7 → 495214021 · CkTests 0accfe9c → 536d6013 · CkGameplayDebugger 5fff942 (8A) → 25e0c91 (8B + P7 fix-up) → 6bde863 (final fix-up)); **gate of record: full serial 1150/1146/4/0 with the failing set == Phase-4 baseline set exactly (zero new, zero gone), `Jolt` 98/98, `DebuggerLauncher` 3/3, `Probe` 27/27**; benchmark 100k steady 1.73 ms / revision 15.8 / highlight 16.8 / pick 12.5 / first pass 48.6, all-flags ≈ 290–306 ms; final review 10 findings → [P8-D74] (F6 second half SKIPPED → Known gap); `[EDITOR-VERIFY]` pending user |
| Ship | 🟢 READY — all 8 phases committed locally; push + pointer bumps await user (P0-D8/P4-D36). Instructions in the "Ship" section above; run the `[EDITOR-VERIFY]` pass first |

## `[EDITOR-VERIFY]` — user pass (phases 5–8)

Every step below is one live PIE action. They are consolidated and deduped from
`CkJoltDebugger/CLAUDE.md` § "What the specs cannot reach" and `CkJolt/Claude.md`; headless specs
construct widgets but cannot render, cannot deliver a real mouse-move, and cannot give a target real
content, so this list IS the remaining evidence. **The Phase-3 (7 steps, `PHASE_3.md`) and Phase-4
(5 steps + the static-probe line: destroy a Static probe with the debugger open → its instance
disappears) pointers stand unchanged** — run them too if they have not been run since Phase 4.

**In-world CVars (the Phase-5 re-host — verify NOTHING regressed for the existing consumer)**
1. `ck.Jolt.DebugDraw.Enabled 1` still draws every body, static and dynamic, as it did before.
2. `ck.Jolt.DebugDraw.Opacity` still changes in-world body translucency.
3. `ck.Jolt.DebugDraw.Velocity 1` draws **both** the linear and the angular arrow (one CVar, two flags).
4. `ck.Jolt.DebugDraw.WorldTransform 1` draws body axes at a **visible** size (the ×100 scale note).
5. `ck.Jolt.DebugDraw.Constraints 1` draws constraints in-world.
6. `ck.Jolt.DebugDraw.Contacts 1` draws contact points AND normals — and confirm it also arms them in the debugger preview (this toggle is process-wide, unlike every other flag).
7. `ck.Jolt.DebugDraw.SleepColoring 1` collapses statics/kinematics to one neutral colour each and distinguishes only awake vs asleep.
8. In-world body colours now follow the facility palette (grey static / green kinematic / yellow awake / dim red sleeping / blue sensor / tan baked-static / magenta character) instead of per-body distinct colours — confirm this reads as an improvement, not a regression.

**Viewport draw + contacts**
9. Bodies actually render in the debugger viewport and match the in-world draw.
10. Every Draw-lane toggle visibly changes the viewport — velocity, angular velocity, world transform, COM axes, AABBs, constraints, contacts, labels.
11. The whole Draw lane (all flags + colour mode) survives an editor restart.
12. Switching colour mode recolours the bodies and the legend follows.
13. The population toggles grey out outside BodyClass mode; returning to Class restores the toggles the user had set.
14. The wireframe/solid toggle flips render mode with the colour surviving both.
15. Demand goes off when the tab is backgrounded — click a sibling tab during PIE and the viewport stops capturing.
16. The window survives PIE end without crashing, showing its last state.
17. The contacts list fills for a resting body and its rows select the other body on click; a character selection lists none.

**Highlight, hover, labels**
18. A row click highlights that body **unmistakably** — opaque magenta, drawn over its neighbours.
19. Hovering a body highlights it subtly and shows its name.
20. The hover never fires mid-click or mid-drag, and leaving the viewport clears it.
21. **Known gap (F6 second half, deliberately skipped):** a hovered body whose row leaves the world keeps its hover until the mouse moves — confirm this is tolerable in practice.
22. Labels appear on the primary selection with **no** draw flag set.
23. Turning the Labels flag on labels bodies up to the 500 cap without tanking the frame rate (the cap logs once).
24. Labels sit on the right bodies at every camera angle and in both projections.

**Camera (Unreal editor scheme, P7-D49)**
25. RMB-drag looks around **without moving the eye**.
26. WASD/QE fly **only** while RMB is held; RMB+wheel changes fly speed.
27. MMB-drag pans; the wheel dollies along the view.
28. Alt+LMB orbits the current look-at; Alt+RMB dollies.
29. Plain LMB-drag tracks forward/back + yaw, and the **sign** feels right (drag down = forward) — a spec can only assert the eye moved along the view, not which way feels correct.
30. In an ortho preset RMB and MMB pan, the wheel zooms, and the view **never** rotates.
31. The ortho eye offset keeps solid bodies from being clipped by the near plane in Top/Front/etc.
32. Framing lands on real content — Frame All / `Home` and each ortho preset put the bodies on screen.
33. `F` frames the selection; `Ctrl+F` and `Alt+F` do **not** frame.
34. The wheel keeps flying forward past whatever was framed instead of stalling in front of it, and an ortho pan drags proportionally at every zoom level.

**Selection, isolate, follow**
35. The outliner lists all four populations with clean names and both searches filter them.
36. A viewport click on a body selects its row — including a body **past the first** of a baked-static actor (resolved through the collector's owner index).
37. A drag-then-release does **not** pick; a click does.
38. ECS debugger → a Jolt entity → "Open In → Jolt" lands on the row; "Sync from ECS" works; both reach a row the filter is currently hiding.
39. The game-viewport picker previews and picks only Jolt entities and their owner chain.
40. A selected row stays visible, dimmed, when a filter typed afterwards excludes it.
41. Ctrl+click in the outliner **and** in the viewport builds a multi-selection, all of it highlighted; the detail panel follows the **last** body clicked.
42. Isolate hides everything else, `I` toggles it, it re-applies as the selection changes, and with nothing selected it is inert rather than blanking the viewport.
43. Follow keeps the camera on a moving body **without re-framing it** — rotation and distance stay put.
44. The detail panel's velocity tracks a moving dynamic body and reads `--` for a character; the character group flips visible for a character selection and its rigid-body rows degrade.
45. Preferences persist across an **editor restart** — render mode, the four population toggles, and the camera orientation come back as they were left.

**Sim controls + stats**
46. Pause freezes the sim (**in-world too**); Step advances exactly one step.
47. Space toggles pause and Enter steps, with the viewport focused.
48. The stat rail shows the PAUSED pill, the step ms, and a body breakdown that visibly lags by up to 30 captures under its "(sampled)" labels.

**Drag (P7-D54 / P6-D47)**
49. Ctrl+LMB drag on a dynamic body pulls it around with a visible **yellow drag line**; release drops it and the line disappears.
50. The drag grabs the body **at the point clicked** and starts on the press — click a corner and the body hangs from that corner, not its centre, and the very first mouse move already pulls.
51. Ctrl+wheel pushes the drag plane away and pulls it back.
52. Dragging a **static or kinematic** body does nothing; Ctrl+LMB on **empty space** opens no drag (the previously selected body does not jump to the cursor).
53. Switching the world selector mid-drag drops the body and takes the drag line with it; closing the tab mid-drag does the same.

**Constraints, probes, health**
54. Constraints appear in the outliner with their type; selecting one highlights **both** bodies and turns the world's constraint reference frames on for as long as the selection holds.
55. "Open In → Jolt" from the ECS debugger reaches a constraint row.
56. A rope built by `UCk_Utils_JoltRope_UE` lists its constraints and they all highlight sensibly.
57. Selecting an overlapping probe with "Probe results" on draws contact points, normals and a line to each overlapping entity; the lines do **not** flicker between captures and disappear when the selection leaves the probe.
58. The tooltip explains why a persistent ProbeTrace has no points.
59. Throwing a body far below KillZ, or making one a runaway, lights the header badge **and** the Problems chip and narrows the list to it; slowing it down clears both.

**Grid, gizmo, bookmarks**
60. The ground grid gives the empty preview world a sense of scale — 1 m cells, a heavier line every 10 m, red/green axes through the origin; the toggle takes it away and brings it back, and it survives an editor restart.
61. The world-axis gizmo (bottom-left) points the right way in **every** camera preset and turns with the camera; clicking through it still picks the body underneath.
62. `Ctrl+3`, move the camera, then `3` returns it **exactly** — including the projection, so a bookmark taken in Top comes back orthographic.
63. An unused bookmark digit does nothing; `Ctrl+Alt+3` does not store; digits typed into the outliner's search box do not move the camera.

**Multi-world / authority**
64. In a **server+client PIE** session both worlds draw in-world under the same CVars (the old first-world-only behaviour was a singleton artifact and is gone — P1-D17).
65. The world selector switches the viewport between the PIE worlds and the outliner repopulates.
66. On a PIE **client** world the Drag chip is dark with an **authority-aware** tooltip and Ctrl+LMB does nothing (it still adds to the selection — only the drag is refused). ⚠ P7-D71/F11 moved the tooltip binding onto the `SCkDebug_IconToggle` itself — confirm the authority text is what appears on hover, not the toggle's generic one.

**`[PACKAGED-VERIFY]`** (not PIE — step 4 of the Ship section): both engine debug materials render in
a packaged **Development** build, solid and wireframe. Exact acceptance steps in `CkJolt/Claude.md`.

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
| P5-D38 | 2026-08-15 | **Line + text channels on `FCk_Jolt_DebugDrawTarget`.** Target owns a `ULineBatchComponent` in its target world (same NewObject + RegisterComponentWithWorld + TStrongObjectPtr ownership as the ISMs, P1-D14) and calls `Flush()` at the start of every capture flush; `FCk_Jolt_DebugRenderer::DrawLine/DrawTriangle` route to the ACTIVE target's line channel (no more direct `DrawDebugLine`); wireframe triangles = 3 lines. `DrawText3D` → target stores `FCk_Jolt_DebugDrawLabel {WorldPos, Text, Color}` per frame; consumers render labels (viewport OnPaint projection for the preview; the in-world subsystem via `DrawDebugString`). Public API also exposes `Draw_ExternalLine/Box/Sphere/Arrow(...)` for non-JPH contributors (probe results, health, drag visuals) — an "External" channel cleared per capture. If an `FPreviewScene` world cannot host a `ULineBatchComponent` (unverifiable headlessly until built) the fallback is a PDI `Draw` override on the viewport (Crowd mechanism) — executor STOPs and reports if hit. | JPH line/text output currently dead-ends in the game world; the preview viewport needs it, and the External channel is the only sane home for probe/health/drag visuals | preview world proves unable to host a LineBatchComponent |
| P5-D39 | 2026-08-15 | **Per-target draw flags** `FCk_Jolt_DebugDrawFlags` (bitmask): Shape (default on), Velocity, AngularVelocity, WorldTransform, CenterOfMassTransform, BoundingBox, MassAndInertia, SleepStats(if cheap; else drop), Constraints, ConstraintLimits, ConstraintReferenceFrames, ContactPoints, ContactNormals, SupportingFaces, Labels. Capture pass draws the per-body items itself from the flags (velocity arrow, axes, AABB…) — no `DrawBodies` in the capture; `DrawConstraints` called under the constraint flags. **The in-world draw is re-hosted onto the same flags**: the subsystem's default target's flags are sourced from the existing CVars (behavior-preserving; CVars remain the in-world source of truth) and the subsystem Tick path uses the same capture code (delete the duplicated `DrawSettings` block). | One vocabulary, two consumers; per-target so the preview and the in-world draw never fight | flags outgrow a 32-bit mask |
| P5-D40 | 2026-08-15 | **Contact recording.** JPH `sDrawContact*` statics are set to the UNION of registered targets' contact flags (recomputed when flags change). `FProcessor_JoltWorld_Step` wraps `PhysicsSystem::Update` in a renderer `Begin/EndRecord()` scope: `DrawLine`s during recording append to a frame buffer tagged Channel=Contact (cap 200,000 lines/frame, warn once at Display when hit); immediately after Update the renderer replays the buffer into every registered target whose flags request contacts (targets keep them until their next flush). Disclose in docs: contact toggles are process-wide (shared in-world + preview), unlike every other flag. | Contacts only exist DURING the solve; the statics are process-wide by Jolt's design and cannot be per-target | Jolt makes the contact draw flags non-static |
| P5-D41 | 2026-08-15 | **Highlight visibility.** Highlight colour → `{1.00, 0.15, 0.85}` (magenta, not near any palette entry), highlight bucket ALWAYS opaque (ignores palette opacity), overlay drawn at scale 1.03 about COM, highlight ISM `TranslucencySortPriority` = +1. Spec extends `HighlightAddsOverlayInstance`: asserts overlay instance scale ≠ 1.0 and highlight MID colour ≠ every palette colour. Add a **Hover** colour class (subdued: highlight at 0.5 alpha, scale 1.02) — see D43 for the mask. | User PIE report: the selected body is not visibly distinguishable | palette gains a magenta class |
| P5-D42 | 2026-08-15 | **Colour-by modes.** `ECk_Jolt_DebugDrawColorMode { BodyClass (default), SleepState, ObjectLayer, Island, ShapeType }` per target. Bucket key's colour-class becomes a `uint8` class index; visibility mask widens from `uint8` to `uint64` (static_assert ≤ 64 classes per mode); ObjectLayer > 61 → "Other"; Island → island index hashed into a 16-colour palette (islands churn per step: acceptable, active bodies re-draw every frame anyway; sleeping = "no island" class). Highlight and Hover classes are mode-independent (always the same two indices, always visible). Legend API `Get_LegendEntries(mode)` returns (class name, colour) so the debugger's legend follows the mode. | Body class alone can't answer "why is this asleep / what layer is this / what island is this" | a mode needs > 64 classes |
| P6-D43 | 2026-08-15 | **Pause / step.** Verify what sets `IsPaused()` today (grep the setter). Add `Request_StepOnce()` on the Jolt world: when paused, allows exactly one Step then re-pauses; exposed on `UCk_Jolt_Subsystem` (`Request_SetPaused`, `Request_StepOnce`, `Get_IsPaused`). Step processor also records `_LastStepDurationMs` (wall time of `Update`) on the world for stats. | Debugging a physics bug requires stopping time and advancing one step | — |
| P6-D44 | 2026-08-15 | **Selected-body detail sample.** Extend the selection sample struct (currently linear velocity only) to `FCk_Jolt_DebugDraw_BodySample`: linear+angular velocity, mass (inverse mass → mass, 0 = infinite), friction, restitution, gravity factor, motion quality, object layer, broadphase layer, IsSensor, AllowSleeping, world AABB, shape type/subtype, shape scale, island index (`MotionProperties::GetIslandIndexInternal()` — read-only debug use, document it), user data. For character selections `FCk_Jolt_DebugDraw_CharacterSample`: ground state, ground normal, ground body key, ground velocity, character velocity, up. Sampled in the capture pass only (P3 rule: no PhysicsSystem reads from Slate). Multi-selection: sample the PRIMARY only. | The detail panel currently answers almost nothing; every field is a cheap body read in the async-safe window | sample cost shows up in the benchmark |
| P6-D45 | 2026-08-15 | **Contacts of the selected body** via `NarrowPhaseQuery::CollideShape` with the selected body's shape at its COM transform, run in the capture pass on demand (only when a body is selected and the debugger asks): result list `{other body key, num contact points, penetration depth, contact points}` on the target; ContactPoints/normals for the selection are also drawn via the line channel. No ContactListener changes. | Answers "what is this body touching" without a listener, without per-step cost when nothing is selected | a ContactListener with retained manifolds appears |
| P6-D46 | 2026-08-15 | **Multi-select + isolate.** `Set_HighlightedBodies(TArray<uint64>)` (keeps `Set_HighlightedBody` as the 1-element convenience) — Highlight keys are `HighlightKeyBit \| bodykey` so many coexist; `Set_IsolatedBodies(TSet<uint64>)` — when non-empty the capture skips every body not in the set (all populations), plus `Clear_Isolation()`. Both re-arm the full pass. | Comparing bodies and clearing visual clutter are the two things a 100k-body viewer most needs | — |
| P6-D47 | 2026-08-15 | **Mouse-drag facility (sim-mutating, dev-only).** `UCk_Jolt_Subsystem::Request_BeginDrag(BodyKey/BodyID, WorldGrabPoint)`, `Request_UpdateDrag(WorldTargetPoint)`, `Request_EndDrag()`; requests queue on the subsystem and are applied by a processor that runs BEFORE Step (same group discipline as the capture); implementation = kinematic anchor body (no collision layer / non-colliding object layer) + `DistanceConstraint` (spring: frequency 2 Hz, damping 1, min/max 0) between grab point (body-local) and anchor; body activated on begin and each update; end removes constraint + anchor. Dynamic bodies only; ignored otherwise (log Verbose). Verify a non-colliding object layer exists in CkJolt's layer setup — if not, STOP-report which layer to use. | User PIE report: you cannot poke the simulation; this is the standard Jolt Samples mechanism | a non-colliding layer does not exist |
| P6-D48 | 2026-08-15 | **Stats extension.** World exposes step duration (D43), `PhysicsSystem::GetBodyStats()`, `GetNumActiveBodies(Rigid/Soft)`, constraint count (`GetConstraints().size()`), and — if CkJolt has an existing ContactListener — a per-step counter of OnContactAdded/Persisted for "contact pairs" (else omit contacts, record why). | The stat rail is the debugger's only whole-world view | — |
| P7-D49 | 2026-08-15 | **Camera = Unreal editor scheme.** RMB-drag = look in place (yaw/pitch, eye fixed); WASD/QE/arrows fly ONLY while RMB held; RMB+wheel = speed; MMB-drag = pan; wheel = dolly along view; LMB-drag = forward/back + yaw (UE style); Alt+LMB = orbit around the current look-at; Alt+RMB = dolly; F = Frame Selection, Home = Frame All (unchanged); LMB click without drag = pick (unchanged 4-px threshold). Ortho: RMB/MMB drag = pan, wheel = zoom, no rotation. Rewrite `InputKey`/`InputAxis`/`Tick_Navigation` only; the look-at is maintained as eye + forward × distance so Frame/orbit still work. Crowd viewport NOT touched. | User PIE report: the camera does not behave like the editor viewport, which is the muscle memory every user has | Crowd refactor campaign adopts it |
| P7-D50 | 2026-08-15 | **"Draw" command lane** — one checkbox per D39 flag (grouped: Bodies / Constraints / Contacts / Labels), colour-mode combo (D42), all persisted in `UCkJoltDebuggerSettings`; legend follows the mode. "In-world draw" lane exposes all six CVars. | The flags are worthless without a surface | — |
| P7-D51 | 2026-08-15 | **Sim controls** — toolbar Pause/Resume + Step buttons (D43) + Space (toggle) / Enter (step) when the viewport is focused; Pause state shown in the stats header. Stats panel gains D48 fields. | — | — |
| P7-D52 | 2026-08-15 | **Detail panel** shows every D44 field (two-column, grouped: Motion / Material / Layers / Shape / Misc; character group when applicable) and a "Contacts" list (D45) whose rows select the other body on click. | — | — |
| P7-D53 | 2026-08-15 | **Selection**: outliner multi-select (Ctrl/Shift), primary = last clicked; viewport Ctrl+click adds; all selected highlighted (D46); Isolate/Un-isolate toolbar toggle + `I` hotkey; Follow-selection toggle (camera keeps its offset to the primary's bounds centre every tick). | — | — |
| P7-D54 | 2026-08-15 | **Mouse-drag UI** — Ctrl+LMB press on a dynamic body begins drag (D47), drag moves the anchor on the camera-parallel plane through the grab point (Ctrl+wheel moves the plane along the view), release ends; a drag line (external channel) from grab point to anchor. Only when the selected world is the authority (server/standalone) — disabled with a tooltip otherwise. | Dragging a client-side proxy body would desync and teach the user a lie | — |
| P8-D55 | 2026-08-15 | **Constraints population** — if CkJolt exposes constraint entities/handles, list them keyed by handle with entity route; else list `PhysicsSystem::GetConstraints()` by index with body-pair display names and no route (record which branch applies after research). Selecting a constraint highlights both bodies + draws its reference frames. | — | — |
| P8-D56 | 2026-08-15 | **Probe results** — draw the selected probe's current overlaps/hits from the ECS fragments via the External channel; toggle in the Draw lane. | Probes are drawn as sensor shapes today but their RESULTS — the actual product — are invisible | — |
| P8-D57 | 2026-08-15 | **Health checks** — collector flags per body: NaN transform/velocity, \|v\| > threshold (setting, default 5000 cm/s), below world KillZ, zero-extent shape; outliner filter chip "Problems", warning badge with count in the stats header. | Finding the one broken body in 100k is the debugger's highest-value job | — |
| P8-D58 | 2026-08-15 | **Labels + hover** — labels channel (D38) rendered in viewport `OnPaint` (project world→screen with the current view); label on the primary selection always, on all bodies when the Labels flag is set (cap 500 nearest, log once); hover = throttled `TryPick_Body` on mouse move (≥ 60 ms) → Hover class + name tooltip. | — | — |
| P8-D59 | 2026-08-15 | **Grid, gizmo, bookmarks** — grid at Z=0 (100 cm cells, 20 m extent, major line every 10) via the External channel each capture (toggle, persisted); world-axis gizmo in the viewport's bottom-left in `OnPaint`; camera bookmarks Ctrl+0..9 set / 0..9 recall, persisted in settings. | Without a ground reference the preview world has no sense of scale or orientation | — |
| P5-D61 | 2026-08-15 | **STOP-list rulings (S1–S14), issued after the planner's research pass — all 14 closed, nothing blocks Unit XI.** **S1** contact recording moves INSIDE `FJoltWorld::DoPhysicsUpdate` around `PhysicsSystem::Update`, buffer = `FCriticalSection`-guarded append active only while some target wants contacts, **double-buffered and consumed on the GAME THREAD by `FProcessor_JoltDebugDraw_Capture`** (not replayed in the Step processor); one-frame latency under async physics is accepted and documented. **S2** drag-anchor layer = a lazily registered default-constructed `FCk_Jolt_CollisionSignature` (`_ResponseMask = 0`, `_Domain = Dynamic`); the anchor is a raw JPH kinematic body with NO entity, and the subsystem publishes `Get_DebugInternalBodyIds()` so the capture skips it — never drawn, never pickable, never listed. **S3** the External channel is **retained named sub-channels** (`Draw_External*(Name, …)` accumulates, `Clear_External(Name)` empties, flushed every capture without clearing) — this is what makes probe results, the grid and the drag line stable. **S6** ObjectLayer legend = `Layer N — <object-channel name>` via a read-only reverse lookup on the layer table (add a getter if needed), bare `Layer N` with no channel. **S7** the world-axis gizmo REUSES `SCkDebug_OrientationCube` in an overlay slot — no hand-drawn `OnPaint` gizmo (labels still need `OnPaint`). **S8** add public `Get_BodyA`/`Get_BodyB` on `UCk_Utils_JoltConstraint_UE` (executor picks the doctrine-conformant spot, STOPs if unclear); all-constraints reference-frame drawing while a constraint is selected is accepted. **S10** `GetBodyStats()`/constraint-count cadence = **N = 30 captures**, UI labels those fields "(sampled)". **S12** mechanical `CkJoltDebugger` call-site collateral is in scope for Phases 5–6, no back-compat shims. **S14** the three user reports are recorded verbatim, plus the follow-up scope ruling "do all of the above including mouse-drag" (runtime property editing, rewind and soft bodies stay excluded). **S4, S5, S9, S11, S13** stand exactly as the planner recorded them | Research turned five rulings into forks and two into "does not exist"; ruling them all in one pass keeps the executor from re-litigating mid-unit | a ruled fork proves wrong in the build |
| P8-D60 | 2026-08-15 | **Close-out** — module CLAUDE.md files updated (both), full serial gate + scoped gates, adversarial review per phase (fresh Opus drafts triage; orchestrator ratifies), commits local per phase, ship withheld (P0-D8/P4-D36 unchanged). | — | user returns and ships |

## Open items
| Item | Status | Next step |
|---|---|---|
| INV-A color mechanism | ✅ CLOSED — RULED → P1-D11, shipped | — |
| INV-B static-body zero-cost model | ✅ CLOSED — RULED → P1-D10, measured in P4/P8 | — |
| INV-C ISM registration in preview world | ✅ CLOSED — RULED → P1-D12 and PROVEN by `Ck.Jolt.DebugDraw.PreviewWorldCompat` | — |
| INV-D async-physics capture strategy | ✅ CLOSED — RULED → P1-D9, shipped | — |
| INV-E renderer rename/architecture | ✅ CLOSED — RULED → P1-D8, shipped | — |
| P1-D13 wireframe material source | ✅ CLOSED — branch (a) shipped; packaged risk tracked in its own row below | — |
| Baseline gate | ✅ CLOSED — 1146/1150, 4 named pre-existing fails; the final gate of record matched the set EXACTLY | — |
| Phases 5–8 planner STOP list (S1–S14) | ✅ CLOSED 2026-08-15 — all 14 ruled, see [P5-D61] | — |
| **`[EDITOR-VERIFY]` — user PIE pass (66 steps, section above) + the retained Phase-3/4 pointers** | **OPEN — awaits the user** | Run it before shipping; fix-forward anything red |
| **`[PACKAGED-VERIFY]` both debug materials (`M_SimpleUnlitTranslucent` + `WireframeMaterial`) load in a packaged Development build** | **OPEN** (pre-existing risk, named since P1-D13) | Ship step 4; on failure switch wireframe to P1-D13 branch (b) |
| **F6 second half — hover does not clear when the hovered body leaves the outliner set** | **OPEN — deliberate gap** ([P8-D74], skipped under the P5-D65 speed policy) | Documented as a Known gap in `CkJoltDebugger/CLAUDE.md`; the hover clears on the next mouse move. Close it if the user's PIE pass finds it annoying (step 21) |
| **ProbeTrace results population** | **OPEN — follow-up** | A persistent ProbeTrace draws lines to hit entities but no contact points (the fragments do not carry them); the tooltip explains it. Populating them is a CkSpatialQuery change, not a debugger one |
| **All-flags cost ≈ 290–306 ms at 100k** | **OPEN — perf follow-up** | The line channel re-pushes every line every capture. Retaining unchanged lines (or batching per flag) is the obvious lever; NOT chased — the default 1.73 ms path is the one that matters |
| **Island colour mode** | **OPEN — optional** | Dropped in P5-D66 because `GetIslandIndexInternal()` sits under Jolt's INTERNAL-USE banner. A vendored-patch option exists if the user wants it back |
| **Per-line contact-record lock** | **OPEN — perf item** ([P5-D64] F6) | The contact record takes the `FCriticalSection` per LINE, not per batch. Only costs while some target demands contacts; measure before optimizing |
| **The drag anchor inflates `BodyStats`** | **OPEN — documented, not fixed** | The kinematic anchor is a real JPH body, so the sampled body counts tick up by one while a drag is live. The capture skips it for drawing/picking/listing; only the stat rail sees it |
**Rule: no completion claim may be written anywhere in this file while any row here is unresolved.**
> The seven OPEN rows above are **user-gated or explicitly deferred**, not unfinished implementation
> work. No completion claim in this file rests on any of them.

## Planner STOP list — Phases 5–8 (orchestrator rulings needed; NOT resolved by the planner)

Each row is a place where the code makes a binding ruling infeasible, ambiguous, or riskier than it
reads. The planner wrote the phase contracts to the ruling **as issued** and recorded the fork here.

| # | Ruling | What the code says (evidence) | Fork options |
|---|---|---|---|
| S1 | **P5-D40** — "`FProcessor_JoltWorld_Step` wraps `PhysicsSystem::Update` in a `Begin/EndRecord()` scope" | `PhysicsSystem::Update` is **not** called in that processor. The step loop calls `JoltWorld->DoPhysicsUpdate(FixedDt)` (`CkJoltWorld_Processor.cpp:199`, lambda `:194-202`); `DoPhysicsUpdate` is declared `CkJoltWorld.h:158` and defined in `CkJoltWorld.cpp`. In **async** mode the whole loop runs on a TaskGraph thread (`Async(EAsyncExecution::TaskGraph, …)`, `:206-210`). Jolt's contact draw is emitted from inside the solve, which is multi-threaded when `jolt.EnableParallelPhysics` is on. **HIGH RISK: the record buffer would be appended from worker threads.** | (a) move the record scope into `FJoltWorld::DoPhysicsUpdate` and make the buffer thread-safe (lock-free per-thread chunks, or a critical section like `CkContactListener`'s `_QueueLock`, `CkJolt_Subsystem.cpp:213`); (b) record only in **sync** mode and document contacts as unavailable under async physics; (c) drop contact recording and rely on P6-D45's on-demand `CollideShape` for the selected body only |
| S2 | **P6-D47** — "verify a non-colliding object layer exists … if not, STOP-report which layer to use" | **It does not exist.** Object layers are allocated per unique `FCk_Jolt_CollisionSignature` (`CkJoltCollisionLayer_Data.h:40-98`) by `Build_FromCollisionProfiles` (`CkJoltCollisionLayerTable.cpp:20-65`), which **skips** every `NoCollision` profile (`:36-37`). Broadphase layers are only `Static`/`Dynamic` (`CkJoltCollisionLayerTable.h:78-83`). | **Planner's recommended recipe (needs ratification):** register a layer for a *default-constructed* signature — `_ResponseMask = 0` (`CkJoltCollisionLayer_Data.h:55`) makes `Get_PairInteraction` (`.cpp:134-152`) `Ignore` against everything, so `ObjectLayerPairFilter::ShouldCollide` (`.h:139-147`) is false for every pair, and channel queries never see it; set `_Domain = Dynamic` so `ObjectVsBroadPhaseLayerFilter` (`.h:113-127`) keeps it in the dynamic tree. Alternative: reuse the fixed probe layer (`Get_ProbeSignature`, `:99-110`) — rejected by the planner because it overlaps WorldDynamic and would generate sensor events |
| S3 | **P5-D38 / P8-D56 / P8-D59 / P7-D54** — the External channel is "cleared per capture" | CkJolt **cannot** read probe fragments: the dependency runs CkSpatialQuery → CkJolt (`CkProbe_Processor.cpp:1247` calls the Jolt world's `Request_NoteBodyRemoved`). So probe results, the grid and the drag line must be pushed **from the debugger's Slate tick**, whose rate is unrelated to the capture rate. A per-capture clear means anything pushed between captures is dropped and anything pushed before one is erased → flicker | (a) External channel **retained until its contributor clears it** (`Draw_External*` accumulates into a named sub-channel; `Clear_External(Name)` empties it) — planner's recommendation, one extra API, no flicker; (b) debugger re-pushes every Slate tick and accepts flicker; (c) grid only moves into the capture (CkJolt can draw a grid) and probe/drag stay flickery |
| S4 | **P5-D39** — `SleepStats` "if cheap; else drop" | Jolt draws sleep stats from `MotionProperties`' internal sleep-test spheres; the accessors sit below the `FOR INTERNAL USE ONLY` banner (`MotionProperties.h:189-191`) and are not otherwise exposed, and the capture deliberately does not call `DrawBodies`. **Planner ruled it DROPPED** per the ruling's own escape clause | none needed — recorded so it is not re-litigated |
| S5 | **P5-D41** — "Add a **Hover** colour class … see D43 for the mask" | D43 is the pause/step ruling. The mask widening is **P5-D42**. | Treat as a typo; the planner wrote PHASE_5 to take the mask from D42 and to land D42's widening **before** D41's Hover class (today `Count <= 8` is `static_assert`ed at `CkJolt_DebugDrawTarget.cpp:28-30`, and Hover would be the 9th) |
| S6 | **P5-D42** — ObjectLayer colour mode "ObjectLayer > 61 → Other" | Object layers are **not** an enum and have no names: up to 1024 dynamically registered signatures (`CkJoltCollisionLayerTable.h:28`). The numeric bucketing works; `Get_LegendEntries` has nothing to name them with | (a) legend shows `Layer N — <channel name from the signature's `_ObjectChannel`>` (needs a layer→signature reverse read on the table); (b) legend shows bare `Layer N`; (c) drop the ObjectLayer mode |
| S7 | **P8-D59** — "world-axis gizmo in the viewport's bottom-left in `OnPaint`" | `SCkDebug_OrientationCube` already exists as a shared 2D orientation gizmo (`CkDebuggerCommon/Public/CkDebuggerCommon/Widgets/SCkDebug_OrientationCube.h:34`, an `SLeafWidget`) | (a) keep the ruling and hand-draw in `OnPaint`; (b) drop the existing widget into an overlay slot over the viewport — cheaper, consistent with the suite, and needs no `OnPaint` for the gizmo (labels still do) |
| S8 | **P8-D55** — "if CkJolt exposes constraint entities/handles … else …" | **Branch (a) applies.** A full constraint feature exists: `ck::FFragment_JoltConstraint_Current` (`CkJoltConstraint_Fragment.h:41-67`), `FCk_Handle_JoltConstraint`, `UCk_Utils_JoltConstraint_UE` (`CkJoltConstraint_Utils.h:18`), four processors, plus `UCk_Utils_JoltRope_UE`. **But** `_BodyA`/`_BodyB` (`:55-56`) are private behind a friend list (`:26-29`) — a narrow public read API is required. Separately, Jolt's `DrawConstraintReferenceFrame` (`PhysicsSystem.h:159`) is **all-constraints**, so "draws its reference frames" cannot be per-constraint | (a) add `Get_BodyA`/`Get_BodyB` to the utils and accept all-constraints reference-frame drawing while a constraint is selected (planner's write-up); (b) drop the reference-frame half of the ruling |
| S9 | **P6-D48** — "if CkJolt has an existing ContactListener …" | **It does** — `class CkContactListener : public JPH::ContactListener` at `CkJolt_Subsystem.cpp:70` (`OnContactAdded :84-129`, `OnContactPersisted :131-175`), installed `:457-458`, running on Jolt worker threads under `_QueueLock` `:213`. So contact pairs are IN. Counter must be atomic | none needed — recorded |
| S10 | **P6-D48** — `GetBodyStats()` in the stats | `BodyManager.h:78` documents it as *"slow, iterates through all bodies"*; `PhysicsSystem::GetConstraints()` (`:109`) returns `Array<Ref<Constraint>>` **by value** (full copy + refcount per element). At the campaign's 100k bar neither can be per-frame | planner wrote a throttle (every Nth capture, constant N) into PHASE_6 item 4; orchestrator should confirm the cadence is acceptable for a live stat rail |
| S11 | **P5-D38** — `ULineBatchComponent` in an `FPreviewScene` world | **Zero precedent** — `rg` over all of `Plugins/` finds no `ULineBatchComponent` anywhere in the suite. Unverifiable headlessly. The named fallback IS proven in-suite: `SCkCrowdDebugger_3dViewport.cpp:518-547` overrides `Draw(const FSceneView*, FPrimitiveDrawInterface*)` and draws with `PDI->DrawLine` (`:917-920`) | ruling already names the fallback; recorded as the phase's top build-time risk |
| S12 | **Phase-5/6 scope wording** — "CkFoundation only" | Phase 5 changes public signatures the debugger consumes (`Set_ClassVisibility`, `Get_IsClassVisible`, `Get_BucketColorClasses`, `Get_Palette().Get_Color()` at `SCkJoltDebuggerWindow.cpp:1029-1068` and `:1099-1138`; `Get_HighlightedBodyLinearVelocity` in Phase 6). Both plugins compile into one editor target, and module doctrine bans back-compat shims | planner wrote an explicit **"mechanical call-site collateral only"** allowance into PHASE_5 and PHASE_6 Fences; orchestrator should confirm |
| S13 | **P8-D58** — labels in `OnPaint` | No `OnPaint` override and no world→screen projection exist anywhere in `CkGameplayDebugger`; the only view math is the inverse (`GetCursorWorldRay`, `SCkJoltDebugger_3dViewport.cpp:298-332`, via `FMinimalViewInfo::CalculateProjectionMatrixGivenView` + `FSceneView::DeprojectScreenToWorld`). Also `CkDebuggerCommon/CLAUDE.md:476-480` bans brush/font allocation in `OnPaint` | greenfield but feasible by building the forward matrices the same way; recorded as risk, no fork needed |
| S14 | **The three user reports** in the dated entry below | The planner never saw the user's words — they were reconstructed from the ruling set | orchestrator should overwrite the three bullets with the user's verbatim feedback |

### RULED — orchestrator, 2026-08-15 → decision-log entry **[P5-D61]**

**Every S-row is now resolved. Nothing on this list blocks Unit XI.**

| # | RULED |
|---|---|
| S1 | **Fork (a), refined.** Record scope goes **inside `FJoltWorld::DoPhysicsUpdate`** around `PhysicsSystem::Update`. Buffer = `FCriticalSection`-guarded append, **active only while some target's contact flags are on** (zero cost otherwise). **Do NOT replay in the Step processor** — the buffer is **double-buffered** and CONSUMED on the game thread by `FProcessor_JoltDebugDraw_Capture` (already after `WaitForAsync`), which replays into every registered target that wants contacts. **One-frame latency in async mode is ACCEPTED and documented.** |
| S2 | **Planner recipe RATIFIED.** Register a layer for a default-constructed `FCk_Jolt_CollisionSignature` (`_ResponseMask = 0`, `_Domain = Dynamic`), **lazily on first drag**. The anchor is a **raw JPH kinematic body with no JoltBody entity**; the subsystem exposes `Get_DebugInternalBodyIds()` (or equivalent) and the **capture SKIPS those bodies — never drawn, never pickable, never listed**. Drag visual = External line per P7-D54. |
| S3 | **Fork (a).** The External channel is **retained named sub-channels**: `Draw_External*(Name, …)` accumulates, `Clear_External(Name)` empties. Sub-channels are **flushed to the line batcher every capture WITHOUT being cleared**. |
| S4 | Recorded as the planner wrote it — `SleepStats` **DROPPED**. No change. |
| S5 | Recorded as the planner wrote it — "D43" in P5-D41 is a typo for **D42**; D42's mask widening lands before D41's Hover class. No change. |
| S6 | **Fork (a).** Legend reads **`Layer N — <object-channel name>`** via a read-only reverse lookup on the layer table (**add a getter if needed**); bare **`Layer N`** when a signature has no channel. |
| S7 | **Fork (b).** Reuse **`SCkDebug_OrientationCube`** in an overlay slot; **no hand-drawn gizmo**. Labels still need `OnPaint` — S13 stands as a risk. |
| S8 | **Fork (a).** Add public **`Get_BodyA` / `Get_BodyB`** read accessors on `UCk_Utils_JoltConstraint_UE` (or on the fragment via a friend-free getter — **the executor picks the doctrine-conformant spot and STOPs if unclear**). Reference frames are drawn **all-constraints while a constraint is selected** — ACCEPTED, documented. |
| S9 | Recorded as the planner wrote it — ContactListener exists, contact pairs are IN, counter is atomic. No change. |
| S10 | **Cadence RATIFIED: N = 30 captures (constant).** The UI labels those fields **"(sampled)"**. |
| S11 | Recorded as the planner wrote it — top build-time risk; D38's PDI fallback stands. No change. |
| S12 | **Allowance CONFIRMED.** Mechanical call-site collateral in `CkJoltDebugger` is **in scope for Phases 5–6**; **no shims**. |
| S13 | Recorded as the planner wrote it — `OnPaint` label projection is greenfield; risk accepted. No change. |
| S14 | **Resolved** — the three user reports are now recorded VERBATIM in the dated entry below, together with the follow-up scope ruling. |

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

### 2026-08-15 — CAMPAIGN CLOSED (phases 5–8): final fix-up landed; gate of record delta-zero; docs welded
- **Final fix-up landed** ([P8-D74]): F1–F7 (first half), F10, and P8-D73 (drag release targets the
  subsystem the drag began on, via a weak ref captured at arm). **F6's second half — clearing the
  hover when the hovered body leaves the outliner set — was SKIPPED** under the P5-D65 speed policy
  and is written up as a **Known gap** in `CkJoltDebugger/CLAUDE.md`; the hover clears on the next
  mouse move. **F8** (grid colour comment) and **F9** (per-capture `BodyIDVector` while armed) were
  accepted no-change, F9 as a documented cost.
- **GATE OF RECORD — orchestrator re-read the logs, serial, on the FINAL artifact.** Full suite
  `Saved/Logs/BuildTest_Final_Full.log` (17:55): **Total 1150 / Passed 1146 / Failed 4 /
  Contaminated 0**. The failing set is **exactly** the Phase-4 baseline set — `Crowd_NavQueryFilter_
  ForceReplan`, `Homing_Retarget_SwitchesPursuit`, `PathNetworkFollower_DesiredNavmeshClearance
  MovesInward`, `PathNetworkFollower_ProjectsRibbonWaypointWithinNavQueryExtent` — **zero new, zero
  gone**. Scoped: `Jolt` **98/98** (`Test_Final_Jolt.log`, 18:04), `DebuggerLauncher` **3/3**,
  `Probe` **27/27**.
- **Benchmark (100k default, measured — NOT a gate):** steady **1.73 ms**, revision **15.8**,
  highlight **16.8**, pick **12.5**, first pass **48.6**; all draw flags on ≈ **290–306 ms**
  (recorded as a follow-up, not chased — the default path is the one that matters).
- **COMMITTED LOCALLY, tips:** CkFoundation **`495214021`**, CkTests **`536d6013`**,
  CkGameplayDebugger **`6bde863`** (plus this docs close-out commit). All three are clean
  fast-forwards of `origin/dev`.
- **Docs welded:** both module CLAUDE.md files finalised; PHASE_7 / PHASE_8 status headers and exit
  criteria ticked with the evidence above; PLAN rows 7/8 ✅ and the Ship row 🟢 READY; PROGRESS's
  Current state, Ship instructions (whole campaign, one pass) and status board rewritten; the
  `[EDITOR-VERIFY]` backlog consolidated into **66 numbered steps in 10 groups** with the Phase-3/4
  pointers retained.
- **SHIP REMAINS WITHHELD (P0-D8 / P4-D36).** Nothing pushed, no superproject pointer bumps, no
  downstream bumps. The only work left in this campaign belongs to the user: the `[EDITOR-VERIFY]`
  pass, then the ship sequence.

### 2026-08-15 — Phase-8 final adversarial review (fresh Opus): 10 findings; triage RATIFIED [P8-D74]; final fix-up + gate of record dispatched
- **[P8-D74]** FIX-NOW: **F1** `DoApplyConstraintReferenceFrames()` on world change (old-world
  leak of the reference-frames flag); **F2** isolation gather folds `ConstraintBodyKeys` (isolate +
  constraint selection cleared isolation); **F3** `Select_NearestLabels` bounded selection (partial
  sort / max-heap of Cap) + fix the two "nothing allocates" claims; **F4** cache the Problems chip
  count in `Refresh()`; **F5** probe-results signature folds `Origin` + each `_OtherLocation` (frozen
  ProbeTrace lines); **F6** `MouseLeave` override clears hover; **F7** `~FScopedPreferences` calls
  `SaveConfig()` + covers `ShowProbeResults` (spec was writing the developer's real ini — must land
  before the gate); **F10** assert the fifth `Is_JoltDebuggerEntity` clause. Plus **P8-D73** (drag
  release targets the subsystem the drag began on — weak ref captured at arm). ACCEPT: **F8**
  (grid colour comment), **F9** (per-capture `BodyIDVector` while armed — documented cost).
- Gate of record after the fix-up (P5-D65): full serial suite (delta vs Phase-4 baseline set) +
  scoped `Jolt`, `JoltDebug`, `DebuggerLauncher`, `Probe`; benchmark numbers recorded.


### 2026-08-15 — Phase 8B + Phase-7 fix-up + TryPick_BodyHit DONE (Opus); orchestrator-verified (Jolt.DebugDraw 27/27 17:23, JoltDebug 15/15 17:24, DebuggerLauncher 3/3 17:25); COMMITTED (CkFoundation 495214021 / CkTests 536d6013 / CkGameplayDebugger 25e0c91); [P8-D73]; final review dispatched
- ALL implementation units of phases 5–8 are now committed. Remaining: final adversarial review →
  fix-up → gate of record (full serial suite vs Phase-4 baseline + scoped) → docs close-out.
- **[P8-D73]** ruling on 8B deviation (a): `HandleDragRelease` in `HandleWorldChanged` runs after
  the selector re-pointed, so it cannot `Request_EndDrag` on the OLD world (backstop = FJoltWorld
  Shutdown — insufficient for a live server↔client selector switch) → FIX in the final fix-up: the
  window keeps a weak ref to the subsystem the drag began on and ends the drag on THAT subsystem.
  Deviations (b) `OutlinerPrune` source + `Simulate_RowClick` seam, (c) `Net` glyph for the grid —
  ACCEPTED.


### 2026-08-15 — Phase 8A DONE (Opus); orchestrator-verified (Jolt.DebugDraw 27/27 16:56, JoltDebug 14/14 16:55, DebuggerLauncher 3/3 16:57, Probe 27/27 16:58); COMMITTED (CkFoundation ee073f2c7 / CkTests 0accfe9c / CkGameplayDebugger 5fff942); [P8-D72]; merged "8B + Phase-7 fix-up" executor dispatched
- Landed: constraint body accessors (`Get_BodyA/B/IsBodyBWorldAnchor` on utils); Constraint
  population (rows, predicate, both bodies highlighted + reference frames while selected); probe
  results retained channel `"JoltDebugger.ProbeResults"`; health scan (target thresholds,
  `Get_ProblemBodies`, O(active), off by default) + Problems chip + count badge; labels OnPaint
  (500 nearest, cached font); hover (≥60 ms, gesture-suppressed). Specs ListsConstraintRows,
  ProblemsChipNarrowsToFlaggedRows, LabelCapKeepsTheNearest, ProblemBodiesFlagTheBrokenOnes.
- **[P8-D72] accepted deviations:** Build.cs +CkEcsExt +CkLog (import libs don't propagate across
  the plugin boundary); new module log category; ProbeTrace branch fragment-guarded (no ProbeTrace
  population — follow-up); health scan O(active) misses a body that sank below KillZ then slept
  (documented); reference-frame toggle reads ON while a constraint is selected; NaN arms of the
  facility spec exercise the pure predicate (live bodies clamp NaN); label font cached at Construct.


### 2026-08-15 — Phase-7 adversarial review (fresh Opus): 12 findings; triage RATIFIED [P7-D71]; fix-up merged into the "8B + Phase-7 fix-up" executor (after 8A lands)
- **[P7-D71]** FIX-NOW: **F1** (HIGH — isolation set survives world change with dead keys → blank
  viewport) → `DoApplyIsolation()`/`Clear_Isolation()` in `HandleWorldChanged`; **F2** drag line
  channel orphaned on world change → `Clear_External(DragChannel)` there; **F3** `Request_EndDrag`
  on world change + destructor (before `_WorldModel` re-points); **F4** Ctrl+LMB on empty space
  arms a drag on the previous selection → arm carries the picked key (`TOptional<uint64>`), refuse
  when unset/≠ primary — folded into P7-D70(i) `TryPick_BodyHit` rework; **F5** ortho pan scale
  from `OrthoWidth`; **F6** wheel dolly moves eye AND look-at along forward (UE semantics) instead
  of shrinking the pivot distance; **F7** no `_PendingPickPress` when Alt is down; **F8** window
  re-derives `_SelectionAll` from `Get_SelectedAll()` on refresh (prune reaches highlight/isolate);
  **F9** multi-select spec drives `SetItemSelection(..., OnMouseClick)` (add / range / ctrl-remove
  primary); **F10** settings spec asserts the restore landed (window read-backs); **F11** authority
  tooltip bound on the IconToggle itself. ACCEPT: **F12a** cross-refs stay `CkJolt/Claude.md`
  (git-tracked spelling; Windows FS is case-insensitive); **F12b** RULING: Follow tracks the union
  of highlighted bounds (what the highlight shows) — P7-D53 amended.
- Sequencing: 8A is building CkGameplayDebugger now → the fix-up + P7-D70(i) + Phase-8 Unit C
  (grid/gizmo/bookmarks) go to ONE executor after 8A lands; then Phase-8 review, close-out gates.


### 2026-08-15 — Phase 7B DONE (Opus); orchestrator-verified (JoltDebug 11/11 `BuildTest_P7B_1.log` 16:20, DebuggerLauncher 3/3 16:22); COMMITTED (CkGameplayDebugger 4bdd694); [P7-D70]; Phase-7 review ∥ Phase 8A dispatched
- Landed: `FCkJoltDebugger_SelectionFacts`; detail groups + Contacts list; outliner multi-select
  (panel-owned store; SListView hands an arbitrary set element → primary derived from the delta);
  window selection set → highlight all / bounds union / contacts demand; Ctrl+click add
  (`ViewportAdditive` source); Isolate + `I`; Follow; drag UI (arm/ray/plane-shift/release,
  retained "JoltDebugger.Drag" line, authority gate, !UE_BUILD_SHIPPING).
- **[P7-D70] rulings on 7B deviations:** (i) drag grab-point via bounds centre on first mouse move
  is a WORKAROUND — NOT ACCEPTED as final: the Phase-7 fix-up adds facility
  `FCk_Jolt_DebugDrawTarget::TryPick_BodyHit(origin, dir, OutKey, OutHitPointWorld, OutDistance)`
  (oriented-box slab hit already computed by `TryPick_Body`) and the drag opens on press at the
  exact hit point (spec: hit point lies on the picked body's bounds surface ± tolerance);
  (ii) `ViewportAdditive` source that re-broadcasts but does not re-stamp the outliner — ACCEPTED;
  (iii) Isolate with empty selection clears isolation — ACCEPTED.
- Phase-7 adversarial review (fresh Opus, range CkGameplayDebugger 1e24bfe..4bdd694 debugger only)
  runs concurrently with Phase 8A (constraints population / probe results / health checks); the
  Phase-7 fix-up (+ P7-D70 i) runs after 8A lands (no edits during another lane's build).


### 2026-08-15 — Phase-6 fix-up re-gated after reboot: Jolt 91/91, JoltDebug 10/10 (orchestrator re-read `BuildTest_P6fix_Jolt.log` 15:50 / `_JoltDebug.log` 15:52); COMMITTED (CkFoundation 1df23d699 / CkTests 551fe5f0). Phase 6 DONE. 7B dispatched
- All 15 findings mapped DONE by the gate-runner (F1–F15 incl. accepted-doc items). Benchmark
  100k default steady 1.82 ms; all-flags 304 ms (unchanged class). Binaries post-dated sources
  (not stale-green). Stale BusterBlock toolbox locks (reboot casualty) auto-recovered by the build.


### 2026-08-15 — machine REBOOTED mid Phase-6 fix-up gate; fix-up edits survived on disk (uncommitted); re-gate dispatched
- State at 15:40: CkFoundation 14 dirty CkJolt files (F1–F15 fix-up), CkTests spec dirty; commits
  intact (CkFoundation 2bef8ef5c, CkTests ecacf8c5, CkGameplayDebugger 1300de4). Original gate
  (91 tests, in FrameCostMatrix) never produced a verdict → treated as NOT RUN.
- Fresh Opus gate-runner: maps diff → F1…F15, clears stale locks, runs whole `Jolt` + `JoltDebug`
  serially, reports verdicts. Nothing accepted until the log is re-read by the orchestrator.


### 2026-08-15 — Phase 7A DONE (Opus); orchestrator-verified; COMMITTED (CkGameplayDebugger 1300de4); Phase-6 fix-up dispatched (7B follows it)
- Landed: UE-editor camera scheme (`_LookAt`/`_OrbitDistance` invariant; look-in-place, RMB-gated
  flight, pan, dolly, track/yaw, Alt-orbit/dolly, ortho pan-only), spec
  `Viewport.CameraSchemeIsUnrealStyle`; settings fields (`IsolateActive`/`FollowSelection`/`ShowGrid`
  — no `b` prefix per house rule; `RunawayVelocityCmS`, `CameraBookmarks`, `DrawFlags`, `ColorMode`)
  + round-trip spec; `JoltDraw` lane (Bodies/Constraints/Contacts/Labels + colour mode; legend from
  `Get_LegendEntries`; population toggles BodyClass-only + re-applied on return); six in-world CVars;
  `JoltSim` lane Pause/Step + Space/Enter + PAUSED pill; Simulation stat section (sampled rows).
- Gate — **orchestrator re-read the run outputs**: `JoltDebug` 10/10 (14:51), `DebuggerLauncher`
  3/3 (14:53), exit 0 both. Executor notes the engine is UE 5.7 (FInputKeyEventArgs deprecations).
- **[P7-D69]** accepted deviations: no-`b` bool prefs; gesture state tracked in the client (headless
  drivability); `Set_ColorMode` re-applies the saved population mask on return to BodyClass.


### 2026-08-15 — Phase-6 adversarial review (fresh Opus): 15 findings; triage RATIFIED [P6-D68]; fix-up queued behind Phase 7A's build
- **[P6-D68]** FIX-NOW: **F1** (HIGH — PlanStep consumes the step-once gate before the engine-
  paused test → click silently lost) → evaluate `WorldBlocks` first; **F2** (HIGH — dragged body
  destroyed mid-drag → dangling constraint/UAF) → drop the empty-queue early-out; while dragging,
  `DoEnd_Drag()` when `TryGetBody(_DraggedBodyId)==nullptr`; **F3** (HIGH — `Query_SelectionContacts`
  uses accept-all filters → anchor/probes listed as contacts) → `DefaultBroadPhaseLayerFilter` +
  `DefaultObjectLayerFilter` for the selected body's layer AND skip `_InternalBodyKeys`; **F4**
  (step-once advances floor(acc/dt) steps) → granted frame bypasses `ComputeStepPlan`: exactly 1
  step of FixedDt, accumulator untouched; **F5/F6** (specs don't drive PlanStep/Step processors;
  step-duration measurement uncovered) → drive `FProcessor_JoltWorld_PlanStep::DoTick`, assert
  `Get_NumStepsLastFrame()==1`, engine-paused leg, `Get_LastStepDurationMs()>0` after real steps;
  **F7** (`_HighlightedBodyKeys.Contains` linear per body) → parallel `TSet`; **F8** (drag facility
  compiled into Shipping; `_BodyKey=0` aliasing) → wrap facility + processor + subsystem API in
  `#if !UE_BUILD_SHIPPING` consistently; **F9** (`Get_DragState` reads body transform off-thread) →
  cache grab point at `Apply_DragRequests`, getter returns the cache (do now — cheap, P7 consumes
  it); **F11** → reject keys with bits ≥ 32 (Verbose); **F12** → add legs: kinematic refused,
  second Begin while dragging, world Shutdown mid-drag; **F14** → fix comment, use
  `TryResolve_JoltWorld` precedent, dedupe `Conv_GroundState`; **F15** → `Excluded` counting.
  ACCEPT: **F10** (document anchor in BodyStats counts), **F13** (spec names as shipped; census
  updated in Claude.md — PHASE_6 census note added by orchestrator).
- Sequencing: fix-up touches CkJolt while Phase 7A may be building the editor → fix-up dispatched
  AFTER 7A reports (no source edits during another lane's build). Whole-`Jolt` Phase-6 gate runs
  with the fix-up.


### 2026-08-15 — Phase 6 executor B DONE (Opus); orchestrator-verified; COMMITTED (CkFoundation 2bef8ef5c / CkTests ecacf8c5); Phase-6 review + Phase 7A dispatched concurrently
- Landed: `FJoltWorld::Request_BeginDrag/UpdateDrag/EndDrag` + `FProcessor_JoltDebugDrag_Apply`
  (FGroup_Transform, after WaitForAsync+KinematicPush, before Step; CK_REGISTER_PROCESSOR); kinematic
  anchor on lazily registered non-colliding layer (S2 recipe); `Get_DebugInternalBodyKeys()`;
  capture + `TryPick_Body` skip internal keys; `FCk_Jolt_DebugDraw_WorldStats` (step ms, contact
  pairs via atomic sink on FJoltWorld from `CkContactListener`, active counts, BodyStats+constraints
  every 30 captures). Specs DragMovesDynamicBody, InternalBodiesAreInvisible, StatsSampled.
- Gate — **orchestrator re-read logs**: `Jolt.DebugDraw` 26/26 (BuildTest_P6B_DebugDraw.log
  14:29); `JoltDebug` 9/9 (BuildTest_P6B_JoltDebug.log 14:31). Whole-`Jolt` for Phase 6 runs at
  the Phase-6 fix-up (P5-D65).
- Phase-6 adversarial review (fresh Opus, read-only, range 4e8b1c5a6..2bef8ef5c) and Phase 7A
  (camera rewrite P7-D49, Draw lane P7-D50, sim controls + stats P7-D51; PHASE_7 items 1–12) run
  concurrently — different repos, review has no build.


### 2026-08-15 — Phase 6 executor A DONE (Opus); orchestrator-verified; COMMITTED (CkFoundation 1242ba703 / CkTests 756e7260 / CkGameplayDebugger d905760); executor B dispatched
- Landed: Island mode dropped (P5-D66); `Request_SetDebugPaused`/`Request_StepOnce` (consumed in
  PlanStep)/`LastStepDurationMs`; `FCk_Jolt_DebugDraw_BodySample`/`CharacterSample` (capture-owned);
  `Set_WantsSelectionContacts` + `CollideShape` contact entries; `Set_HighlightedBodies`/
  `Set_IsolatedBodies`/`Clear_Isolation`. Specs PauseAndStepOnce, BodySampleFields,
  SelectionContacts, MultiHighlightAndIsolate, SelectionSampleIsCaptureOwned.
- Gate — **orchestrator re-read logs**: `JoltDebug` 9/9 (BuildTest_P6A.log 13:51); `Jolt` 87/87
  (BuildTest_P6A_Jolt.log 14:02).
- **[P6-D67] P5-D65 correction:** the toolbox pattern is a substring match — `JoltDebug` matches only
  `Ck.JoltDebugger.*`. Per-unit facility gate = `--test-pattern Jolt.DebugDraw` (+ `JoltDebug` when
  debugger code moved); whole `Jolt` once per phase stays.
- Committed per-executor (kept clean file boundaries between concurrent-touching units).


### 2026-08-15 — Phase 5 fix-up landed (Opus); gate of record VERIFIED; Phase 5 COMMITTED (local); [P5-D66]
- F1–F5, F7(b)(c), F8–F10, P5-D63(v) DONE (per-world contact recorder keyed by `JPH::PhysicsSystem*`
  + atomic recording-world CAS; `Set_ColorMode` re-shows buckets; `Labels` gates text; MoveTemp
  replay; SleepState/legend-fallback spec legs; `ck.Jolt.DebugDraw.Contacts` CVar incl. gate-close
  flag reset and default-target replay via new `Get_DefaultDebugDrawTarget()`).
- Gate of record (serial, final artifact) — **orchestrator re-read logs**: `Jolt` 83/83
  (`Saved/Logs/BuildTest.log` 13:22), `DebuggerLauncher` 3/3 (BuildTest2 13:23), `Probe` 27/27
  (BuildTest3 13:24). Benchmark default 100k steady 1.89 ms; all-flags 331 ms (unchanged class).
- **[P5-D66] F7(a) STOP RULED: Island colour mode is INERT on vendored Jolt 5.2.1**
  (`SetIslandIndexInternal` has zero call sites; `IslandBuilder` never writes back — executor-
  verified). Ruling = **(a) DROP `Island` from `ECk_Jolt_DebugDrawColorMode`** (no dead API), executed
  as the first item of the Phase-6 executor A (avoids an extra gate cycle now). Follow-up recorded:
  option (c) — a one-line vendored patch calling `SetIslandIndexInternal` from `IslandBuilder` —
  if the user wants island colouring later (carries a third-party divergence across Jolt bumps).
- Committed locally (Phase 5): CkFoundation, CkTests, CkGameplayDebugger (collateral + docs). Ship
  withheld (P0-D8/P4-D36). Phase 6 next under P5-D65 speed policy.


### 2026-08-15 — [P5-D65] SPEED POLICY (user: "I am looking for speed at this point")
- Per-unit gate = `--test-pattern JoltDebug --parallel 1` (DebugDraw + JoltDebugger specs, no
  benchmarks). Whole-module `--test-pattern Jolt` (benchmarks incl.) ONCE per phase at the fix-up.
  Adversarial review ONCE per phase (fresh Opus, drafted triage, Fable ratifies) — no per-unit
  reviews. **Full serial suite ONCE, at the Phase-8 close** (delta vs the Phase-4 baseline set);
  Phase-6 full suite dropped. Units merged: Phase 6 = 2 executors (XIV+XV sim/inspection;
  XVI+XVII drag+stats), Phase 7 = 2 (camera+draw lane+sim controls; detail/selection/drag UI),
  Phase 8 = 2 (populations/probe/health; labels/hover/grid/gizmo/bookmarks) + close-out.
  Executors told to trust the contract and read only what they touch. Stale-green ban and
  orchestrator log re-read unchanged. Baseline of record for the final full suite stays the
  Phase-4 snapshot (`C:\Users\sulfu\.claude\reports\baseline_20260814-214144.md`, 1146/1150 named set).


### 2026-08-15 — Phase-5 adversarial review (fresh Opus): 10 findings; triage RATIFIED [P5-D64]; fix-up dispatched
- **[P5-D64]** FIX-NOW: **F1** (HIGH — `DrawLine` bound-target branch unlocked while workers may
  draw during a concurrent world's async solve) → route to the recorder whenever recording is
  active (atomic test first), never by `_ActiveTarget == nullptr` alone. **F2** (HIGH —
  `Set_ColorMode` clears the hidden mask but surviving ISMs stay `SetVisibility(false)`) → re-apply
  visibility on every bucket. **F3** (MED — one process-wide record buffer for N Jolt worlds) →
  RULING: per-world record buffers (keyed by `FJoltWorld`/`PhysicsSystem*`), one global atomic
  "current recording world"; a second world's `Begin` while another is recording SKIPS recording for
  that step (Verbose, once) rather than mixing; the capture processor replays only its own world's
  buffer into that world's targets. **F4** (dead `Labels` flag) → gate `DrawText3D` on `Labels`.
  **F5** → `MoveTemp` into the last demanding target / shared ref (cheap, do now). **F7** (spec
  gaps) → Island leg steps the fixture and asserts ≥2 distinct non-zero islands; SleepState
  rebucket leg; layer-name refresh driven through a fixture with a layer context (or, if no headless
  layer context exists, assert the bare-`Layer N` fallback of F8). **F8** → bare `Layer N` legend
  rows for present indices when no names were published (do now, cheap). **F9** → `Excluded`
  counting + agree the two paths. **F10** docs (acquire not relaxed; "demanding" not "registered";
  close D63(vi) as NOT-A-DEFECT — `MotionProperties.h` is used; `Claude.md` filename stays, cross-
  refs fixed is the normalization). Plus **P5-D63(v)**: add `ck.Jolt.DebugDraw.Contacts` CVar
  (points+normals) mapped onto the in-world target's flags. DEFER: **F6** (per-line lock during
  the parallel solve) → open perf item, measure before redesign (thread-local batching candidate).
- Reviewer CLEAN list recorded in the transcript summary: lifetime/teardown, statics reset, overlay
  scale in COM space, assert-safety, in-world parity incl. entity-less bodies, namespaces, doctrine.


### 2026-08-15 — Phase 5 Unit XII (colour modes + highlight/hover + contact recording) DONE (Opus); orchestrator-verified; Unit XIII review dispatched
- Landed (uncommitted): `ECk_Jolt_DebugDrawColorMode` (BodyClass/SleepState/ObjectLayer/Island/
  ShapeType), uint8 class index + uint64 mask (Highlight=62, Hover=63, MaxColorClasses 64),
  `Get_LegendEntries` (S6 layer names via existing `Get_NumLayers`/`Get_Signature`), highlight
  magenta/opaque/1.03×/sort+1, Hover class 0.5α/1.02×, contact recorder (union → JPH statics; lock-
  guarded double buffer inside `FJoltWorld::DoPhysicsUpdate`; replay by the capture processor; 200k
  cap), `SleepColoring` CVar → in-world SleepState mode (P5-D62 ii). Specs `ColorModesAndLegend`,
  `HoverOverlay`, `ContactRecordingReplays`; `HighlightAddsOverlayInstance` extended (scale≠1,
  colour≠every palette). Collateral: `SCkJoltDebuggerWindow.cpp` 4 call sites (index API).
- Gate (executor, serial, final artifact): `--test-pattern Jolt` **83/83**, exit 0 —
  **orchestrator re-read `Saved/Logs/BuildTest2.log` (12:33): Total 83 / Passed 83 / Failed 0,
  zero `Result={Fail`.**
- Benchmark 100k: default flags first 56.4 / steady 1.82 / revision 16.9 / pick 13.1 / highlight
  17.6 ms; **all flags: 330 / 308 / 318 / 13.4 / 322 ms** (lines clear every capture → the
  incremental pass cannot skip inactive bodies' extras). Measure-only.
- **[P5-D63] rulings on Unit XII deviations/findings:** (i) ObjectLayer catch-all = index 61
  "Layer 61+" ACCEPTED (no 63rd index exists); (ii) `Get_BucketColorClasses` live-only ACCEPTED;
  (iii) `Set_ColorMode` clears hidden mask ACCEPTED; (iv) all-flags 308 ms at 100k ACCEPTED as
  measured — follow-up recorded (retain inactive-body extra lines across captures, rebuild on
  revision) for Phase 8 polish if the user asks; (v) **in-world has no contact CVar → ADD
  `ck.Jolt.DebugDraw.Contacts` (points+normals) in the Unit XIII fix-up** — the user's report
  expects contacts in-world; (vi) stray `MotionType.h` include — remove in fix-up.
- `[EDITOR-VERIFY]` (Phase 5, cumulative): contact points visible in preview with the flag on and
  gone everywhere when off (process-wide); highlight unmistakable vs sleeping body + sensor;
  `SleepColoring` CVar recolours in-world; colour modes + legends read correctly in preview.


### 2026-08-15 — Phase 5 Unit XI (channels + flags + in-world re-host) DONE (Opus); orchestrator-verified; Unit XII dispatched
- Landed (uncommitted, CkFoundation 8 files / CkTests 1 / CkGameplayDebugger CLAUDE.md only): line +
  label + retained named External channels on `FCk_Jolt_DebugDrawTarget` (owned `ULineBatchComponent`);
  renderer `DrawLine/DrawTriangle/DrawText3D` → active target channels; `ECk_Jolt_DebugDrawFlags`
  (SleepStats dropped, S4); `Draw_BodyExtras` (velocity/angular arrows, world+COM axes, AABB,
  mass+inertia) via `…Unchecked()` accessors; constraint draws under flags; in-world Tick re-hosted
  onto `Capture_JoltWorld` (`DrawSettings`/`DrawBodies` block + `BeginFrame/EndFrame` deleted);
  specs `Ck.Jolt.DebugDraw.LineAndLabelChannels`, `DrawFlagsGatePerBodyExtras`; docs defects fixed.
- Gate (executor, serial, final artifact): `--test-pattern Jolt` **80/80**, exit 0 —
  **orchestrator re-read `Saved/Logs/BuildTest2.log` (11:48): Total 80 / Passed 80 / Failed 0.**
  Benchmark 100k within noise of Phase-4 numbers (steady 2.66 ms).
- **[P5-D62] In-world re-host deltas RULED ACCEPTED:** (i) in-world body colours now come from the
  facility palette (was JPH `sGetDistinctColor` per motion type) — consistency with the preview is
  the point; (ii) `ck.Jolt.DebugDraw.SleepColoring` INERT until D42 lands in Unit XII — Unit XII
  MUST wire it to the SleepState colour mode of the in-world target; (iii) `Velocity` CVar maps to
  Velocity|AngularVelocity and axis/arrow sizes are ×100 (uu) — old sizes were sub-pixel.
  Naming deviation `ECk_Jolt_DebugDrawFlags` (enum prefix) accepted.
- Open `[EDITOR-VERIFY]` added (Phase 5): (1) in-world CVars: Enabled/Velocity/Constraints/
  WorldTransform show; (2) Opacity live; (3) **a facility line renders in the preview viewport**
  (`ULineBatchComponent` in `FPreviewScene` — headless-unverifiable, S11); (4) sleeping bodies still
  distinguishable in-world.
- Routing: executor Opus (fresh); orchestrator verified the log; Unit XII dispatched to fresh Opus.


### 2026-08-15 — Phase 5–8 planning (Opus planner) — scope from user PIE feedback
- Phases 1–4 shipped a **shape viewer**. The user's live PIE pass says it has to become a **physics
  debugger**. The campaign is extended by four phases; ship moves from 🟢 READY to 🟡 withheld.
- **The three user reports — VERBATIM** (supplied by the orchestrator 2026-08-15; these are the
  user's own words and supersede the planner's earlier paraphrase):
  1. *"I noticed that the in-world debug has more information than the jolt debugger. I can see
     velocities, normals, contact points. The jolt debugger should have all those and they should
     all be toggleable."* → P5-D38, P5-D39, P5-D40, P7-D50.
  2. *"I also noticed that if I have something selected in the outliner, it does not change the
     color of the body to show me what is selected."* → P5-D41.
  3. *"Also, why aren't the camera controls similar to the Unreal Editor which is first person
     controls where right-click moves the camera similar to an FPS instead of pivoting around the
     focus."* → P7-D49.
- **Follow-up scope ruling (user, verbatim):** *"I would like you to do all of the above including
  mouse-drag."* — i.e. all 12 extras listed below **plus** mouse-drag (P6-D47/P7-D54). Runtime
  property editing, rewind/record-replay and soft bodies remain **excluded**.
- **The 13 extra items** scoped on top of the three reports:
  1. Colour-by modes — sleep state / object layer / island / shape type, not just body class (P5-D42).
  2. Pause and single-step the simulation (P6-D43, P7-D51).
  3. A real detail panel: mass, friction, restitution, gravity factor, motion quality, layers,
     shape type/scale, island, AABB, user data; character ground state (P6-D44, P7-D52).
  4. "What is this body touching" — contacts of the selection (P6-D45, P7-D52).
  5. Multi-select (P6-D46, P7-D53).
  6. Isolate — hide everything but the selection (P6-D46, P7-D53).
  7. Follow-selection camera (P7-D53).
  8. Mouse-drag bodies to poke the simulation (P6-D47, P7-D54).
  9. Extended world stats: step duration, body stats, active counts, constraint count (P6-D48).
  10. A Constraints population in the outliner (P8-D55).
  11. Probe overlap/hit results drawn — today only the sensor shape is (P8-D56).
  12. Health checks: NaN, runaway velocity, below KillZ, degenerate shape, with a "Problems"
      filter chip and a header badge (P8-D57).
  13. Orientation aids: world-space labels + hover highlight, ground grid, world-axis gizmo,
      camera bookmarks (P8-D58, P8-D59).
- **Rulings recorded verbatim: P5-D38…P5-D42, P6-D43…P6-D48, P7-D49…P7-D54, P8-D55…P8-D60.**
  None were re-litigated by the planner; every point where the code makes a ruling infeasible or
  ambiguous is carried in the planner's STOP list (returned to the orchestrator) and in the
  "Planner STOP list" section below, NOT resolved unilaterally.
- Docs authored: `PHASE_5.md`, `PHASE_6.md`, `PHASE_7.md`, `PHASE_8.md`, `PHASE_5_DISPATCH.md`
  (Units XI–XXI); `PLAN.md` rows 5–8 + phase summaries; this file.

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
| 2026-08-15 | Fable 5 (orchestrator); Opus (planner) | Campaign extended by phases 5–8 after the user's PIE pass: rulings P5-D38…P8-D60 issued by the orchestrator, four phase contracts + dispatch plan authored by an Opus planner (research-only, no code, no builds, no commits) | 1× Opus planner + 4× Opus Explore research agents (facility, debugger module, vendored Jolt API, probes/specs/docs) |
| 2026-08-15 | Fable 5 | Phase 1 committed; Phase 2 executed end-to-end: facility extensions, viewport shell, review (10 findings) fixed, rulings P2-D19…D21, docs weld, gate of record green | standing Opus executor (Unit V), fresh Opus executor (Units VI/VII + docs), 1× Opus adversarial reviewer; gates run by orchestrator |
| 2026-08-15 | Fable 5 | **Phases 5–8 executed end-to-end and the campaign CLOSED**: draw channels/flags/colour modes, sim control + inspection + drag facility, Unreal-scheme camera + command lanes + detail + drag UI, constraints/probes/health/labels/grid/gizmo/bookmarks; 4 review rounds (47 findings triaged, rulings P5-D64…P8-D74), final fix-up, gate of record delta-zero on the final artifact, both module CLAUDE.md files welded, all commits LOCAL; **ship withheld** | 1× Opus planner, 9× Opus executors (P5 units XI/XII/fix-up, P6 A/B/fix-up, P7A, P7B, 8A, 8B+P7 fix-up, final fix-up), 4× Opus adversarial reviewers (each drafted its own triage; orchestrator ratified), 1× Opus gate-runner; final gate re-read by the orchestrator |
