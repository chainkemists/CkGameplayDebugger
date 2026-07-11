# ECS Debugger Redesign — Campaign Tracker

**Spec (contract of record):** [docs/specs/2026-07-10-ecs-debugger-redesign.md](docs/specs/2026-07-10-ecs-debugger-redesign.md)
**Visual contract:** [docs/specs/ecs-debugger-redesign-mockup.html](docs/specs/ecs-debugger-redesign-mockup.html) (open in browser; Design notes = 12 ideas)
**Mode:** Fable end-to-end, one-shot campaign, phased with hard gates. Per-phase plan is
written into this file at each phase start; the spec is not restated here.

## Decisions log (resolved — do not re-litigate)

| Decision | Outcome |
|---|---|
| Routing | Fable implements all phases; spec-first; no monolithic upfront plan |
| Archetype home | CkEcs (CkFoundation) — framework primitive; debugger/overlay consume |
| Typed `CK_DEFINE_ARCHETYPE` struct | In-campaign, last phase (B), C++ only |
| HAS-rollup boundary | Internals only; stops at primary-entity boundaries |
| AS authoring | Primary path for game archetypes; `Get_FeatureIds` is BPNE (TArray-default gotcha); asset-registry discovery |
| Perf | Feature-flag bit cache (registry-ctx table, EnTT signals, cvar-gated) + incremental model; steady-state = zero O(n); baseline trace in Phase 0 |
| Today-parity | Fold + group are toggles; classic tree reachable |

## Known unknowns (verify, don't assume)

- [ ] EnTT 3.16.0 (vendored, `CkThirdParty/entt-3.16.0`) per-storage `on_construct`/`on_destroy` signals — verify availability given the global `in_place_delete=true` trait (`CkHandle.h:71-77`) (Phase A). Fallback: hook feature Utils' Add paths.
- [ ] Uniform member-type→Cast trait for typesafe handles (Phase B). Fallback: add one trait template in CkEcs.
- [ ] Slate SVG rasterizer vs stroke-based SVGs — visual check in editor (Phase 0 gate). Fallback: stroke→outline conversion pass.
- [ ] Large-map perf baseline needs a real map / stress gym — coordinate with maintainer (Phase 0 gate).
- [ ] AS `asset X of <AS-defined subclass>` + `default` ctor ergonomics — Phase A test asset.

## Phase status

| Phase | Scope (see spec §6) | Status |
|---|---|---|
| 0 | Icons + style + inspector metadata + perf baseline | **IN PROGRESS** |
| A | Feature-flag cache + archetype registry (CkEcs) | pending |
| 1 | Classification/rollup/names/query logic + incremental model | pending |
| 2 | Tree rework (fold/coalesce/badges/identity) + perf gate | pending |
| 3 | Query tokens + rail + pinned/recent | pending |
| 4 | Overview archetype map + Archetypes lens | pending |
| 5 | Activity feed + churn flashes | pending |
| B | `CK_DEFINE_ARCHETYPE` typed struct + TryCast | pending |

Gate for every phase: toolbox build green (`CkAuto/UnrealToolbox.exe`, never raw Build.bat),
debugger fully usable, phase log updated here. Phases 1/A/B add automation specs.
Editor must be closed for builds (PreToolUse hook enforces).

## Phase 0 — plan + status (2026-07-10)

1. ✅ `Resources/Icons/*.svg` — 23 monochrome white glyphs (geometry = mockup `IC` table 1:1;
   stroke-based — see unknowns re: rasterizer).
2. ✅ `FCkDebuggerStyle` — content root → plugin `Resources/` (plugin NAME is `CkDebugger`);
   `CreateIconBrushes` registers `CkDebugger.Icon.<Id>` via `FSlateVectorImageBrush`;
   public `Get_IconBrush(FName)` resolver (unknown id → Slate no-brush).
3. ✅ Inspector metadata: `ICkDebuggerComponentInspector_Base::Get_IconName/Get_FeatureColor`
   (defaulted virtuals — zero behavior change); `FCkDebuggerInspectorMetadata` + registry
   `Get_AllMetadata` propagate both. **Deferred to Phase 2** (when badges render): per-inspector
   overrides across the 40 inspectors + InspectorFilter curated-color migration.
   **Decision:** icons live in `FCkDebuggerStyle` (this plugin's documented brush home), NOT
   CkFoundation's CkEditorStyle — revisit only if the overlay needs the same glyphs.
4. ⬜ Perf baseline: Insights trace with debugger open on the largest available map — needs a
   human/PIE session, coordinate with maintainer. Record numbers + trace path here.
5. Gate status: ✅ toolbox build green (`Result: Succeeded`, Development, 2026-07-10);
   ✅ editor-boot smoke via `--test --test-pattern Timer` → 36/36 passed (brush
   registration runs at module startup, so the boot exercises it). One build iteration:
   `IPluginManager::Get()` → LNK2019 (no Projects dep) → swapped to CkCore's
   `UCk_Utils_IO_UE::Get_PluginsDir(<folder name>)`, the CkEditorStyle pattern.
   ⬜ `[EDITOR-VERIFY]` icon render check below; ⬜ perf baseline (item 4).

`[EDITOR-VERIFY]` Phase 0 icon check (agents cannot PIE):
1. Build, open editor, console `ck.EcsDebugger` to open the tab (Tools menu → Debug section).
2. Temporary check until Phase 2 renders badges: in Widget Reflector or a scratch widget,
   confirm brush `CkDebugger.Icon.Timer` resolves and draws (not empty). Alternative: wait for
   Phase 2 where every tree row exercises the brushes; then this check is subsumed.
3. If glyphs render blank/garbled → stroke-support gap in the SVG rasterizer → run the
   stroke→outline conversion fallback (unknowns above).

## Visible slice (pulled forward from Phase 2) — 2026-07-10

Inspector section headers now render each inspector's feature glyph + color:
`SCkDebug_InspectorPanel` gained optional `IconBrush`/`IconColor` (nullptr = unchanged),
the ECS inspector panel passes them on inspector-titled headers, and **27 inspectors
declare `Get_IconName`/`Get_FeatureColor`** (colors = mockup FEAT table). Commit `1a5aa3e`,
gate: build green + Timer 36/36. **`[EDITOR-VERIFY]` PASSED 2026-07-10** — user screenshot
confirms crisp glyphs (Timer amber clock, Network blue globe): stroke-based SVGs rasterize
fine; no outline-conversion fallback needed. Phase 2 may lean on the pipeline freely.

**Bonus fix (`b46fd5e`):** the long-standing garbled collapse indicator — text chevrons
(▾/▸) in the shared `SCkDebug_InspectorPanel` + `SCkDebug_ExpandableColumn` render as
unknown-glyph boxes — replaced with engine `Icons.ChevronDown/Right` SVG brushes; every
debugger composing these widgets is fixed. Follow-up (user-visible text glyphs elsewhere,
NOT yet fixed): SM debugger ▶ play/resume buttons, EQS "View ▾" label, GOAP graph ▸ strips.
Remaining without glyphs (need new SVGs or a mapping decision, Phase 2): EntityInfo,
Relationships, DynamicFragments, EntityCollections, Tween, Physics, Shapes, OverlapBody,
Resolver, AStar, UI, MontagePlayer, AnimPlans.

## Phase A — IN PROGRESS (CkFoundation/CkEcs), design locked 2026-07-10

**Recon resolved (all cited first-hand):**
- ✅ EnTT sinks: `registry.hpp:1022/:1070` (`on_construct/on_destroy<T>()`);
  `mixin.hpp:79-99` publishes destruction on the in-place branches → tombstone trait does
  NOT disable signals. Unknown #1 CLOSED.
- `FCk_Registry` = non-owning view; raw entt registry via
  `ck::registry_table::Resolve(FCk_RegistryHandle)` (public, `CkRegistry_SlotTable.h:61`);
  `EnttRegistryType = entt::basic_registry<FCk_Entity::IdType,...>` (`:17`).
  Ctx pattern: `namespace ck { struct FCtx_X {...}; }` + `SetContext/GetContext/TryGetContext`.
- Handle→registry: `Get_Registry` was RENAMED (view semantics, `CkHandle.h:273`) — resolve
  registry via the handle's registry view or `UCk_EcsWorld_Subsystem_UE::Get_Registry()`.
- Specs home: `CkTests/Source/CkTests/Private/UnitTests/*.spec.cpp` (in-module `.spec.cpp`
  are EMPTY placeholders — do not copy them). New spec needs touch+rebuild relink.
- entt include style: `#include "entt/entity/registry.hpp"`.

**Design (locked):**
- `CkEcs/Public/CkEcs/DebugFeatureFlags/` — `ck::FCtx_DebugFeatureFlags` (uint64 row per
  entity index, grows on demand; rows self-clear because each feature's `on_destroy` fires
  on fragment remove AND entity destroy). Global connector registry:
  `ck::debug_feature_flags::RegisterFlag<TFragment>(FeatureId)` (≤64 flags, ensure on
  overflow); per-bit `FBitListener` instances stored in ctx and connected as entt sink
  payloads (sinks don't take capturing lambdas). Enable(reg) = connect + O(n) seed via
  `view<T>()`; Disable = disconnect + drop ctx. BP/AS surface:
  `UCk_Utils_DebugFeatureFlags_UE` (Request_Enable/Disable, Get_IsEnabled, Get_Flags→int64,
  Get_HasFeature, Get_BitIndex).
- **Feature→fragment registration lives DEBUGGER-side** (CkEcsDebugger startup registers
  ~20 marker fragments; CkEcs stays feature-agnostic — tier rule: CkEcs (T2) cannot see
  T4 feature modules). Phase A ships mechanism + specs with test-local fragment types.
- `CkEcs/Public/CkEcs/Archetype/` — `FCk_ArchetypeDescriptor` (Name, DisplayName,
  FeatureIds:FName[], RequiredLabel:FName — label MATCHING is consumer-side, CkEcs can't
  dep CkLabel (T2→T2 sibling, inverted dep), NamePattern, IconSvgPath, Color, Priority,
  optional native matcher fn), `FCk_ArchetypeRegistry` (static; Register/Unregister/Find/
  Get_All/TryGet_BestMatch using flag-cache bits for FeatureIds), `UCk_ArchetypeDefinition`
  UDataAsset (BPNE `Get_FeatureIds`), `UCk_Utils_Archetype_UE` (BP/AS surface).
- AS test asset (authoring validation): CkTests `Script/` — with the archetype phase gate.
- `CK_DEFINE_ARCHETYPE` typed struct = Phase B, untouched.

**RESULT — Phase A gate GREEN (2026-07-10):**
- `CkEcs/DebugFeatureFlags/` (4 files) + `CkEcs/Archetype/` (6 files) build green.
  One compile iteration: entt::any (ctx backing) requires copy-constructible payloads →
  move-only sink listeners/connections live behind `TSharedPtr<FImpl>` in the ctx struct.
- Specs 4/4: `Ck.DebugFeatureFlags.{RegisterEnableSeedAndLiveBits,
  IdempotentRegisterAndUnknownQueries}` + `Ck.Archetype.{RegisterFindReplaceUnregister,
  MatchingViaFlagCacheAndNativeMatcher}` (CkTests/Private/UnitTests). Behaviorally proves
  sinks fire under in_place_delete: seed scan, live bits, erase/destroy clearing.
- AS authoring VALIDATED: `CkTests/Script/CkEcs/CkArchetype_AuthoringValidation.as`
  (`asset X of UCk_ArchetypeDefinition` + `FeatureIds.Add(n"...")`) compiles clean at
  editor boot. **Spec §3.3 amended**: plain TArray UPROPERTY + imperative `.Add()`
  (the UCkDynamic_HandleDefinition pattern) — the BPNE accessor was unnecessary.
- Feature→fragment flag registration (the ~20 markers) is Phase 1, debugger-side.
- Phase B note: native matcher param on `ck::archetype_registry::Register` is the
  `CK_DEFINE_ARCHETYPE` TryCast hook.

## Phase 1 — IN PROGRESS (2026-07-10)

**Chunk 1 (this session): flag registration + enable wiring.**
- `CkEcsDebugger/FeatureFlags/CkEcsDebugger_FeatureFlags.h/.cpp` — `RegisterAll()` maps
  10 verified marker fragments: Timer, Transform, SceneNode(Current), Probe,
  FloatAttribute(Current — on the ATTR entity), StateMachine(Sm_Params), Aggro(Current),
  Label(GameplayLabel), InteractionResolver, AudioTrack. Called from
  `FCkEcsDebuggerModule::StartupModule`. Enable hook:
  `FCkDebuggerModel_WorldContext::Refresh_EntityCache` → `EnableFor(registry view)`
  (idempotent; ctx + sinks die with the registry on PIE end — no explicit teardown).
- **Remaining flag candidates:** Inventory, Objective, Vfx, Tween, Ism/IskmProxy,
  EntityTag, Net, ActorBridge, Camera (verify marker fragment + include per feature).

**Chunk 2 — DONE (2026-07-10, gate green: build + editor boot + Ck.DebugFeatureFlags 2/2).
`Test_Entity` → bit tests. PARITY VERDICTS (all cited first-hand — the contract for any
future wiring):**
- `CK_DEFINE_HAS_CAST_CONV_HANDLE_TYPESAFE` generates `Has` = `Has_All<every listed
  fragment>` (CkHandle_TypeSafe.h:195-203) — a single bit is parity ONLY when the listed
  fragments are added/removed atomically with the registered marker.
- **WIRED (5)** — `Get_FeatureFlagId` overrides: Timer (`Has_All<Current,Params>`, both
  added in one construct lambda CkTimer_Utils.cpp:58-59, no individual removal);
  Transform (single-fragment `Has_All<FFragment_Transform>`); SceneNode
  (`Has_All<SceneNodeParent,Current>`, co-added DoAdd :143-145, co-removed ONLY in
  Request_Detach :229-230); Probe (`Has_All<Params,Current>`, co-added :30-32);
  InteractionResolver (`Has_All<Params,Current>`, co-added :32-33).
- **DO-NOT-WIRE**: FloatAttributes (`Has_Any` OWNER-side record — bit is on the attr
  sub-entity); StateMachine (`Has_Any` over 7+ Sm fragments); Aggro
  (`Has_Any<Aggro_Current,AggroOwner_Current>`); Audio
  (`Has_Any<AudioTrack_Current,AudioDirector_Current>`). Their flags stay registered
  for classification/rollup only.
- **Chunk-1 defect fixed**: Transform flag was registered on `FFragment_Transform_Params`
  — a ParamsData ALIAS never added to any entity (real pool = `FFragment_Transform`,
  CkTransform_Utils.cpp:41). The bit read permanently zero. Re-pointed.
- Implementation: base `Get_FeatureFlagId()` (default NAME_None = legacy) with the
  parity contract in its doc comment; metadata propagates it; filter-model entries
  resolve id→bit at construction; `Test_Entity` + `Test_Entity_IsExcluded` bit-test when
  the entity's registry has the cache enabled, else instantiate via registry (legacy
  byte-for-byte when cache off). Bonus: exclusion token→inspector-ID substring matching
  (entity-independent) precomputed per exclusion-set change instead of per entity per
  pass (`DoRebuildExclusionMatches`), identical union semantics.

**Chunk 3 — DONE (2026-07-10, gate green: build + Ck.EcsDebugger.Classification 2/2):**
classification + HAS-rollup as pure logic over the bit cache.
- `CkEcsDebugger/Classification/CkEcsDebugger_Classification.h/.cpp` —
  `BuildTable(internal-set)` snapshots flag bits (structural mask = Transform|Label);
  `Classify(ownBits)` = internal iff exactly one non-structural bit AND it's in the
  internal mask (spec §3.1 literally: structural-only entities are PRIMARY);
  `ComputeRollups(bits[], ownerIndex[])` = each internal's X counts toward its nearest
  PRIMARY ancestor, walks through internal chains, stops at primaries, cycle-guarded
  (path-compressed memo; owner cycles contribute nothing — snapshot-restore history).
  Pure arrays in/out → Phase 2 feeds cached entities, specs feed synthetic forests.
- `UCkEcsDebuggerSettings::InternalFeatureIds` (Config, seeded: Timer, SceneNode,
  Probe, Float/Byte/IntegerAttribute, CueRelay).
- Flags: +ByteAttribute, +IntegerAttribute (`FFragment_<X>Attribute_Current`, verified
  single-fragment `Has` like Float). 12 registered total.
- Specs (in-module, `Private/Tests/`, overlay-module pattern):
  `Ck.EcsDebugger.Classification.{SignatureAndInternalSet, RollupStopsAtPrimaries}` —
  discovered on first link (no relink trap this time).
- **Known gaps (recorded):** (a) features without registered flags are blind spots —
  an entity carrying only unregistered features + one internal feature misclassifies
  internal; mitigation = keep extending RegisterAll (remaining candidates in chunk 1
  note) + the set is user-editable data. (b) CueRelay is an ACTOR (`ACk_CueRelay_UE`),
  no marker fragment — its internal-set entry is inert until a marker exists.

**Chunk 4 — DONE (2026-07-10, gate green: build + Ck.EcsDebugger 5/5).**
- Query tokenizer `CkEcsDebugger/Query/` (spec §3.5): Parse (incomplete tokens DROP,
  unknown keys/malformed ids → fuzzy), FFeatureTokenTable (prefix, id+display-name →
  bits), Matches (has: own∪rollup; is: own; is:primary/aux; net: prefix-forgiving;
  id:; arch: substring; fuzzy via ck::fuzzy), Get_InferredArchetypeKey
  ("Base#signature"). 3 specs. NOTE: `EQueryNetMode` (engine ENetMode collides).
- **O(1) churn detection**: CkEcs `debug_feature_flags::Get_Revision` (sink-bumped
  counter) + debugger flag `_TreeEntity` on FFragment_LifetimeOwner (underscore prefix
  = infrastructure → structural in classification, skipped by badges/tokens). World
  model polls revision in IsCacheDirty → tree LIVE-TRACKS spawns (it never did!).
- Incremental world model: Refresh_EntityCache diffs vs previous set
  (Get_LastAdded/Removed + OnCacheDiff broadcast — Phase 5's feed source); dead-world
  refresh drops handles without broadcasting.
- Tree: nodes cache DebugName/CleanName/DisplayText at creation (rows previously
  derived names PER ROW PER FRAME via bound attributes); ApplyCacheDiff incremental
  add/remove (stable TSharedPtr identity); RecomputeNodeSignatures per churn (bits,
  classification, rollup, registry-first ArchetypeKey, NetRole); selection restore +
  auto-expand now gated to full rebuilds/filters (were about to fire at 10 Hz).
  Manual Refresh button = ForceFullRefresh (re-derives names).

**Phase 2 — DONE (same gate).** Tree presentation layer:
- Fold internals (settings default + toolbar toggle + per-owner "+N" chip — ASCII on
  purpose, decorated glyphs = tofu). Filter-overrides-fold rule. ExpandToReveal
  auto-unfolds the owner on external navigation to a folded internal.
- Sibling coalescing: same-ArchetypeKey runs ≥ threshold (settings, default 5) →
  synthetic "Base xN" group rows; stable identity via GroupNodeCache keyed
  (owner ptr, key), pruned on node removal (ptr-reuse guard); group click never
  clobbers entity selection; context menu on group = Copy all N names/IDs.
- Identity glyphs (internal = feature icon, primary = Cube) + IS/HAS badge strip
  (solid = own, hollow+count = rollup; cap 6 + "+k"); feature visuals from inspector
  metadata (flag-wired 5) + manual rows for flag-only ids (SM/Aggro/Audio/Label/attrs).
- Status bar: "Entities: N (P primary · I internal)". Deviation from spec §5.3: no
  30-row "Show all" cap inside expanded groups — STreeView virtualization already
  bounds row materialization; revisit only if group expansion measures hot.

**Phase 1 leftovers folded forward:** computed names §3.4 partially (cleaned names
cached; archetype DisplayName + role-name pipeline lands with Phase 4's lens);
refresh time-budget (§5.4) not needed yet — steady state is zero-O(n) by revision gate.

## Phases 3–5 + B — DONE (2026-07-10/11, single session, per-phase gates green)

**Phase 3 (`85cbc68`):** Filter+Highlight inputs parse the §3.5 grammar and evaluate
against cached node signatures (plain text = fuzzy, unchanged UX); "?" help button;
feature rail (icon chips, include-any semantics, own∪rollup); Pin/Unpin context menu +
Pinned/Recent sections above the tree (Recent = last 5 selections, panel-tracked).
Rail/filter suppress folding so matches stay reachable. Feature visuals shared via
`Presentation/CkEcsDebugger_FeatureVisuals`. Deviation: rail is include-only (spec's
rail-exclusion composes through the existing exclusion popover instead).

**Phases 4+5 (`5327429`):** Archetypes page (hero count + per-archetype cards,
registered-first with [GAME] tag + signature badges; click → `arch:<name>` into the
Filter bar via new page-context RequestEntityFilter + `SCkDebug_DualSearchBar::
Set_FilterText`; 1 Hz rebuild only while visible). Activity page (spawn/destroy feed
from OnCacheDiff, 200-event ring, click-to-select spawns, world-switch clears).
Spawn rows flash the tree status dot green 0.6s. Deviations: no sparklines/family
bars; no unseen-count tab badge; SM-transition rows not tapped (stretch item).

**Phase B:** `CkEcs/Archetype/CkArchetype_Typed.h` — `CK_ARCHETYPE_BODY` +
`CK_DEFINE_ARCHETYPE(Struct, "Name", Members...)` (≤8): generates all-or-nothing
`TryCast` from the member-name==feature-name==Utils-class convention (purely textual —
utils resolve at expansion site, CkEcs stays feature-agnostic; the open cast-trait
question from §3.3 resolved as unnecessary) + deferred descriptor registration
(EndOfEngineInit) with TryCast as the native matcher. USTRUCT optional (spec's sketch
implied required — relaxed). Spec: `Ck.Archetype.TypedTryCastAndAutoRegistration`
over real Transform+Timer fragments.

## Session log

- **2026-07-10 (Fable):** Spec + mockup written and reviewed (user + colleague ideas
  incorporated: IS/HAS, archetypes, perf architecture). Campaign tracker created. Phase 0
  implemented: 23 SVGs, style content root + icon brushes + `Get_IconBrush`, inspector
  icon/color metadata plumbing (registry + base interface). Toolbox build launched.
  Docs commit to `dev` pending user approval.
