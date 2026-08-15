# Phase 6 — Pause/step, detail sample, selection contacts, multi-select/isolate, drag, stats (CkJolt)

> **Status:** ⏳ Pending
> **Depends on:** Phase 5 ✅
> **Scope (repo):** `Plugins/CkFoundation/Source/CkJolt/` + `Plugins/CkTests/.../UnitTests/CkJolt/`.
> Same collateral allowance as Phase 5 (mechanical `CkJoltDebugger` call-site fixes only).
> **Estimate:** 2 sessions (3 implementation units + review)

## Goal

After this phase the facility can be *interrogated* and *controlled*, not just watched: the Jolt
world can be paused and single-stepped and reports how long its last step took; a full per-body
sample and a per-character sample are taken in the capture pass (never from Slate); the contacts of
the selected body are available on demand; many bodies can be highlighted at once and everything
else can be hidden; a dynamic body can be dragged by a spring; and the world publishes real stats.
This is the last CkFoundation phase — Phases 7–8 consume it.

## Entry criteria

- [ ] Phase-5 exit green and committed; benchmark numbers re-recorded.
- [ ] Baseline for Phase 6 = those numbers.

## Design rulings (orchestrator, binding — verbatim)

| ID | Ruling |
|---|---|
| P6-D43 | **Pause / step.** Verify what sets `IsPaused()` today (grep the setter). Add `Request_StepOnce()` on the Jolt world: when paused, allows exactly one Step then re-pauses; exposed on `UCk_Jolt_Subsystem` (`Request_SetPaused`, `Request_StepOnce`, `Get_IsPaused`). Step processor also records `_LastStepDurationMs` (wall time of `Update`) on the world for stats. |
| P6-D44 | **Selected-body detail sample.** Extend the selection sample struct (currently linear velocity only) to `FCk_Jolt_DebugDraw_BodySample`: linear+angular velocity, mass (inverse mass → mass, 0 = infinite), friction, restitution, gravity factor, motion quality, object layer, broadphase layer, IsSensor, AllowSleeping, world AABB, shape type/subtype, shape scale, island index (`MotionProperties::GetIslandIndexInternal()` — read-only debug use, document it), user data. For character selections `FCk_Jolt_DebugDraw_CharacterSample`: ground state, ground normal, ground body key, ground velocity, character velocity, up. Sampled in the capture pass only (P3 rule: no PhysicsSystem reads from Slate). Multi-selection: sample the PRIMARY only. |
| P6-D45 | **Contacts of the selected body** via `NarrowPhaseQuery::CollideShape` with the selected body's shape at its COM transform, run in the capture pass on demand (only when a body is selected and the debugger asks): result list `{other body key, num contact points, penetration depth, contact points}` on the target; ContactPoints/normals for the selection are also drawn via the line channel. No ContactListener changes. |
| P6-D46 | **Multi-select + isolate.** `Set_HighlightedBodies(TArray<uint64>)` (keeps `Set_HighlightedBody` as the 1-element convenience) — Highlight keys are `HighlightKeyBit \| bodykey` so many coexist; `Set_IsolatedBodies(TSet<uint64>)` — when non-empty the capture skips every body not in the set (all populations), plus `Clear_Isolation()`. Both re-arm the full pass. |
| P6-D47 | **Mouse-drag facility (sim-mutating, dev-only).** `UCk_Jolt_Subsystem::Request_BeginDrag(BodyKey/BodyID, WorldGrabPoint)`, `Request_UpdateDrag(WorldTargetPoint)`, `Request_EndDrag()`; requests queue on the subsystem and are applied by a processor that runs BEFORE Step (same group discipline as the capture); implementation = kinematic anchor body (no collision layer / non-colliding object layer) + `DistanceConstraint` (spring: frequency 2 Hz, damping 1, min/max 0) between grab point (body-local) and anchor; body activated on begin and each update; end removes constraint + anchor. Dynamic bodies only; ignored otherwise (log Verbose). Verify a non-colliding object layer exists in CkJolt's layer setup — if not, STOP-report which layer to use. |
| P5-D61 | **STOP-list rulings — the parts that bind this phase.** **S2 (drag layer) RATIFIED:** register a layer for a **default-constructed `FCk_Jolt_CollisionSignature`** (`_ResponseMask = 0`, `_Domain = Dynamic`), **lazily on the first drag**. The anchor is a **raw JPH kinematic body with NO JoltBody entity**; the subsystem publishes **`Get_DebugInternalBodyIds()`** (or equivalent) and the **capture SKIPS those bodies — never drawn, never pickable, never listed**. The drag visual is an External line (P7-D54), not a facility-drawn primitive. **S10 (stats cadence) RATIFIED: N = 30 captures**, constant; the UI labels those fields **"(sampled)"**. **S9:** the ContactListener exists, so contact pairs are IN and the counter is atomic. **S1:** the contact buffer this phase's capture processor consumes is filled inside `DoPhysicsUpdate` (Phase 5). **S12:** mechanical `CkJoltDebugger` call-site collateral is in scope; no shims. |
| P6-D48 | **Stats extension.** World exposes step duration (D43), `PhysicsSystem::GetBodyStats()`, `GetNumActiveBodies(Rigid/Soft)`, constraint count (`GetConstraints().size()`), and — if CkJolt has an existing ContactListener — a per-step counter of OnContactAdded/Persisted for "contact pairs" (else omit contacts, record why). |

## Research facts the executor must not re-derive

**Pause (D43) — the answer to "grep the setter"**
- **CkJolt has no pause of its own.** The only pause in the module is `UWorld::IsPaused()`, read in
  two places: `FProcessor_JoltWorld_PlanStep::DoTick` (`CkJoltWorld_Processor.cpp:129`, which on
  pause zeroes the plan — `Set_NumStepsLastFrame(0)` `:131`, `Set_PendingSimTime(0.0f)` `:132`) and
  `FProcessor_JoltWorld_Step::DoTick` (`:175`). **The setter is the engine** (`APlayerController::
  SetPause` / `AWorldSettings`), not CkJolt — so `Request_SetPaused` is a NEW `_DebugPaused` field
  on `FJoltWorld` plus a second clause in both guards, not a wrapper over something existing.
- `FProcessor_JoltWorld_Step` is declared `CkJoltWorld_Processor.h:88-104` (Group `FGroup_Transform`
  `:90`, `RunAfter<FProcessor_JoltBody_KinematicPush, FProcessor_JoltCharacter_PreStep>` `:93`),
  `DoTick` at `.cpp:163-219`. **`PhysicsSystem::Update` is NOT called there** — the loop calls
  `JoltWorld->DoPhysicsUpdate(FixedDt)` (`:199`, inside the `StepLoop` lambda `:194-202`), declared
  `CkJoltWorld.h:158` and defined in `CkJoltWorld.cpp`. In **async** mode the whole `StepLoop` runs
  on a TaskGraph thread (`Async(EAsyncExecution::TaskGraph, …)`, `:206-210`); the sync branch is
  `:214-217`. Existing timing scopes: `STAT_CkJolt_WorldStep` (`:178`), `STAT_CkJolt_UpdateAsync`
  (`:208`), `STAT_CkJolt_Update` (`:214`) — `_LastStepDurationMs` should be measured where
  `STAT_CkJolt_Update`/`UpdateAsync` already are, i.e. around `DoPhysicsUpdate`, and written with a
  relaxed atomic because the async branch is off the game thread.
- Because pause is checked in **PlanStep**, step-once must be a one-shot consumed by PlanStep
  (emit exactly one step, clear the flag), not a flag the Step processor interprets — otherwise the
  accumulator model (`_Accumulator`, `_PendingSimTime`, `_NumStepsLastFrame`, `CkJoltWorld.h:242-251`)
  is bypassed. Pausing must also NOT let `_Accumulator` build up and burst on resume; mirror what
  the engine-pause branch already does at `:131-132`.

**Sample (D44) — assert-safety is the whole difficulty**
- Existing sample: `_HighlightedBodyLinearVelocity`, written by `Sample_SelectionVelocity`
  (`CkJoltDebugDraw_Capture.cpp:162-175`), reset per capture at `:510`, read by
  `Get_HighlightedBodyLinearVelocity` (`Target.h:232` / `Target.cpp:571-577`). Extend this, don't
  invent a parallel path.
- `JPH_ENABLE_ASSERTS` is on in **every** configuration (`CkThirdParty.build.cs:27`). Therefore:
  `Body::GetMotionProperties()` (`Body.h:285`) asserts `!IsStatic()` → use
  `GetMotionPropertiesUnchecked()` (`:289`); `MotionProperties::GetInverseMass()` (`:95`) and
  `GetInverseInertiaDiagonal()` (`:105`) assert Dynamic → use `GetInverseMassUnchecked()` (`:96`)
  or gate on motion type; `Body::GetAllowSleeping()` (`:131`) dereferences `mMotionProperties`
  **unguarded** → never call it on a static body; `GetPosition`/`GetRotation`/
  `GetCenterOfMassPosition` and `MotionProperties::Get{Linear,Angular}Velocity` assert BodyAccess
  read rights → only from the async-safe capture window, which is where this code lives.
- Field sources: `GetFriction` `Body.h:138`, `GetRestitution` `:142`, `GetObjectLayer` `:123`,
  `GetBroadPhaseLayer` `:120`, `IsSensor` `:76`, `GetWorldSpaceBounds` `:282`, `GetShape` `:261`,
  `GetUserData` `:293`, `GetWorldTransform` `:270`, `GetCenterOfMassTransform` `:276`;
  `GetGravityFactor` `MotionProperties.h:88`, `GetMotionQuality` `:34`,
  `GetIslandIndexInternal` `:208` (**below the INTERNAL-USE-ONLY banner at `:189-191` — document
  the knowing use**); `Shape::GetType/GetSubType` `Shape.h:200-201`,
  `GetMassProperties` `:241` (`MassProperties::mMass` `MassProperties.h:52`).
  `EMotionQuality` has only `Discrete`/`LinearCast` (`MotionQuality.h:14,28`).
- **Character:** the feature is `Public/CkJolt/Character/`, the JPH type is **`JPH::CharacterVirtual`**
  (`CkJoltCharacter_Fragment.h:58`), and every ground getter is on the **`CharacterBase`** base —
  `GetGroundState` `CharacterBase.h:97`, `GetGroundNormal` `:106`, `GetGroundVelocity` `:109`,
  `GetGroundBodyID` `:115`, `GetUp` `:70`, plus a free `sToString(EGroundState)` at `:92`;
  `EGroundState` `:83-89` (`OnGround/OnSteepGround/NotSupported/InAir`);
  `CharacterVirtual::GetLinearVelocity` `CharacterVirtual.h:193`. The capture already walks
  characters and holds the pointer — `DrawCharacters` `CkJoltDebugDraw_Capture.cpp:431-478`
  (`Get_Character().GetPtr()` `:452`, keyed by `Make_CharacterBodyKey_FromEntityId` `:455`), so the
  sample goes right there. Fragment mirrors also exist (`_GroundStateMirror` /
  `_GroundNormalMirror` / `_GroundVelocityMirror`, `CkJoltCharacter_Fragment.h:60-62`).

**Contacts of the selection (D45)**
- `NarrowPhaseQuery::CollideShape(const Shape*, Vec3Arg inShapeScale, RMat44Arg inCenterOfMassTransform, const CollideShapeSettings&, RVec3Arg inBaseOffset, CollideShapeCollector&, …)` —
  `NarrowPhaseQuery.h:53`. Collector alias `Shape.h:47`; use
  `AllHitCollisionCollector<CollideShapeCollector>` from `CollisionCollectorImpl.h`.
- `CollideShapeResult` (`CollideShape.h:59-67`): `mContactPointOn1`, `mContactPointOn2`,
  `mPenetrationAxis` (**contact normal = `-mPenetrationAxis.Normalized()`**), `mPenetrationDepth`,
  `mBodyID2`, `mShape1Face`/`mShape2Face` (`StaticArray<Vec3,32>`, `:57`).
- **`mShape1Face`/`mShape2Face` are only populated when
  `CollideSettingsBase::mCollectFacesMode != ECollectFacesMode::NoFaces`** (`CollideShape.h:80`,
  default is `NoFaces`) — D45's "num contact points" needs faces collected, so set it explicitly.
- Two traps: the query will report the selected body itself unless a `BodyFilter` excludes it, and
  `mMaxSeparationDistance` defaults to `0.0f` (`:100`) so only actually-penetrating pairs are
  returned; a small positive value is what makes "resting on the floor" show up.
- `sDrawContact*` statics are **not** involved here — D45 is a query, not the recorder.

**Multi-select / isolate (D46)**
- Keyspace bits live in `CkJolt_DebugDrawTarget_Impl.h:46-47` (`CharacterKeyBit = 1<<40`,
  `HighlightKeyBit = 1<<41`), with `Make_HighlightKey` `:57-63`. Phase 5 adds `HoverKeyBit` there.
- `Set_HighlightedBody` (`Target.cpp:502-531`) already releases the old overlay (`:515-517`),
  resets the velocity sample (`:523`) and re-arms the full pass (`_FullPassEverRan = false` `:528`).
  The plural form generalizes that — release the set-difference, not everything.
- Isolation must be applied in **every** technique step
  (`DrawInactiveBodiesOnSceneRevisionChange` `Capture.cpp:234-310`, `DrawActiveBodies` `:312-348`,
  `RecolorBodiesThatFellAsleep` `:351-386`, `DrawCharacters` `:431-478`) and must **release**
  already-slotted bodies that fall outside the set, or turning isolation on leaves stale instances.

**Drag (D47) — the layer answer**
- **There is no pre-registered non-colliding object layer.** Object layers are not an enum: they
  are allocated one per unique `FCk_Jolt_CollisionSignature`
  (`CkJoltCollisionLayer_Data.h:40-98`) in `FCk_Jolt_CollisionLayerTable` (max 1024,
  `CkJoltCollisionLayerTable.h:23-72`), seeded profile×domain by `Build_FromCollisionProfiles`
  (`.cpp:20-65`) which **skips** every profile whose `CollisionEnabled == NoCollision` (`:36-37`),
  plus one fixed probe layer (`Get_ProbeSignature` `:99-110`). Broadphase layers are exactly two
  (`Static{0}`, `Dynamic{1}`, `CkJoltCollisionLayerTable.h:78-83`).
- **Recipe — RATIFIED by P5-D61/S2, implement exactly this:** a *default-constructed*
  `FCk_Jolt_CollisionSignature` has `_ResponseMask = 0` (`CkJoltCollisionLayer_Data.h:55`), so
  `Get_OrRegisterLayer` (`CkJoltCollisionLayerTable.cpp:67-89`) on it yields a layer whose
  `Get_PairInteraction` (`.cpp:134-152`) is `Ignore` against everything —
  `ObjectLayerPairFilter::ShouldCollide` (`.h:139-147`) therefore returns false for every pair —
  and which is invisible to channel queries. Set `_Domain = Dynamic` so
  `ObjectVsBroadPhaseLayerFilter::ShouldCollide` (`.h:113-127`) does not cull it from the dynamic
  tree. Register once, lazily, on the world's layer table; `Get_OrRegisterLayer` is game-thread-only
  and returns `cObjectLayerInvalid` on exhaustion (`:79`) — handle that.
- Note that today a JoltBody's layer is **always** profile-derived
  (`CkJoltBody_Processor.cpp:233-258`); there is no params field for injecting a raw layer, so the
  anchor body must be created directly through the `BodyInterface`, not through the JoltBody feature.
- **CkJolt creates no kinematic helper bodies today** — nothing to reuse
  (`EMotionType::Kinematic` appears only in the enum conversion `CkJolt_Utils.cpp:235`, the capture
  colour switch `Capture.cpp:100`, and CkSpatialQuery's probes `CkProbe_Processor.cpp:455,472`).
- Jolt API: `BodyInterface::CreateAndAddBody` `BodyInterface.h:108`, `RemoveBody` `:101`,
  `DestroyBody` `:88`, `ActivateBody` `:141`, `MoveKinematic` `:198`, `SetPosition` `:188`
  (**no default for `inActivationMode`**), `GetCenterOfMassPosition` `:190`.
  `PhysicsSystem::AddConstraint` `PhysicsSystem.h:97` / `RemoveConstraint` `:100` (constraints are
  on the system, NOT the body interface). `DistanceConstraintSettings`
  (`DistanceConstraint.h:13-45`): `mSpace :25`, `mPoint1 :30`, `mPoint2 :33`, `mMinDistance :36`,
  `mMaxDistance :37`, `mLimitsSpringSettings :40`. `SpringSettings` (`SpringSettings.h`):
  `ESpringMode :15-19`, ctor `:31`, `mMode :44`, **`mFrequency`/`mStiffness` share a union
  (`:46-61`)** — set `mMode = FrequencyAndDamping` before writing `mFrequency`; `mDamping :67`.
  `EConstraintSpace` `Constraint.h:57-61` — note `LocalToBodyCOM` requires subtracting
  `Shape::GetCenterOfMass()`, so `WorldSpace` on begin is the simpler correct choice.
  `AddConstraint` takes a raw `Constraint*` while the manager holds `Ref<Constraint>` — keep a
  `Ref<DistanceConstraint>` and `RemoveConstraint` before dropping it.
- **The Jolt Samples project is NOT vendored** (zero hits for `mDragConstraint` / `DragAnchor` /
  `SamplesApp` anywhere in the repo). There is no reference implementation to copy; the recipe
  above is designed from the API, so budget an iteration and prove it with a spec.
- CkJolt already ships a full **constraint feature** — `ck::FFragment_JoltConstraint_Current`
  (`Constraint/CkJoltConstraint_Fragment.h:41-67`, holds `JPH::Ref<JPH::TwoBodyConstraint>` `:54`),
  `FCk_Handle_JoltConstraint`, `UCk_Utils_JoltConstraint_UE` (`CkJoltConstraint_Utils.h:18`),
  Setup/HandleRequests/LivenessReap/EndPlay processors, plus `UCk_Utils_JoltRope_UE`. **Do not use
  it for the drag** (the drag anchor is transient, sim-only, and must not become an entity), but
  Phase 8's constraint outliner does — see P8-D55.

**Stats (D48)**
- `PhysicsSystem::GetBodyStats()` `PhysicsSystem.h:191` returns `BodyManager::BodyStats`
  (`BodyManager.h:61-76`, 9 fields: `mNumBodies`, `mMaxBodies`, `mNumBodiesStatic`,
  `mNumBodiesDynamic`, `mNumActiveBodiesDynamic`, `mNumBodiesKinematic`,
  `mNumActiveBodiesKinematic`, `mNumSoftBodies`, `mNumActiveSoftBodies`). **`BodyManager.h:78`
  documents it as "slow, iterates through all bodies"** — at the campaign's 100k bar it must be
  throttled (sample every N captures, N constant, mirroring the P4-D32 cadence idea) and that
  throttle must be visible in the benchmark.
- `GetNumActiveBodies(EBodyType)` `:182` — **requires** the argument, no default; `EBodyType`
  is `RigidBody`/`SoftBody` (`BodyType.h:10-14`).
- `GetConstraints()` `:109` returns `Array<Ref<Constraint>>` **by value** — a full copy with a
  refcount bump per element. Take `.size()` and cache; never call it per frame.
- **A ContactListener DOES exist**, so D48's conditional resolves to *include* contact pairs:
  `class CkContactListener : public JPH::ContactListener` at `CkJolt_Subsystem.cpp:70`, with
  `OnContactValidate :74-82`, `OnContactAdded :84-129`, `OnContactPersisted :131-175`,
  `OnContactRemoved :177-202`; installed `:457-458`. It runs on Jolt worker threads under
  `FCriticalSection _QueueLock` (`:213`) — the per-step counter must be an atomic incremented in
  `OnContactAdded`/`OnContactPersisted` and snapshot+reset once per step, not a plain int.
- `FJoltWorld` publishes **no** body counts today (`CK_PROPERTY_GET` block `CkJoltWorld.h:242-251`
  is world/async/accumulator/alpha/steps/pending/optimize/two revisions/future). This is greenfield.

## Work items

### Unit A — pause, step-once, step duration, stats (P6-D43, P6-D48)

1. `CkJoltWorld.h/.cpp` — `_DebugPaused` (bool), `_StepOnceRequested` (bool), `_LastStepDurationMs`
   (atomic float), `_WorldStats` (a JPH-free `FCk_Jolt_WorldStats` struct) + `CK_PROPERTY` accessors
   and `Request_SetDebugPaused` / `Request_StepOnce`. → verify: build.
2. `CkJoltWorld_Processor.cpp` — extend the pause clauses at `:129` and `:175` to
   `World->IsPaused() || (Get_IsDebugPaused() && !ConsumeStepOnce())`; the one-shot is consumed in
   **PlanStep** so exactly one step is planned. Mirror the existing pause recovery
   (`Set_NumStepsLastFrame(0)`, `Set_PendingSimTime(0.0f)`, `:131-132`) so a resume does not burst.
   → verify: build; new spec **`Ck.Jolt.World.DebugPauseAndStepOnce`** next to
   `Test_JoltWorld_FixedTimestep.spec.cpp` — a paused world advances zero steps over N ticks; one
   `Request_StepOnce` advances exactly one and re-pauses; resume restores normal stepping; the
   accumulator does not burst after a long pause.
3. Step duration around `DoPhysicsUpdate` (`CkJoltWorld_Processor.cpp:199`, both the async `:206-210`
   and sync `:214-217` branches) using `FPlatformTime::Seconds()`, stored relaxed-atomically.
   → verify: build.
4. Stats sampling in the capture window (NOT in Slate): `GetBodyStats()` and the constraint count
   throttled to **every 30th capture (P5-D61/S10 — N = 30, a constant, and the UI labels those
   fields "(sampled)")**, `GetNumActiveBodies(RigidBody)` + `(SoftBody)` per capture, and the
   ContactListener's atomic pair counter snapshot per step. Expose one JPH-free
   `Get_WorldStats()` on the subsystem, with the sampled fields flagged as such in the struct so
   the UI does not have to guess. → verify: build; new spec
   **`Ck.Jolt.World.StatsReflectTheBodyPopulation`** — counts match a fixture with a known mix of
   static/kinematic/dynamic and awake/asleep bodies, and the throttle does not make them wrong
   (only late).
5. `UCk_Jolt_Subsystem` wrappers: `Request_SetPaused`, `Request_StepOnce`, `Get_IsPaused`,
   `Get_WorldStats` — mirroring the existing forwarder style at `CkJolt_Subsystem.cpp:769-789`.
   → verify: build.

### Unit B — samples, selection contacts, multi-select, isolate (P6-D44, P6-D45, P6-D46)

6. `FCk_Jolt_DebugDraw_BodySample` + `FCk_Jolt_DebugDraw_CharacterSample` (JPH-free: FVector,
   float, uint8 enums mirrored into `ECk_` enums or plain ints with documented meaning, FBox,
   FString for shape sub-type name) on `CkJolt_DebugDrawTarget.h`; `Get_BodySample()` /
   `Get_CharacterSample()` returning `TOptional<>`; written by an extended
   `Sample_SelectionVelocity` → `Sample_Selection` (`Capture.cpp:162-175`) and by `DrawCharacters`
   (`:431-478`). **Replace** `Get_HighlightedBodyLinearVelocity` — no back-compat shim; fix the two
   debugger call sites. → verify: build; the existing
   `Ck.Jolt.DebugDraw.HighlightedBodyLinearVelocity` spec is rewritten as
   **`Ck.Jolt.DebugDraw.SelectionSampleIsCaptureOwned`**, keeping its four discriminating legs
   (unset before a capture, correct after, follows a re-selection, cleared with the selection) and
   adding ≥ 5 new fields incl. a static body (no motion properties) and a sensor.
7. Selection contacts: `Set_ContactsRequested(bool)` + `Get_SelectionContacts()` on the target;
   the capture runs `CollideShape` only when a body is selected AND contacts are requested, with a
   self-excluding `BodyFilter`, `mCollectFacesMode = CollectFaces` and a small positive
   `mMaxSeparationDistance`; results drawn through the line channel under the ContactPoints /
   ContactNormals flags. → verify: build; new spec
   **`Ck.Jolt.DebugDraw.SelectionContactsFindTheRestingPair`** — two touching bodies: the selected
   one reports the other with a non-empty contact-point list and never reports itself; separating
   them empties the list; turning the request off empties it without a re-selection.
8. `Set_HighlightedBodies(TArray<uint64>)` (+ the 1-element convenience) — overlay slots keyed by
   `HighlightKeyBit | key` so N coexist; releasing only the set-difference on change. → verify:
   build; extend `HighlightAddsOverlayInstance` with a multi leg (3 highlighted → 3 overlay
   instances; dropping one releases exactly one).
9. `Set_IsolatedBodies(TSet<uint64>)` / `Clear_Isolation()` — applied in all four technique steps,
   releasing already-slotted bodies outside the set; re-arms the full pass. → verify: build; new
   spec **`Ck.Jolt.DebugDraw.IsolationHidesEverythingElse`** — with 4 bodies, isolating 1 leaves
   exactly 1 drawn (and releases the other 3's instances, not merely hides them); clearing restores
   all 4 without a revision bump; isolation and highlight compose (the isolated body still gets its
   overlay).

### Unit C — mouse-drag facility (P6-D47)

10. Request queue on `UCk_Jolt_Subsystem`: `Request_BeginDrag(uint64 BodyKey, FVector WorldGrabPoint)`,
    `Request_UpdateDrag(FVector WorldTargetPoint)`, `Request_EndDrag()` — JPH-free signatures,
    queued, one active drag at a time. → verify: build.
11. `FProcessor_JoltDebugDrag_Apply` in `CkJolt/Subsystem/` (or beside the capture processor):
    Group `FGroup_Transform`, `RunBefore TDepList<FProcessor_JoltWorld_Step>` — the same discipline
    the capture processor uses (`CkJoltDebugDraw_Processor.h:30-32`), and it must **also** run after
    `FProcessor_JoltBody_KinematicPush` so it does not fight the kinematic writer. Registered with
    `CK_REGISTER_PROCESSOR` (`CkJoltDebugDraw_Processor.cpp:20` is the pattern) — a processor
    without one is an unscheduled no-op.
12. Implementation per the RATIFIED recipe (P5-D61/S2): lazily register the ignore-everything
    object layer **on the first drag, not at world init**;
    `CreateAndAddBody` a kinematic anchor with a tiny sphere shape at the grab point;
    `DistanceConstraint` in `WorldSpace` with `mMinDistance = mMaxDistance = 0` and
    `mLimitsSpringSettings{ FrequencyAndDamping, 2.0f, 1.0f }`; `AddConstraint` on the
    PhysicsSystem; on update `SetPosition`/`MoveKinematic` the anchor and `ActivateBody` the
    dragged body; on end `RemoveConstraint` → drop the `Ref` → `RemoveBody` + `DestroyBody` the
    anchor. Dynamic bodies only; anything else logs at Verbose and is dropped.
    **The anchor is a raw JPH body with NO entity** — publish its id through
    `UCk_Jolt_Subsystem::Get_DebugInternalBodyIds()` and make the capture **skip every id in that
    set in all four technique steps AND in `TryPick_Body`**, so the anchor is never drawn, never
    picked and never listed in the outliner. → verify: build;
    new spec **`Ck.Jolt.DebugDrag.PullsADynamicBodyAndCleansUp`** — a dynamic body at rest, dragged
    toward a target point over M stepped frames, ends measurably closer to the target than it
    started (the discriminating assertion — not "the call did not crash"); `Request_EndDrag`
    removes the anchor and the constraint (body count and constraint count return to their
    pre-drag values); a static and a kinematic body are refused with no side effects; and **a
    capture taken mid-drag draws no instance for the anchor and `TryPick_Body` never returns it**.
13. Docs weld: `CkJolt/Claude.md` — a new "Pause / step / stats" section, the extended selection
    surface (`:404-426`), the drag facility with its **dev-only, sim-mutating, authority-only**
    warning, the new specs in the census (`:505-523`), the anti-patterns (`:617-634`) gaining
    "never call `GetBodyStats()` per frame" and "never take a drag on a client world".

## Fences

- Every `PhysicsSystem` read stays inside the capture window or a processor — **never Slate**
  (`CkJolt/Claude.md:626-630`, the rule that exists because a velocity read got in once).
- Drag is the only sim-mutating addition and is gated to dynamic bodies; it must be impossible to
  leave an orphaned anchor body or constraint behind (world teardown included).
- `GetBodyStats()` and `GetConstraints()` are throttled to N = 30 captures, never per-frame.
- The drag anchor must be invisible to every consumer surface (capture, pick, outliner) via
  `Get_DebugInternalBodyIds()` — a visible anchor is a bug, not a cosmetic issue.
- No back-compat shim for `Get_HighlightedBodyLinearVelocity` — replace and fix call sites.
- Island index and any other internal-use JPH accessor is read-only and documented at the call site.
- Same collateral rule as Phase 5: mechanical debugger call-site fixes only.

## `[EDITOR-VERIFY]`

1. Pausing from the console/API freezes the in-world draw and the sim; one step advances visibly;
   resume does not fast-forward.
2. The selected body's sample shows plausible mass / friction / layer / shape values for a dynamic
   body, and degrades safely for a static one (no assert, no crash).
3. A character selection shows a ground state that changes as it walks off a ledge.
4. Contacts of a body resting on the floor list the floor and draw at the contact points.
5. Isolating one body leaves the viewport with exactly that body; clearing restores everything.
6. Dragging a dynamic body in PIE moves it and it settles when released; dragging a wall does
   nothing; ending the drag leaves no leftover body (body count returns to normal).
7. Stats show a non-zero step time that grows with body count, and active counts that fall as
   bodies sleep.

## Exit criteria — ALL in the same commit as the last work item

- [ ] Scoped serial `--test-pattern Jolt` green with **≥ 6 new** specs (pause/step, stats,
      selection sample, selection contacts, isolation, drag) and the multi-highlight extension
- [ ] `--test-pattern JoltDebugger` + `--test-pattern DebuggerLauncher` green
- [ ] **Full serial suite** == baseline set — this is the end of the CkFoundation half, so the full
      gate runs here and again at the end of Phase 8
- [ ] Benchmark re-run recorded (the sample and stats work is per-capture cost)
- [ ] Adversarial review → fix-up → gate of record on the FINAL artifact
- [ ] `CkJolt/Claude.md`, PLAN, PROGRESS updated; commit LOCAL only (ship withheld)
