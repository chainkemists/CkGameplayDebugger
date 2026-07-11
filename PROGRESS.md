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

**Polish pass (post user PIE test, 2026-07-10):** three defects from first hands-on:
(1) Fold/Group toggles + rail chips rendered as grey squares — `FAppStyle`
`"ToggleButton"` doesn't exist in this engine, so Slate fell back to a
default-constructed `FCheckBoxStyle`; replaced with own `CkDebugger.ToggleChip`
(rounded chip, accent fill+outline when checked). (2) Archetypes cards flickered —
`ClearChildren()`+recreate every 1 s violated the spec §4 stable-identity contract;
now a key→widget card cache updates counts in place and only re-slots on key-set/order
change, with a deterministic sort tie-break (unstable `TArray::Sort` over equal counts
would otherwise reorder every tick). (3) Cards restyled to the mockup shape:
`CkDebugger.Card` rounded hover-outline button, tinted icon well, GAME pill, 16 pt
hero count + "instances", badges row; hero line adds archetype count. Gate:
build + 5/5 `Ck.EcsDebugger` specs.

**Polish pass 2 (second PIE round, 2026-07-10):** (1) Archetype cards became
multi-select toggles: repeated `arch:` terms now OR-compose in the query (an entity
has exactly one archetype — AND was always empty); tokenizer honors double quotes so
multi-word tokens (`arch:"UnrealComponent: BackWall"`) survive whole; cards derive
checked state from the live Filter text (context gained `GetEntityFilter`) and
add/remove their token with word-boundary-safe string surgery. +4 spec cases.
(2) Uniform grid: fixed 200 px card width + name ellipsis. (3) Cube fallback replaced
by dominant-feature glyph (first flagged feature in the signature, feature-colored);
registered bespoke icons validated at bucket time. (4) Distinct attribute icons:
new AttributeFloat/Byte/Integer/Vector SVGs, per-type hues, inspectors re-pointed;
`VectorAttribute` flag registered (same TUtils_Attribute machinery = same parity).
(5) Rail narrowing now auto-expands to reveal matches and greys non-matching
ancestors (`IsNarrowingMatch` on tree nodes, also applies to Filter queries).
(6) Pinned/Recent headers are label+rule separators with spacing. Known gap:
features without registered flags (Message, CueRelay, EntityScript, relays) still
have no glyph — needs marker-fragment flags per feature (Phase 1 tracker list).

**Polish pass 5 (fourth PIE round, 2026-07-11):** (1) Rail chips were stretching to
the rail's full width — the grouped rail's SVerticalBox slots default to HAlign_Fill
and the AutoWidth column is sized by the widest group label; chips now HAlign_Left.
(2) Full feature-coverage audit, per the maintainer's "count the `Ck*_Fragment`
files" method: 127 fragment headers across ~55 CkFoundation modules inventoried
(struct decls + `using` aliases + `Add<>`/`AddOrGet<>` call sites). Result: 32 new
flags, every one keyed on a fragment verified to be added at feature-Add() time —
EntityExtension, UnrealComponent, Snapshot (SaveKey), TagSet, EntityTag,
RotatorAttribute (+ bespoke AttributeRotator glyph), CrowdAgent, Grid (+ bespoke
glyph), Marker, Sensor, RaySense, Velocity, Spline, InteractSource, InteractTarget,
Inventory (Params alias existed after all), Item, Team, Player, Projectile
(tag-keyed), ResolverSource, ResolverTarget, GeometryCollection, AnimPlan,
MontagePlayer, VatProxy, RenderTarget, WorldSpaceWidget, CameraShake, Vfx (CkFx
leaf), AudioDirector, Sfx. Existing "Vfx" flag renamed **VfxCue** (it keys on
CkVfx's cue coordinator; CkFx's leaf Niagara wrapper now owns "Vfx"). 56/64 flag
bits used — the next batch this size needs the cache widened past one uint64 row.
Deliberately unflagged with reasons (recorded in FeatureFlags.cpp): Cue/Messaging
(record/transient-entity based), Nav (request-driven), AStar (rides on Goap
planners), EntitySpawner (transient PendingSpawn), Variables/Dynamic (no single
static type), Shapes, sub-features (Homing/Ballistic/CameraLayer/AnimAsset), owner
sides, renderer singletons, LagCompensation (revisit), infrastructure. (3) Rail
regrouped into 12 clusters (Core/Tags/Attributes/AI/Spatial/Motion/Gameplay/Combat/
Animation/Rendering/Audio/Other), ~53 chips; new features' glyphs are hand-picked
semantic matches from the 148-icon general pool (People, Backpack, Coin, Sword,
Shield, Tv, FilmReel, …) with per-feature hues. CkEcsDebugger.Build.cs gained 10
module deps (Chaos, Crowd, Fx, Projectile, RaySense, RenderTarget, Snapshot,
Spline, UnrealComponent, Vat). Inspector fast-path (Get_FeatureFlagId) remains
unwired for the new batch, same as batch 2.

**Polish pass 4 (third PIE round, 2026-07-11):** (1) Overview became the mockup
dashboard: hero count + live 60-sample sparkline (new volatile SLeafWidget
`SCkEcsDebugger_Sparkline`), population-by-family bars (descriptor gained `_Family`
in CkEcs; internals/dominant-feature/Other for unregistered), top-12 archetype cards
with per-card sparklines + family labels, singletons list with EntityRef pills; the
hierarchy graph moved to a "Graph" tab; Activity tab gained the unseen-count badge
(`Get_BadgeCount` on the page interface). Aggregation extracted to shared
`Presentation/CkEcsDebugger_ArchetypeAggregation` (keys/glyphs/colors/family resolved
once for both pages); arch-token toggle helpers moved into the query namespace.
(2) Selection highlight was unreadable (engine TableView.Row saturated fill) —
own `CkDebugger.TableView.Row` style with translucent accent selection, applied to
tree + activity rows. (3) Icons got color: 16-hue curated palette, stable
FCrc-hash-assigned per archetype key; registered descriptors' explicit `_Color`
wins; feature glyphs keep feature hues. (4) Rail: 10 new feature flags (Objective,
Vfx=VfxCue, Camera, Goap=Planner, Eqs=QueryState, IsmProxy, IskmProxy, ActorBridge=
OwningActor, Tween, EntityCollection — 24 bits total), 4 new glyphs
(Goap/Eqs/Tween/EntityCollection), chips clustered under tiny group labels
(Core/Attributes/AI/Gameplay/Rendering) in a scrollable rail. Known gap: Inventory
has no single always-present marker fragment — flag deferred.

**Polish pass 3 (2026-07-10):** (1) Archetype grid density is user-adjustable:
`ArchetypeGridColumns` setting (persisted, Project Settings → Ck ECS Debugger) +
2..6 chips in the page header; SWrapBox → SUniformGridPanel with explicit
(col,row) slotting — true uniform cells, re-slot on density change, card cache
reused. (2) General icon library: 148 glyphs in `Resources/Icons/General/`
(combat/items/nature/creatures/buildings/vehicles/tech/food/people/symbols/
office/tools + Rewind99 flavor: VhsTape, Register, Shelf, FilmReel, Popcorn,
Remote …), generated in the house 16x16 stroke style. Icon registration is now a
directory scan (no hand-list); General/* forms a sorted pool exposed via
`FCkDebuggerStyle::Get_GeneralIconPool`. Card icon resolution:
registered-bespoke → dominant-feature glyph (inferred only) → FCrc-hash pick
from the pool (stable per key) → Cube only if the pool is empty. Registered
archetypes may name any pool glyph directly via descriptor IconSvgPath.

## World-interaction phase — code complete (2026-07-11, Fable e2e, maintainer-approved design)

**Spec:** [docs/specs/2026-07-11-debugger-world-interaction.md](docs/specs/2026-07-11-debugger-world-interaction.md).
Six workstreams (A–F), all code-complete in one session; gate = toolbox build + `Ck.EcsDebugger.*`
specs + the spec §3 `[EDITOR-VERIFY]` table for the maintainer's next PIE round.

| WS | What shipped | Key files |
|---|---|---|
| A | Viewport-picker selections now broadcast on the selection-sync bus (they were applied `Direct` and never re-broadcast — the confirmed root cause of "world-picked entity doesn't show up in Crowd/GOAP"). Crowd/GOAP receives scroll-into-view + 1.2 s fading row flash; Crowd one-shot re-centers the 2D map via new `Request_FrameSelectedAgent` VM signal. | `CkDebuggerModel_ViewportPicker.cpp` (broadcast), both `*_AgentListPanel.*`, `CkCrowdDebugger_ViewModel.h`, `SCkCrowdDebugger_ViewportPanel.*` |
| B | Selection gizmo is now a persistent PMG RGB triad (`FCkDebug_PmgGizmoSet`, pooled procmesh, moved per tick) replacing per-tick `DrawDebugTransformGizmo` — kills the refresh-gate blink and per-tick line re-batching. Transform + SceneNode inspectors swapped (SceneNode also re-homes the parent gizmo on re-parent); Camera inspector untouched. NOT built on PMG `Pivot`: it is a Composite whose child arrows bake world transforms at setup (`CkPmg_Processor_DirectionalShapes.cpp:261-310`) — moving the parent moves nothing; the set creates/re-aims 3 Arrows directly. | `CkDebuggerCommon/Markers/CkDebug_PmgGizmoSet.{h,cpp}`, `CkInspector_Transform.*`, `CkInspector_SceneNode.*` |
| C | `ck::DebugViewportView` (CkDebuggerCommon) now owns ejected-detection / deproject / view-camera-location — picker + overlay duplicates re-pointed. `ck::DebugFocus::Focus_Entity` = editor-F for entities: bounds (ISM mesh → direct actor → 1 m box at transform → recursive actor) framed by pulling the ejected editor camera back along its current view dir (manual fit — no engine `FocusViewportOnBox` dependency). Wired: **F key + context menu** on ECS tree, Crowd list, GOAP list; Focus button in Crowd detail. Possessed = deliberate no-op. | `CkDebuggerCommon/Navigation/CkDebug_ViewportView.{h,cpp}`, `CkDebug_Focus.{h,cpp}` + panel wiring |
| D | Crowd 2D map: player pawn chevron + view-camera FOV wedge (collector samples pawn/camera pose; ejected camera included); `PlayerProxy` status finally assigned (lineage match vs the possessed pawn's entity). RMB on the map = command (auto-arms debug override), RMB-drag still pans (Slate drag-threshold defer), MMB pan unchanged, LMB-command branch removed. In-world: `FCkCrowdDebugger_WorldCommandProcessor` (passive pre-processor, never consumes — consuming would wedge the viewport's RMB camera-look capture) — RMB-click on ground while ejected commands the selected agent + 1.5 s PMG ring ping. "Take Control" button kept as explicit release. | `CkCrowdDebugger_DataCollector.*`, `SCkCrowdDebugger_ViewportPanel.*`, `CkCrowdDebugger/Input/CkCrowdDebugger_WorldCommandProcessor.{h,cpp}`, `SCkCrowdDebuggerWindow.*`, `SCkCrowdDebugger_AgentDetailPanel.cpp` |
| E | ECS inspector gets a pinned owner-chain breadcrumb (root › … › selected; `SCkDebug_EntityRef` pills, depth-capped 8, transient root omitted, rebuilt only on selection change). Crowd rows show a muted `→ Owner` name (pre-sampled in the collector — no per-paint ECS walks); Crowd detail gets an Owner pill row. GOAP already lists owners. | `CkDebuggerPanel_Inspector.*`, `CkCrowdDebugger_Types.h`, `SCkCrowdDebugger_AgentListPanel.cpp`, `SCkCrowdDebugger_AgentDetailPanel.cpp` |
| F | Picker: trace hits on ISM components resolve to the proxy ENTITY via instance-transform match against the marker snapshot (`FHitResult.Item` → `GetInstanceTransform` → nearest `IsmProxy`-carrying entry ≤ 1 m) — deliberately transform-based, no version-sensitive instance-id↔index component API and no renderer-internal poking. Hover on mesh-resolvable entities draws a one-frame bounds box (`DebugFocus::Get_EntityWorldBounds`). New "Meshes First" toolbar toggle (persisted `UCkEcsDebuggerSettings::PickerMeshesFirst`) suppresses diamonds for geometry-pickable entities via the marker `SuppressedEntityNums` mechanism — still gathered, still pickable. | `CkDebuggerModel_ViewportPicker.*`, `CkEcsDebuggerSettings.h`, `CkDebuggerWindow_Main.cpp` |

**Deliberate calls (don't re-litigate without new evidence):**
- Gizmo alpha 0.85 / no wireframe lines; entity label stays `DrawDebugString` (still gate-blinks — separate follow-up if it bothers).
- In-world command processor is ejected-only and passive; if PIE shows an editor context menu popping on RMB command, the recorded fallback is consuming the down+up pair when gates hold at down-time (costs RMB-look only while an agent is selected).
- ISM resolve precision bound: co-located instances within 1 m are ambiguous (nearest wins); exact resolution via `GetInstanceIdForInstanceIndex` recorded as follow-up if it bites.
- ISKM batched clusters: no per-instance resolve this phase — entities keep diamond picking (spec §4 limitation).

**Recorded follow-ups:** re-point `CkCrowdDebugger_DataCollector`'s own ejected-yaw block onto `DebugViewportView` (still uses `bIsSimulatingInEditor`); ISKM bounds for focus (currently 1 m fallback box); consider PMG text label to kill the label blink.

**`[EDITOR-VERIFY]` (maintainer PIE round):** spec §3 table A1–F1 — sync flash + map ping; gizmo stability at low refresh caps + zero orphans after deselect/EndPIE; F-focus framing from all three lists (ejected only); player chevron/wedge; map + in-world RMB command (incl. no stuck camera-look, no context-menu conflict); breadcrumb + owner column; ISM mesh pick + hover box + Meshes First declutter.

## PIE feedback round 1 — code complete (2026-07-11, Fable)

Maintainer PIE-verified the world-interaction phase: **A (sync) and E (owner linkage) passed**;
seven revision items came back (spec §5 has the full verdict table). All code-complete:

| # | Item | Root cause → change | Key files |
|---|---|---|---|
| R1 | In-world RMB command dropped (maintainer accepted): the editor viewport's context menu owns the ejected RMB click, so the passive processor's clean-click never fires usefully. Map-only commanding stays; the map command now draws the in-world PMG destination ring itself. | `CkCrowdDebugger_WorldCommandProcessor.{h,cpp}` **deleted**; window registration removed; ping + `Destination` local added to the map RMB branch; banner/tooltip strings say "map destination". | `SCkCrowdDebuggerWindow.{h,cpp}`, `SCkCrowdDebugger_ViewportPanel.cpp`, `SCkCrowdDebugger_AgentDetailPanel.cpp` |
| R2 | Flat gizmo arrows read edge-on-invisible → real solids with bright outlines. | `FCkDebug_PmgGizmoSet` now builds 6 parts/gizmo (cylinder shaft + cone tip per axis, `UCk_Utils_Pmg_BasicShapes`); `InDrawLines=true` outlines bake as a second procmesh section at alpha 1 (`FProcessor_Pmg_DebugShape_BakeLines`) — retained geometry, moves with the part, cannot blink. Aim = `FRotationMatrix::MakeFromZ(WorldDir)` (both solids build along +Z; `ECk_Plane_Axis::XY` = identity, verified `Get_PlaneAxisRotation`). | `CkDebuggerCommon/Markers/CkDebug_PmgGizmoSet.{h,cpp}` |
| R3 | Focus: force-eject when possessed; glide; 2× distance (tunable). | Possessed → queued focus + `GEditor->RequestToggleBetweenPIEandSIE()` + core-ticker poll until ejected (2 s expiry; `FEditorDelegates::EndPIE` clears the queued handle — handle contract). Framing now `GetViewTransform().TransitionToLocation(...)` (fork-verified EditorViewportClient.h:283/490 — the editor's own animated F). New cvar `ck.Debug.Focus.DistanceScale` (default 2.0) scales the FOV-fit distance. `Get_CanFocus` loosened to "ejected OR PIE exists". Tooltips updated in all three lists + detail button. | `CkDebug_Focus.{h,cpp}` + 4 string sites |
| R4 | Chevron didn't rotate: collector sampled `GetActorRotation().Yaw` — orient-to-movement bodies don't follow the camera. | Now `GetBaseAimRotation().Yaw`. | `CkCrowdDebugger_DataCollector.cpp` |
| R5 | Mesh picking dead in ISKM stress gyms — TWO causes: (1) ISKM entities carry `IskmProxy` (CkIskmRenderer), not `IsmProxy` — every mesh gate was blind; (2) renderer ISMs run `NoCollision` (`CkIsmRenderer_Processor.h:157` param-driven) so the visibility trace can never hit. | `DoIsMeshResolvable` accepts `UCk_Utils_IskmProxy_UE::Has`; new analytic ray-vs-AABB candidate stage in `DoPickAtRay` (slab test w/ near-T) over `_MeshPickBounds` — per-tick cache built in `DoRefreshMarkers` from `DebugFocus::Get_EntityWorldBounds` (collision-independent; nearest-T competes with the trace hit). ISKM pick volume = the 1 m-box fallback (no `Get_MeshBounds` on IskmProxy utils yet — recorded CkFoundation follow-up). | `CkDebuggerModel_ViewportPicker.{h,cpp}` |
| R6 | Transform missing from rail + has/is badges: deliberate exclusion in `Get_BadgeFeatures()`. | Exclusion removed (Label stays excluded — the name column IS the label). Also enables the `has:transform` query token. Transform visual already existed via the inspector's metadata (`CkInspector_Transform.h:11-12` + `Transform.svg`). | `CkEcsDebugger_FeatureVisuals.cpp` |
| R7 | One icon rail needs scrolling → split across both flanks of the tree. | `Build_FeatureRail(bool InRightFlank)`: whole groups greedily assigned to the lighter flank in display order (weight = chips + header); deterministic across both calls; each flank keeps its own scrollbox for short windows. Right flank slotted after the tree. | `CkDebuggerPanel_EntityList.{h,cpp}` |

**Deliberate calls:** in-world commanding could return later behind a modifier key
(e.g. Ctrl+RMB) if wanted — dropped per maintainer, not for technical impossibility. R5 keeps
the physics trace as a precision assist (actor-backed meshes with collision); nearest-T
between trace and box candidates is accepted ranking noise for a debug picker.

**Recorded follow-ups (new):** `Get_MeshBounds` (or bounds getter) on `UCk_Utils_IskmProxy_UE`
in CkFoundation for exact ISKM pick volumes / focus framing (1 m box today); per-row badge
strips near `MaxBadges` now spend one slot on Transform for most entities — revisit the cap if
rows start truncating badges that matter.

**`[EDITOR-VERIFY]` (round 2):** spec §5 re-verify queue — gizmo solids/outlines; focus
auto-eject + glide + distance feel (`ck.Debug.Focus.DistanceScale`); chevron rotation;
map-RMB world ping; ISKM hover/pick + Meshes First in the stress gym; two-flank rail;
Transform chip + badge.

## Session log

- **2026-07-10 (Fable):** Spec + mockup written and reviewed (user + colleague ideas
  incorporated: IS/HAS, archetypes, perf architecture). Campaign tracker created. Phase 0
  implemented: 23 SVGs, style content root + icon brushes + `Get_IconBrush`, inspector
  icon/color metadata plumbing (registry + base interface). Toolbox build launched.
  Docs commit to `dev` pending user approval.
- **2026-07-11 (Fable, world-interaction phase):** Maintainer declared the prior polish passes
  done and pivoted the campaign to world interaction (5 new asks). Design brainstormed +
  approved (4 forks decided: Fable e2e; map+world RTS command; one-shot ejected-only focus;
  trace+ISM picking). Spec `2026-07-11-debugger-world-interaction.md` written; workstreams
  A–F implemented (section above). Backlog untouched: pooling-interaction question,
  inspector fast-path wiring, flag-cache widening, perf acceptance.
- **2026-07-11 (Fable, PIE feedback round 1):** Maintainer's PIE verdicts: A + E pass; seven
  revisions (spec §5, section above) — in-world RMB dropped (editor context menu owns the
  click), gizmo → solid cylinder/cone triad with baked outlines, focus auto-eject + glide +
  distance cvar, chevron aim-yaw fix, ISKM-aware collision-independent mesh picking (IskmProxy
  blindness + NoCollision were the two root causes), Transform badge/chip enabled, icon rail
  split across both tree flanks (the last two were the maintainer's "other comments").
