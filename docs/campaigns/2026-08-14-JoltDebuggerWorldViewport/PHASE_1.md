# Phase 1 — World-targetable batched Jolt debug renderer (CkFoundation/CkJolt)

> **Status:** ✅ Done 2026-08-15 (code uncommitted, pending user commit approval)
> **Depends on:** campaign start (no prior phase)
> **Estimate:** 1–2 sessions — actual: 1 session (2026-08-14 → 15, orchestrator Fable, executor Opus ×4 units + 1 review)

## Goal

After this phase: CkJolt exposes an **instantiable, world-targetable** batched debug renderer that
a consumer can point at ANY `UWorld` (including an `FPreviewScene` world), pump with body data from
a `JPH::PhysicsSystem`, and switch between solid and wireframe materials with per-body-class colors
— while the existing subsystem-driven in-world debug draw behaves exactly as before.

## Entry criteria (pre-flight — run, don't assume)

- [ ] `/baseline` captured: toolbox full-suite counts + failing names on current CkFoundation dev
      HEAD (hash recorded in PROGRESS.md); `git -C Plugins/CkFoundation status` dirt enumerated
      (CkUsf GeneratedLooks churn is a known other-workstream artifact — do not stage).
- [ ] Read `CkJolt/Claude.md` §"Debug draw + stats", §"Anti-patterns", §"Threading model" on
      current HEAD.
- [ ] Design rulings below ([INV-A]…[INV-D]) recorded in PROGRESS.md **before** implementation
      units dispatch.

## Design rulings — RULED 2026-08-14, recorded in PROGRESS.md (P1-D8…D13)

Summary (PROGRESS.md decision log is authoritative): **P1-D8** front-end/target split — one
`FCk_Jolt_DebugRenderer` (JPH enforces a single instance: `DebugRenderer.cpp:75` asserts) owning
the geometry/batch cache; per-world `FCk_Jolt_DebugDrawTarget` owns ISM buckets/MIDs/mode/palette.
**P1-D9** capture runs as a CkJolt processor in the async-safe window (after
`WaitForAsync`+`SleepStateMirror`, before `Step`), pumping registered+demanding targets; the
debugger never touches `PhysicsSystem`. **P1-D10** two-pass capture: static bodies once per
static-revision; per-frame = active bodies + sleep/activation transitions; persistent body→slot
maps in buckets. **P1-D11** per-(geometry, color-class) buckets, palette ≤ ~10 classes; no
per-instance custom data in v1. **P1-D12** ISM registration keeps the existing
`Request_CreateNewObject` + `RegisterComponentWithWorld(TargetWorld)` path — the preview-world
spec must prove it before Phase 2. **P1-D13** wireframe material source = bounded investigation
(engine material vs generated content asset), decision returns to the orchestrator.

## Work items (each `step → verify:`; executors STOP on any unenumerated observation)

1. **Refactor to front-end/target split (P1-D8) + rename** — extract per-world state
   (`FBucket` map, opacity, `_AnyLive`, ISM ownership — `CkJolt_DebugRenderer.cpp:144-258`) into
   `FCk_Jolt_DebugDrawTarget`; rename class `CkJoltDebugger` → `FCk_Jolt_DebugRenderer`; move
   singleton ownership to a module-level holder with `OnEnginePreExit` teardown; subsystem's
   in-world draw becomes the default target with IDENTICAL behavior (same CVars, gate, opacity).
   → verify: CkJolt + all consumers compile; existing Jolt tests green.
2. **Capture pipeline processor (P1-D9) + two-pass model (P1-D10)** — target
   registration/demand API on the subsystem; capture processor in `FGroup_Transform` with explicit
   `RunAfter FProcessor_JoltWorld_WaitForAsync` (module rule) drawing into each demanding target:
   static pass keyed to a static-revision counter, per-frame active-body pass, sleep/activation
   recolor. Characters drawn from `FFragment_JoltCharacter_Current` capsule params.
   → verify: headless spec — synthetic bodies, two pumps, second pump reconciles no-ops for
   static; sleep transition moves instance between buckets.
3. **Body-class palette (P1-D11)** — color-class classification (motion type tags/body state,
   `IsSensor()`, character, baked-static) + sleeping dim; palette struct on the target.
   → verify: spec feeds one body per class, asserts expected bucket/color assignment.
4. **Wireframe/solid mode (P1-D13 investigation first)** — investigate material source branches
   (a) engine wireframe-capable material available in packaged builds, (b) generated content asset
   (CkUsf GeneratedLooks precedent); REPORT findings to orchestrator before wiring; then
   `Set_RenderMode(Solid|Wireframe)` on the target swaps per-bucket MIDs, zero geometry rebuild.
   → verify: spec asserts material swap flips bucket materials, instance counts stable.
5. **Preview-world compatibility spec (P1-D12)** — spec creates a minimal non-game world, target
   bound to it, captures synthetic geometry, asserts ISMs exist/registered/instances correct.
   → verify: the spec itself; failure = STOP, bounce to orchestrator with verbatim output.
6. **Docs weld** — `CkJolt/Claude.md` debug-draw section rewritten for the facility; PLAN.md row +
   this Status header; PROGRESS.md dated entry. Same commit as the last work item.

## Expected observations at the gate — branches

| I will run | I expect | If instead | Response |
|---|---|---|---|
| Toolbox full suite (`--discover-fresh`) | delta-zero vs baseline | new reds in Jolt/SpatialQuery patterns | STOP; restore known-good; diagnose before re-applying |
| New renderer specs | green, counts match spec asserts | ISMs absent in minimal world | INV-C ruling was wrong — bounce to orchestrator, re-rule with the observed registration failure verbatim |
| PIE with `ck.Jolt.DebugDraw.Enabled 1` (deferred to Phase 2 `[EDITOR-VERIFY]`) | identical visuals to pre-campaign | tint/opacity/coverage drift | treat as regression vs criterion 6; bisect the refactor commit |

## Fences (known wrong paths)

- Do NOT create a second `JPH::PhysicsSystem`, call `PhysicsSystem::Update`, or resolve entities
  inside Jolt callbacks (`CkJolt/Claude.md` anti-patterns).
- Do NOT touch `GetTriangles*` on wrapped shapes without the `RotatedTranslatedShape` unwrap
  (`CkSpatialQuery/CLAUDE.md`); prefer the existing `CreateTriangleBatch`/`DrawGeometry` route.
- No side effects in `CK_ENSURE` expressions; no anonymous namespaces (unity builds); trailing
  return types; `_Member` + `CK_PROPERTY`.
- No Script/ or source edits while a gate run is in flight; `--parallel 1` for trustworthy gates.

## Exit criteria — ALL in the same commit as the last work item

- [ ] Every expected observation confirmed; evidence (counts, spec names) in PROGRESS.md
- [ ] Existing subsystem debug-draw path verified unchanged (build-level; PIE visual deferred to
      Phase 2 `[EDITOR-VERIFY]`, listed there)
- [ ] Full toolbox gate green, delta-zero vs baseline, re-run by the orchestrator (not trusted
      from an executor)
- [ ] PLAN.md row + this Status header updated; `CkJolt/Claude.md` updated
- [ ] PROGRESS.md dated entry with confirmed/inferred split
