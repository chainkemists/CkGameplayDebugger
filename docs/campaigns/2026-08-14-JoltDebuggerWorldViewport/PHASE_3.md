# Phase 3 — Outliner + selection + picking + detail (CkJoltDebugger, + CkJolt read APIs)

> **Status:** 🟡 In progress (entered 2026-08-15)
> **Depends on:** Phase 2 ✅ (CkGameplayDebugger dev @ c1cbd5f, CkFoundation dev @ 5b3ce3eb3, CkTests dev @ d039513b)
> **Estimate:** 1 session

## Goal

After this phase: the Jolt debugger has an outliner listing every body-backing entity in the
selected world; selecting a row highlights that body in the preview viewport and enables
Frame Selection (button + `F`); clicking a body in the viewport selects its row; selection
round-trips through `ck::DebugSelectionSync` and an `FCkDebug_EntityTargetRoute` (ECS-debugger
"Open In" reaches it; "Sync from ECS" works); the shared game-viewport picker runs with a
Jolt `TargetFilter`; a detail panel shows the selected body's facts.

## Entry criteria — VERIFIED 2026-08-15

- [x] Phase-2 exit re-verified: full suite 1146/1150 == baseline set; scoped 67/67; commits landed.
- [x] Baseline for Phase 3 = those numbers.

## Design rulings (orchestrator, binding)

| ID | Ruling |
|---|---|
| P3-D22 | **Selection highlight lives in the facility (CkJolt), not the debugger.** `FCk_Jolt_DebugDrawTarget::Set_HighlightedBody(TOptional<uint64 BodyKey>)` (JPH-free: the key is `BodyID.GetIndexAndSequenceNumber()` as uint64, plus the character-key convention already used internally) — implemented as a per-target overlay: the highlighted body's instance is drawn in a dedicated "Highlight" color-class bucket (bright, opaque, on top of its normal instance is acceptable v1: keep the normal instance and ADD one highlight instance — no slot surgery). `Get_HighlightedBodyBounds() -> TOptional<FBox>` for Frame Selection. Facility spec required. |
| P3-D23 | **Outliner rows = flat snapshots collected in the debugger** (Crowd/Intent DataCollector pattern): `Data/CkJoltDebugger_Types.h` (`FCkJoltDebugger_BodySnapshot`: `FCk_Handle Handle` (the ONLY handle-bearing field), `uint64 BodyKey`, population enum, motion type, sleep state, `FString/FName DisplayName` (via `UCk_Utils_Handle_UE::Get_DebugName` + `CkDebug_NameClean_Utils`), body count for JoltStaticActor rows) + `Data/CkJoltDebugger_DataCollector.{h,cpp}` `Collect(UWorld*)` walking `FFragment_JoltBody_Current` / `FFragment_JoltStaticActor_Current` / `FFragment_Probe_Current` (CkSpatialQuery — add the dep) / `FFragment_JoltCharacter_Current` via `Get_TransientEntity(World).View<>()`; rate-gated by the window's refresh gate; SListView items keep `TSharedPtr` identity (mutate in place, `RequestListRefresh` only on set change — CkDebuggerCommon contract). Body key for JoltBody = `Get_BodyId()`; for Probe = its `_BodyId`; for JoltStaticActor rows = first of `_BodyIds` for framing/highlight (v1: highlight the FIRST body; note it); Character = the internal character key convention → expose `ck::jolt::debug_draw::Make_CharacterKey(entity)` or equivalent JPH-free helper on the facility. |
| P3-D24 | **Viewport click-pick = pure Slate/ISM raycast in the PREVIEW world**, no PhysicsSystem: deproject the cursor (Crowd's `GetCursorWorldRay`, restore it), `LineTraceSingle` against the preview world's ISM components is NOT available (no physics scene) → instead iterate the target's buckets: JPH-free facility API `TryPick_Body(FVector Origin, FVector Dir) -> TOptional<uint64 BodyKey>` doing a ray-vs-instance-bounds test over live instances (per-instance `GetInstanceTransform` + mesh bounds; nearest hit wins). O(instances) per CLICK, not per frame — acceptable v1; note for Phase 4. Facility spec required. |
| P3-D25 | **Selection model** = the window's ViewModel-lite: `TOptional<FCkJoltDebugger_BodySnapshot>`-keyed by BodyKey + Handle; single-select. Sources: outliner row click, viewport click, global `DebugSelectionSync` (lineage match via `Resolve_ClosestLineageMatch`), `EntityTargetRoute`, game-viewport picker. Sinks: `Set_HighlightedBody`, outliner `SetSelection(..., ESelectInfo::Direct)` under `FApplyGuard`, `DebugSelectionSync::Broadcast` (only for user-originated selection — echo suppression), detail panel, `_SelectionBounds` in the viewport (Frame Selection returns; `F` hotkey returns). |
| P3-D26 | **`Is_JoltDebuggerEntity` predicate** (static on the window, shared by route + picker filter): `Has<FFragment_JoltBody_Current> || Has<FFragment_JoltStaticActor_Current> || Has<FFragment_Probe_Current> || Has<FFragment_JoltCharacter_Current>`. Route registered after the spawner / unregistered before it (module). |
| P3-D27 | Detail panel = `SCkDebug_KeyValueRow`s: population, motion type, sleep state, body key, linear velocity (via `UCk_Utils_JoltBody_UE::Get_LinearVelocity` for JoltBody only), source actor name (StaticActor), body count; `SCkDebug_EntityRef` for the handle. Attribute-bound, built once. |

## Work items

1. **Facility (CkJolt):** `Set_HighlightedBody` / `Get_HighlightedBodyBounds` / `TryPick_Body` /
   character-key helper + specs (`Ck.Jolt.DebugDraw.Highlight`, `...Pick`). → verify: scoped
   Jolt green.
2. **Data:** `Data/CkJoltDebugger_Types.h` + `Data/CkJoltDebugger_DataCollector.{h,cpp}`
   (Build.cs: + `CkSpatialQuery`, `CkEcsExt` if needed). → verify: build.
3. **Outliner:** `Window/SCkJoltDebugger_OutlinerPanel.{h,cpp}` — `SListView<TSharedPtr<Snapshot>>`,
   `SCkDebug_DualSearchBar`, population column/pill, `Rebuild_ForStyleChange`, pointer-identity
   contract; layout = outliner LEFT rail (0.22) / viewport (0.53) / stats+detail RIGHT rail (0.25).
4. **Selection plumbing:** window selection model per P3-D25/26; module `FCkDebug_EntityTargetRoute`;
   game-viewport picker (`SCkDebug_ViewportPickerControls` in the `JoltTarget` group, `TargetFilter`
   = the predicate); viewport click-pick via P3-D24; Frame Selection + `F` restored; highlight sink.
5. **Detail panel:** `Window/SCkJoltDebugger_DetailPanel.{h,cpp}` per P3-D27.
6. **Lifecycle:** selection + snapshots cleared on session-invalidated (single path, as Phase 2).
7. **Specs:** outliner-construct, detail-construct; a data-collector spec if a headless world can
   host a JoltBody entity (check CkTests for an existing JoltBody headless fixture; if none, note).
8. **Docs weld:** `CkJoltDebugger/CLAUDE.md` (outliner/selection/route/picker sections, FrameSelection
   returns), PLAN row, this header, PROGRESS.

## Fences

- Never read `PhysicsSystem` from the debugger; picking is via the facility's JPH-free API.
- Snapshot is the only handle-bearing struct; every panel row holds plain values.
- No `SCheckBox`; no Slate rebuild on Tick; row `Text` attribute-bound, not frozen.
- Route + picker filter share ONE predicate; an open-only route is invalid.

## `[EDITOR-VERIFY]`
1. Outliner lists JoltBodies/StaticActors/Probes/Characters with clean names; search filters.
2. Row click → body highlighted in the viewport; Frame Selection button + `F` frame it.
3. Viewport click on a body → its row selects (and highlights).
4. ECS debugger → a Jolt entity → "Open In → Jolt" lands on the row; "Sync from ECS" works.
5. Game-viewport picker: only Jolt entities (+ owner chain) previewed/pickable.
6. Detail panel updates live for the selection (velocity for dynamic bodies).
7. PIE Stop → selection clears, no crash; re-PIE → outliner repopulates.

## Exit criteria — ALL in the same commit as the last work item
- [ ] Scoped Jolt + JoltDebugger serial suite green (orchestrator); full suite == baseline set
- [ ] Launcher census untouched-green
- [ ] `[EDITOR-VERIFY]` delivered; PLAN/PROGRESS/module CLAUDE.md updated
