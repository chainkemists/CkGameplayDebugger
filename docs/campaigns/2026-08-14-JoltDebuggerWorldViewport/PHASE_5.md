# Phase 5 — Draw channels, per-target draw flags, contact recording, colour modes (CkJolt)

> **Status:** ✅ Done 2026-08-15 (committed locally; ship withheld; `[EDITOR-VERIFY]` pending; Island colour mode dropped in Phase 6 per P5-D66)
> **Depends on:** Phase 4 ✅ (CkFoundation `dev` @ 2d0f71ced, CkTests `dev` @ 59e3d1d6,
> CkGameplayDebugger `dev` @ 66f1e75 — all local)
> **Scope (repo):** `Plugins/CkFoundation/Source/CkJolt/` + `Plugins/CkTests/.../UnitTests/CkJolt/`.
> Files reached by the P5-D61 rulings beyond the debug-draw set: `World/CkJoltWorld.{h,cpp}`
> (S1 record scope inside `DoPhysicsUpdate`), `Subsystem/CkJoltDebugDraw_Processor.{h,cpp}`
> (S1 game-thread consumption), `CollisionLayer/CkJoltCollisionLayerTable.{h,cpp}`
> (S6 read-only reverse lookup).
> **Allowed collateral (P5-D61/S12, CONFIRMED):** the *mechanical* `CkJoltDebugger` call-site
> updates required to keep the build green when a public facility signature changes (see Fences).
> No new debugger behaviour — that is Phase 7. **No back-compat shims.**
> **Estimate:** 1–2 sessions (2 implementation units + review)

## Goal

After this phase the facility can draw more than shapes. `FCk_Jolt_DebugDrawTarget` owns a line
channel, a label channel and an "External" channel; `FCk_Jolt_DebugRenderer`'s immediate-mode
primitives route into the active target instead of straight into a `UWorld`; a per-target
`FCk_Jolt_DebugDrawFlags` bitmask decides which of Jolt's debug-draw vocabulary the capture emits;
contacts — which only exist during the solve — are recorded around the step and replayed into every
demanding target; the selection is unmistakable and gains a Hover sibling; and colour is a *mode*
(BodyClass / SleepState / ObjectLayer / Island / ShapeType) with a legend API the debugger can
follow. **The existing in-world draw is re-hosted onto the same flags with its behaviour preserved.**

## Entry criteria — verify before writing code

- [ ] Phase-4 exit re-verified: full serial suite == baseline set; scoped `Jolt` + `JoltDebugger` green.
- [ ] Baseline for Phase 5 = those numbers, plus `Ck.Jolt.DebugDraw.Benchmark.ScaleMatrix`'s
      recorded ms per case (`CkJolt/Claude.md:378-384`) — this phase adds per-body draw work and
      **must** re-run the benchmark at exit and record the delta.

## Design rulings (orchestrator, binding — verbatim)

| ID | Ruling |
|---|---|
| P5-D38 | **Line + text channels on `FCk_Jolt_DebugDrawTarget`.** Target owns a `ULineBatchComponent` in its target world (same NewObject + RegisterComponentWithWorld + TStrongObjectPtr ownership as the ISMs, P1-D14) and calls `Flush()` at the start of every capture flush; `FCk_Jolt_DebugRenderer::DrawLine/DrawTriangle` route to the ACTIVE target's line channel (no more direct `DrawDebugLine`); wireframe triangles = 3 lines. `DrawText3D` → target stores `FCk_Jolt_DebugDrawLabel {WorldPos, Text, Color}` per frame; consumers render labels (viewport OnPaint projection for the preview; the in-world subsystem via `DrawDebugString`). Public API also exposes `Draw_ExternalLine/Box/Sphere/Arrow(...)` for non-JPH contributors (probe results, health, drag visuals) — an "External" channel cleared per capture. If an `FPreviewScene` world cannot host a `ULineBatchComponent` (unverifiable headlessly until built) the fallback is a PDI `Draw` override on the viewport (Crowd mechanism) — executor STOPs and reports if hit. |
| P5-D39 | **Per-target draw flags** `FCk_Jolt_DebugDrawFlags` (bitmask): Shape (default on), Velocity, AngularVelocity, WorldTransform, CenterOfMassTransform, BoundingBox, MassAndInertia, SleepStats(if cheap; else drop), Constraints, ConstraintLimits, ConstraintReferenceFrames, ContactPoints, ContactNormals, SupportingFaces, Labels. Capture pass draws the per-body items itself from the flags (velocity arrow, axes, AABB…) — no `DrawBodies` in the capture; `DrawConstraints` called under the constraint flags. **The in-world draw is re-hosted onto the same flags**: the subsystem's default target's flags are sourced from the existing CVars (behavior-preserving; CVars remain the in-world source of truth) and the subsystem Tick path uses the same capture code (delete the duplicated `DrawSettings` block). |
| P5-D40 | **Contact recording.** JPH `sDrawContact*` statics are set to the UNION of registered targets' contact flags (recomputed when flags change). `FProcessor_JoltWorld_Step` wraps `PhysicsSystem::Update` in a renderer `Begin/EndRecord()` scope: `DrawLine`s during recording append to a frame buffer tagged Channel=Contact (cap 200,000 lines/frame, warn once at Display when hit); immediately after Update the renderer replays the buffer into every registered target whose flags request contacts (targets keep them until their next flush). Disclose in docs: contact toggles are process-wide (shared in-world + preview), unlike every other flag. |
| P5-D41 | **Highlight visibility.** Highlight colour → `{1.00, 0.15, 0.85}` (magenta, not near any palette entry), highlight bucket ALWAYS opaque (ignores palette opacity), overlay drawn at scale 1.03 about COM, highlight ISM `TranslucencySortPriority` = +1. Spec extends `HighlightAddsOverlayInstance`: asserts overlay instance scale ≠ 1.0 and highlight MID colour ≠ every palette colour. Add a **Hover** colour class (subdued: highlight at 0.5 alpha, scale 1.02) — see D43 for the mask. |
| P5-D42 | **Colour-by modes.** `ECk_Jolt_DebugDrawColorMode { BodyClass (default), SleepState, ObjectLayer, Island, ShapeType }` per target. Bucket key's colour-class becomes a `uint8` class index; visibility mask widens from `uint8` to `uint64` (static_assert ≤ 64 classes per mode); ObjectLayer > 61 → "Other"; Island → island index hashed into a 16-colour palette (islands churn per step: acceptable, active bodies re-draw every frame anyway; sleeping = "no island" class). Highlight and Hover classes are mode-independent (always the same two indices, always visible). Legend API `Get_LegendEntries(mode)` returns (class name, colour) so the debugger's legend follows the mode. |

| P5-D61 | **STOP-list rulings — the parts that bind this phase.** **S1:** contact recording goes **inside `FJoltWorld::DoPhysicsUpdate`** around `PhysicsSystem::Update`; buffer = `FCriticalSection`-guarded append, **active only while some target's contact flags are on**; **double-buffered and consumed on the GAME THREAD by `FProcessor_JoltDebugDraw_Capture`** (already after `WaitForAsync`), which replays into every registered target that wants contacts — **do NOT replay in the Step processor**; one-frame latency under async physics is ACCEPTED and documented. **S3:** the External channel is **retained named sub-channels** — `Draw_External*(Name, …)` accumulates, `Clear_External(Name)` empties, and each capture **flushes them to the line batcher WITHOUT clearing them**. **S6:** the ObjectLayer legend reads `Layer N — <object-channel name>` via a read-only reverse lookup on the layer table (add a getter if needed), falling back to bare `Layer N`. **S4:** `SleepStats` is DROPPED. **S5:** D41's "see D43" is a typo for **D42**. **S12:** mechanical `CkJoltDebugger` call-site collateral is in scope; no shims. |

> **D41's cross-reference (RULED, S5):** the ruling says "see D43 for the mask". D43 is the
> pause/step ruling; the mask widening is **P5-D42**. Take the mask from D42, and land D42's
> widening **before** D41's Hover class. Recorded rather than re-litigated.

## Research facts the executor must not re-derive

**Renderer / target**
- `FCk_Jolt_DebugRenderer::DrawLine` (`CkJolt_DebugRenderer.cpp:259-276`) already routes through an
  **active target**: it early-outs on `_Impl->_ActiveTarget == nullptr` (`:267`), pulls
  `_ActiveTarget->Get_World()` (`:270`) and calls
  `UCk_Utils_DebugDraw_UE::DrawDebugLine(World, …)` (`:274`). `DrawTriangle` (`:278-292`) is already
  three `DrawLine`s. `DrawText3D` (`:294-312`) calls `DrawDebugString` and **ignores `inHeight`**.
  So D38 is a re-targeting of an existing route, not a new one.
- `_ActiveTarget` lives at `CkJolt_DebugRenderer.cpp:217`; set in `BeginFrame` (`:488`) and
  `BeginCapture` (`:594`), cleared in `EndFrame` (`:507`) and `EndCapture` (`:736`). **There is no
  RAII scope type** — adding one is welcome but is not a ruling.
- Capture entry `Capture_JoltWorld` (`CkJoltDebugDraw_Capture.cpp:483-516`); technique steps
  declared `:213-220`, implemented `DrawInactiveBodiesOnSceneRevisionChange` `:234-310`,
  `DrawActiveBodies` `:312-348`, `RecolorBodiesThatFellAsleep` `:351-386`,
  `ReleaseDestroyedSleepingBodies` `:391-428`, `DrawCharacters` `:431-478`. Per-body shape draw is
  `Draw_Shape` `:120-139` (calls `JPH::Shape::Draw` directly — **the capture never builds a
  `DrawSettings`**), selection overlay `Draw_SelectionOverlay` `:144-158`, velocity sample
  `Sample_SelectionVelocity` `:162-175`, both dispatched from `Draw_Body` `:191-207`.
- Colour class enum `ECk_Jolt_DebugDraw_ColorClass : uint8` (`CkJolt_DebugDrawTarget.h:26-43`;
  `Static/Kinematic/Dynamic_Awake/Dynamic_Sleeping/Sensor/BakedStatic/Character/Highlight/Count`),
  `static_assert(... Count <= 8)` at `CkJolt_DebugDrawTarget.cpp:28-30`, bit helper `Get_ClassBit`
  `:32-38`. `_HiddenClassMask` is a **`uint8`** at `CkJolt_DebugDrawTarget_Impl.h:241`.
  Bucket key `FBucketKey {FBatch* _Batch; uint32 _ColorU32; ECk_Jolt_DebugDraw_ColorClass _ColorClass;}`
  at `Impl.h:69-88`. Palette `FCk_Jolt_DebugDrawPalette` `Target.h:47-81`, `Get_Color` switch
  `Target.cpp:190-226`.
- ISM creation/ownership pattern to copy for the line component: `TryEnsure_BucketIsm`
  (`CkJolt_DebugRenderer.cpp:420-480`) — `NewObject<…>(World, MakeUniqueObjectName(...))` `:448-449`,
  `RegisterComponentWithWorld(World)` `:466`, `TStrongObjectPtr` `_Ism.Reset(Ism)` `:473`;
  destruction `Destroy_BucketIsm` (`Target.cpp:265-282`, `DestroyComponent()` `:275`).
- **There is NO `ULineBatchComponent` anywhere in the whole Ck plugin suite** (`rg` over
  `Plugins/` returns only this campaign's own docs) — D38 is greenfield. The named fallback IS
  proven in-suite: `SCkCrowdDebugger_3dViewport.cpp:518-547` overrides
  `Draw(const FSceneView*, FPrimitiveDrawInterface*)` and draws with `PDI->DrawLine` (`:917-920`)
  and `FDynamicMeshBuilder` (`:698-742`).
- **There is no legend API in CkJolt** (`rg "Legend"` over `Source/CkJolt` → zero hits) and the
  colour-class enum is a plain non-UENUM `enum class` with no formatter. `Get_LegendEntries` needs
  a class→display-name table written from scratch.

**In-world draw (the behaviour that must not change)**
- Six CVars, all in `ck_jolt_subsystem::cvar` (`CkJolt_Subsystem.cpp`): `ck.Jolt.DebugDraw.Enabled`
  `:303-307`, `.SleepColoring` `:309-313`, `.Velocity` `:315-319`, `.WorldTransform` `:321-326`,
  `.Opacity` `:328-331`, `.Constraints` `:333-336`.
- Tick draw path `UCk_Jolt_Subsystem::Tick` `:514-608`: async gate `:528`, gate combination
  `:580-585` (`_DebugDrawGate` OR `DebugDrawEnabled`), `HideAll()` on the else branch `:603`,
  and the draw block `:587-598` — `Set_Opacity` `:589`, `BeginFrame(*_DefaultDebugDrawTarget)`
  `:591`, **`DrawBodies(DrawSettings, &Renderer)` `:592`**, `DrawConstraints` `:594-595`,
  `EndFrame()` `:597`, `NextFrame()` `:598`. The `DrawSettings` aggregate is `:530-578` and is
  **positional** (no designated initializers) — the block D39 deletes.
- Behaviour-preserving mapping (source of truth stays the CVars):
  `Velocity ← ck.Jolt.DebugDraw.Velocity`; `WorldTransform ← .WorldTransform`;
  `Constraints ← .Constraints`; `Shape` always on; everything else **off**, because today's
  `DrawSettings` hard-codes `false` for bounding box, COM transform, mass+inertia and sleep stats
  (`:541-554`). `SleepColoring` is NOT a draw flag — it selects
  `EShapeColor::SleepColor` vs `MotionTypeColor`, which under D42 is the **colour mode**
  (`SleepState` vs `BodyClass`). Map it there, not into the flags.

**Vendored Jolt 5.2.1 (`Source/CkThirdParty/Public/CkThirdParty/JoltPhysics/`)**
- `JPH_DEBUG_RENDERER` **and** `JPH_ENABLE_ASSERTS` are `PublicDefinitions` added
  unconditionally for every target type and configuration —
  `CkThirdParty.build.cs:27-28`. Every `#ifdef JPH_DEBUG_RENDERER` block is live, **and every
  `JPH_ASSERT` body-access / motion-type guard is live in Shipping too.**
- `BodyManager::DrawSettings` — `BodyManager.h:230-253`, 21 members; `EShapeColor` `:219-227`
  (`InstanceColor / ShapeTypeColor / MotionTypeColor / SleepColor / IslandColor / MaterialColor`).
- `PhysicsSystem::DrawBodies` `:150`, `DrawConstraints` `:153`, **`DrawConstraintLimits` `:156`**,
  **`DrawConstraintReferenceFrame` `:159`** (note the singular "Frame") — the last two are exactly
  what D39's `ConstraintLimits` / `ConstraintReferenceFrames` flags call.
- Contact statics — `ContactConstraintManager.h:242-248`: `sDrawContactPoint`,
  `sDrawSupportingFaces`, `sDrawContactPointReduction`, `sDrawContactManifolds`. Plain
  `static bool` (definitions `ContactConstraintManager.cpp:24-30`), **not** `inline static`, all
  under `JPH_DEBUG_RENDERER`. There is no per-body or per-target variant — these are process-wide,
  which is exactly why D40 takes the union and discloses it.
- `DebugRenderer` pure virtuals: `DrawLine :59`, `DrawTriangle :104`, both
  `CreateTriangleBatch :252/:253`, `DrawGeometry :280`, `DrawText3D :284`; singleton assert
  `DebugRenderer.cpp:75-76`.

## Work items

### Unit A — channels + flags + in-world re-host (P5-D38, P5-D39)

1. **`CkJolt_DebugDrawTarget.h/.cpp` + `_Impl.h`** — add the three channels: a
   `TStrongObjectPtr<ULineBatchComponent> _Lines` created like the bucket ISMs
   (`NewObject` + `RegisterComponentWithWorld(World)`), `TArray<FCk_Jolt_DebugDrawLabel> _Labels`,
   and the External channel. Public: `Get_Labels()`, and — **per P5-D61/S3 the External channel is
   RETAINED and NAMED** — `Draw_ExternalLine/Box/Sphere/Arrow(FName Channel, …)` which *accumulate*
   into their named sub-channel, plus `Clear_External(FName Channel)` which empties one.
   Whatever the renderer needs is reached as a `friend` (the class already friends
   `::FCk_Jolt_DebugRenderer` at `Target.h:159`).
   **Per-capture lifecycle:** `Flush()` the line component and clear the JPH line + label buffers
   at the **start** of the capture flush (alongside the existing `BeginCapture` reset), then
   **re-emit every retained External sub-channel into the line batcher without clearing it**.
   That asymmetry is the whole point of S3: JPH output is per-frame, External output is owned by
   its contributor (probe results, grid, drag line — Phase 8 / Phase 7) whose push rate is
   unrelated to the capture rate. → verify: build; existing 14 facility specs unchanged.
   **STOP** if `RegisterComponentWithWorld` on an `EditorPreview` world ensures or the component
   renders nothing — report and take the PDI-`Draw` fallback (D38's named branch).
2. **`CkJolt_DebugRenderer.cpp:259-312`** — re-point `DrawLine` / `DrawTriangle` / `DrawText3D` at
   the active target's channels instead of `UCk_Utils_DebugDraw_UE`. Keep the
   `_ActiveTarget == nullptr` early-outs. → verify: build.
   New spec **`Ck.Jolt.DebugDraw.LineAndLabelChannels`** in
   `Test_JoltDebugDraw_TargetReconcile.cpp` — after a capture with Velocity on, the target has a
   non-zero line count and the count RESETS on the next capture (proving the per-flush clear);
   a `DrawText3D` during capture lands in `Get_Labels()` with the right text and position;
   and — the S3 leg — `Draw_ExternalLine("Test", …)` **SURVIVES** two subsequent captures
   (retention), a second sub-channel does not disturb the first, and `Clear_External("Test")`
   empties exactly that one.
3. **`FCk_Jolt_DebugDrawFlags`** — a bitmask (plain `enum class : uint32` + `ENUM_CLASS_FLAGS`, or
   a `CK_PROPERTY`-style struct if the module has a precedent; match what the module already does)
   with the D39 members. `Set_DrawFlags` / `Get_DrawFlags` on the target.
   **`SleepStats` is DROPPED** — Jolt draws it from `MotionProperties`' internal sleep-test spheres,
   which are below the "FOR INTERNAL USE ONLY" banner (`MotionProperties.h:189-191`) and have no
   public accessor; since the capture does not call `DrawBodies`, there is no cheap way to get it.
   Record the drop and the reason in `CkJolt/Claude.md`. → verify: build.
4. **Capture draws the per-body extras itself** — extend `Draw_Body`
   (`CkJoltDebugDraw_Capture.cpp:191-207`) with flag-gated helpers next to `Draw_Shape`:
   velocity arrow (`Body::GetLinearVelocity()`), angular-velocity arrow, world-transform axes
   (`GetWorldTransform()`), COM axes (`GetCenterOfMassTransform()`), AABB wire box
   (`GetWorldSpaceBounds()`), mass+inertia box (`GetShape()->GetMassProperties()`).
   **Assert-safety (JPH_ENABLE_ASSERTS is ON in every config):** use
   `Body::GetMotionPropertiesUnchecked()` (`Body.h:289`) not `GetMotionProperties()` (`:285`,
   asserts `!IsStatic()`); use `MotionProperties::GetInverseMassUnchecked()` (`:96`) not
   `GetInverseMass()` (`:95`, asserts Dynamic); do **not** call `Body::GetAllowSleeping()`
   (`Body.h:131`) on a static body — it dereferences `mMotionProperties` unguarded.
   → verify: build; new spec **`Ck.Jolt.DebugDraw.DrawFlagsGatePerBodyExtras`** — with only
   `Shape` set the line count is 0; enabling Velocity on a moving body raises it; enabling
   BoundingBox raises it further; clearing back to Shape returns it to 0.
5. **Constraint flags** — call `PhysicsSystem::DrawConstraints` / `DrawConstraintLimits` /
   `DrawConstraintReferenceFrame` once per capture from the flags, after the body passes, inside
   the active-target scope so the lines land in the channel. → verify: build.
6. **Re-host the in-world draw (P5-D39 second half)** — `UCk_Jolt_Subsystem::Tick` `:514-608`:
   delete the `DrawSettings` block (`:530-578`) and the `DrawBodies`/`DrawConstraints` calls
   (`:592-595`); instead set the default target's flags + colour mode from the CVars per the
   mapping table above and let it go through the same capture code. Keep the gate (`:580-585`),
   the `HideAll()` else-branch (`:603`), `Set_Opacity` (`:589`) and `NextFrame()` (`:598`)
   **exactly as they are**. → verify: build; `[EDITOR-VERIFY]` 1–2. **This is the highest-risk
   item in the phase**: it is a behaviour-preservation refactor of the one path that was working
   before the campaign started (PROMPT success criterion 6). If parity cannot be held, STOP.
7. **`ULineBatchComponent` in a game world**: confirm the in-world path still shows velocity lines
   after item 6 (previously they went through `DrawDebugLine` into `World->LineBatcher`; now they
   go into the target's own component). → `[EDITOR-VERIFY]` 1.

### Unit B — contacts, highlight, colour modes (P5-D40, P5-D41, P5-D42)

8. **Colour mode first (D42 before D41 — ordering is load-bearing):** widen `_HiddenClassMask`
   from `uint8` (`Impl.h:241`) to `uint64`, move the `static_assert` (`Target.cpp:28-30`) from
   `<= 8` to `<= 64`, change `FBucketKey::_ColorClass` (`Impl.h:73`) to a `uint8` class index, and
   introduce `ECk_Jolt_DebugDrawColorMode` + `Set_ColorMode`/`Get_ColorMode`. The class index is
   computed per body per mode by a single `Get_ColorClassIndex(Mode, Body, …)` beside the existing
   `Get_ColorClass` (`Capture.cpp:89-118`). Changing the mode invalidates every bucket → re-arm the
   full pass the same way `Set_Palette` does (`Target.cpp:650-652`). → verify: build; the existing
   `ClassPalette` and `ClassVisibility` specs still pass (adapted to the index API).
9. **Mode implementations.** `BodyClass` = today's `Get_ColorClass`. `SleepState` = awake/asleep/
   static. `ShapeType` = `Shape::GetSubType()` (`Shape.h:201`) mapped into a name table — **do not
   index a table by the raw enum value**: `EShapeSubType` (`Shape.h:76-124`) has `User1..User8` and
   `UserConvex1..UserConvex8` gaps and appends `Plane`/`TaperedCylinder`/`Empty` at the end.
   `Island` = `MotionProperties::GetIslandIndexInternal()` (`MotionProperties.h:208` — **below the
   "FOR INTERNAL USE ONLY" banner at `:189-191`; document that we knowingly use it read-only for
   debug colouring**), hashed into 16 colours, static/sleeping → a "no island" class.
   `ObjectLayer` = the raw layer index bucketed with `> 61 → Other`; **its display name comes from a
   read-only reverse lookup on `FCk_Jolt_CollisionLayerTable` (P5-D61/S6) — `Layer N —
   <object-channel name of the signature registered at N>`, falling back to bare `Layer N` when the
   signature carries no channel. Add a getter on the table if none exists; it must be read-only and
   must never register a layer as a side effect.** → verify: build; new spec
   **`Ck.Jolt.DebugDraw.ColorModeRebucketsBodies`** — four bodies that share ONE shape and ONE
   body class but differ in shape sub-type / sleep state produce one bucket in `BodyClass` mode and
   more than one in `ShapeType` / `SleepState` mode, and switching back collapses them again.
10. **`Get_LegendEntries(Mode) -> TArray<FCk_Jolt_DebugDrawLegendEntry {FText Name; FLinearColor Color;}>`**
    — greenfield; a static table per mode. → verify: build; asserted inside the colour-mode spec
    (entry count per mode > 0, names unique, Highlight and Hover present in every mode).
11. **Highlight + Hover (D41)** — palette colour to `{1.00, 0.15, 0.85}`, highlight bucket forced
    opaque (bypass `_Opacity` in `Get_Color`, `Target.cpp:204-212`), overlay instance scaled 1.03
    about COM in `Draw_SelectionOverlay` (`Capture.cpp:144-158`), highlight ISM
    `TranslucencySortPriority = 1` in `TryEnsure_BucketIsm` (`Renderer.cpp:420-480`). Add the
    `Hover` class (mode-independent index, always visible) + `Set_HoveredBody(TOptional<uint64>)`
    using a new `HoverKeyBit` beside `CharacterKeyBit`/`HighlightKeyBit` (`Impl.h:46-47`).
    → verify: build; extend `Ck.Jolt.DebugDraw.HighlightAddsOverlayInstance` per D41 (overlay
    instance scale ≠ 1.0; highlight colour differs from EVERY palette colour) and add a Hover leg
    (hover and highlight coexist on two different bodies, each in its own bucket).
12. **Contact recording (D40, as refined by P5-D61/S1)** — implement exactly this shape:
    - `Begin_Record()` / `End_Record()` on the renderer switch `DrawLine` into a contact-tagged
      frame buffer (cap 200,000 lines, one `Display` warning when the cap bites).
    - **The record scope lives INSIDE `FJoltWorld::DoPhysicsUpdate` (declared `CkJoltWorld.h:158`),
      wrapped around `PhysicsSystem::Update` — NOT in `FProcessor_JoltWorld_Step`, which never
      calls `Update` (it calls `DoPhysicsUpdate` at `CkJoltWorld_Processor.cpp:199`, and in async
      mode from a TaskGraph thread, `:206-210`).**
    - The buffer is **`FCriticalSection`-guarded on append** (Jolt's solve is multi-threaded) and
      is **only armed while at least one registered target has a contact flag set** — when nothing
      wants contacts the recorder is off and the append path costs nothing.
    - The buffer is **double-buffered**: `End_Record()` swaps the filled buffer aside;
      **`FProcessor_JoltDebugDraw_Capture` — which already runs on the game thread after
      `FProcessor_JoltWorld_WaitForAsync` — CONSUMES it** and replays it into every registered
      target whose flags request contacts. **Nothing replays from the Step processor.**
    - **One-frame latency under async physics is ACCEPTED**; document it in `CkJolt/Claude.md`
      beside the process-wide caveat.
    - Union-of-targets recompute writes `JPH::ContactConstraintManager::sDrawContactPoint /
      sDrawSupportingFaces / sDrawContactManifolds` whenever any registered target's flags change.
    → verify: build; new spec
    **`Ck.Jolt.DebugDraw.ContactRecordingReplaysIntoDemandingTargets`** — with the fixture's
    physics system stepped once with two overlapping dynamic bodies and ContactPoints on, the
    target receives contact-channel lines; with the flag off it receives none; a second target with
    the flag off stays empty while the first fills. **The reconcile fixture never steps today**
    (`Move_Body` uses `SetPosition`, `Test_JoltDebugDraw_TargetReconcile.cpp:232-239`) — this spec
    needs a `Step()` helper added to `FScopedJoltWorld`.
13. **Re-run `Ck.Jolt.DebugDraw.Benchmark.ScaleMatrix`** with flags at their in-world defaults and
    again with every body flag on; record both in PROGRESS and in the `CkJolt/Claude.md` measured
    table (`:372-392`). Per-body extras are O(active) but multiply the line count — if the
    all-flags 100k number is pathological, say so rather than tuning silently.
14. **Docs weld** — `CkJolt/Claude.md`: the debug-draw section (`:274-523`), the colour section
    (`:435-447`), the CVar list (`:465-480`), the anti-patterns (`:617-634`), the spec census
    (`:505-523`) **and fix its existing defect** — the table header attributes all 14 rows to
    `Test_JoltDebugDraw_TargetReconcile.cpp` but `Benchmark.ScaleMatrix` (`:523`) lives in
    `Test_JoltDebugDraw_Benchmark.cpp`. Document the process-wide contact-toggle caveat (D40) and
    the `GetIslandIndexInternal` usage. Note the filename inconsistency (`Claude.md` here vs
    `CkJolt/CLAUDE.md` in the debugger's cross-references) and normalize.

## Fences

- **Behaviour of the in-world draw is a regression bar, not a refactor target** (PROMPT success
  criterion 6). Same CVars, same gate, same opacity, same `HideAll` on gate-close.
- The capture still never resolves entities inside a Jolt callback and never runs outside the
  async-safe window (P1-D9). Contact recording is the ONE new read point — and it is a *write* of
  lines, not a read of ECS.
- Never construct a second `FCk_Jolt_DebugRenderer` (`Claude.md:623-625`); `Get_OrCreate()` only.
- Never re-derive the keyspace — `Make_BodyKey` / `Make_CharacterBodyKey` remain the only
  definitions (`Claude.md:631-632`); `HoverKeyBit` joins them in `_Impl.h`, not in a consumer.
- `CkJolt_DebugDrawTarget_Impl.h` stays out of every TU but the debug-draw ones.
- **Collateral debugger edits are limited to making existing call sites compile** against changed
  signatures (`Set_ClassVisibility` / `Get_IsClassVisible` / `Get_BucketColorClasses` /
  `Get_Palette().Get_Color()` are consumed by `SCkJoltDebuggerWindow.cpp` `MakePopulationToggle`
  `:1029-1068` and `BuildLegendGroup` `:1099-1138`). No new lanes, no new behaviour, no new
  debugger specs — Phase 7 owns those. **No back-compat shims** (module doctrine): delete the old
  signature in the same change and fix the call sites.

## `[EDITOR-VERIFY]`

1. In PIE with `ck.Jolt.DebugDraw.Enabled 1`, the in-world draw looks **identical** to before this
   phase — same shapes, same colours, same opacity, velocity lines still present with
   `ck.Jolt.DebugDraw.Velocity 1`, constraints still drawn with `.Constraints 1`, and
   `ck.Jolt.DebugDraw.Enabled 0` still hides everything the same frame.
2. `ck.Jolt.DebugDraw.SleepColoring 1` still recolours sleeping bodies (now via the SleepState
   colour mode).
3. In the debugger's preview viewport, lines drawn by the facility appear at all (this is the
   `ULineBatchComponent`-in-a-preview-world question D38 cannot answer headlessly).
4. Contacts: two bodies resting on each other show contact points in the viewport when the contact
   flag is on, and turning it off in one target removes them everywhere (process-wide caveat).
5. Selection highlight is unmistakable against every palette colour, including against a sleeping
   body and a sensor.

## Exit criteria — ALL in the same commit as the last work item

- [ ] Scoped serial `--test-pattern Jolt` green with **≥ 4 new** `Ck.Jolt.DebugDraw.*` specs
      (line/label channels, flag gating, colour-mode rebucketing, contact replay)
- [ ] `--test-pattern JoltDebugger` green (collateral call-site changes only) and
      `--test-pattern DebuggerLauncher` census 3/3
- [ ] Full serial suite == baseline set
- [ ] Benchmark re-run recorded (default flags and all-flags) in PROGRESS + `CkJolt/Claude.md`
- [ ] Adversarial review (fresh Opus drafts triage; orchestrator ratifies) → fix-up → gate of
      record on the FINAL artifact
- [ ] `CkJolt/Claude.md`, PLAN, PROGRESS updated; commit LOCAL only (ship withheld)
