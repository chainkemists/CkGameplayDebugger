# CK ECS Debugger — Entity Browsing Redesign (Design Spec)

**Date:** 2026-07-10 · **Status:** awaiting review · **Visual contract:** [`ecs-debugger-redesign-mockup.html`](ecs-debugger-redesign-mockup.html) (self-contained; open in any browser)

## 1. Problem & goals

At 221 entities (gym scale) the entity tree is already hard to scan; at Rewind99 scale
(~1,000–5,000 entities: NPCs × their sub-entities) it is unusable. Root causes, observed:

- Feature plumbing (Timers, SceneNodes, Probes, Attributes) renders as peer rows of the
  gameplay entities that own them.
- Identical siblings (118 NPCs, 8 cube gyms) repeat as visually indistinguishable subtrees.
- Names degrade (`SceneNode(Cube_ECk_SceneNode(Cube_EC…`, `Timer: No Name Specified`).
- Every row looks the same: dot + text + ID pill. Scanning requires reading.
- Collapsed nodes carry no information scent; the center pane under-delivers.

**Goals** (all four workflows, in priority order): find one specific entity fast; understand
one entity deeply; survey what exists; watch live behavior. Must hold up at **5,000 entities
under the existing 0.1 s refresh cadence**.

**Non-goals (v1):** AS-typed archetype accessors (data-tier AS authoring IS in scope);
pin persistence across editor restarts; multi-world aggregate views; watch expressions.

## 2. Design summary — the twelve ideas

The developer thinks in *gameplay things* and *features*; the tree shows raw entities.
Two structural moves fix that, ten supporting ideas complete it:

1. **Today ⇄ Concept parity** — every change below is presentation; classic behavior stays
   reachable via toolbar toggles (fold + group off ≈ today's tree).
2. **Internals fold** — feature-plumbing sub-entities collapse into a `⊞ N` chip on their
   owner's row; click to peek (grouped by feature, e.g. "Timer ×2"); global toggle reveals all.
3. **Sibling coalescing** — >2 same-archetype siblings render as one `Name ×N` group row;
   expand on demand; filtered searches look inside and report "k match".
4. **One icon language** — every registered inspector gets a glyph + color; reused
   identically in tree badges, feature rail, archetype cards, inspector section headers.
5. **Entity identity = dominant feature** — a SceneNode-only entity renders as a SceneNode
   (icon replaces the generic dot; name simplifies to role).
6. **Computed names** — role + context instead of raw constructor nesting; `Timer — Attack
   Cooldown` when named, dimmed `Timer (unnamed)` when not; raw name in tooltip/Copy/inspector.
7. **Query tokens** — `has:` `is:` `net:` `id:` `arch:` compose with fuzzy text; Filter
   narrows, Highlight dims (both retained from today's DualSearchBar).
8. **Feature rail** — visible chip per feature with live count; click cycles
   require → exclude → off. (Promotion of the existing InspectorFilter model to a
   first-class surface.)
9. **Pinned & Recent** — pin entities to a top section (session-scoped v1); Recent auto-fills
   from the existing selection history.
10. **Overview + Activity** — center pane becomes an archetype population map (cards with
    count + sparkline + click-to-drill, family bars, total hero) plus an Activity feed of
    spawn/destroy/state events with click-to-jump; spawn/destroy flash in the tree.
11. **IS vs HAS** — solid badge = fragment on the entity; hollow badge + count = feature on
    its internal sub-entities (rollup). Queries mirror it: `is:attr` vs `has:attr`.
12. **Game-defined archetypes** — games register named feature-amalgamations with bespoke
    icons (Rewind99 Shelf); registered archetypes beat inference for grouping, icons, cards,
    `arch:` queries. C++ tier additionally yields a typed accessor struct.

## 3. Semantics

### 3.1 Entity classification (primary vs internal)

An entity is **internal** iff both hold:

- Its **own** feature signature is minimal: own features ⊆ { X, Transform, Label } for a
  single non-structural feature X; and
- X is in the **internal-feature set**, seeded with Timer, SceneNode, Probe, FloatAttribute /
  ByteAttribute / IntegerAttribute (record entries), Cue relay — and extensible in Project
  Settings → Ck ECS Debugger (same pattern as the existing exclusion list).

Everything else is **primary**. Classification is computed once per tree rebuild and memoized
per entity id+version; the internal-feature set is data, not code, so misclassifications are
user-fixable without a rebuild of the plugin.

`is:primary` / `is:aux` query tokens and the fold toggle key off this classification.

### 3.2 HAS rollup

For each primary entity: rollup = multiset of internal-feature types over its internal
sub-entities, walking **through internals but stopping at primary descendants** (a Station
does not claim its Display child's timers; the Display has its own rollup). Attributes count
via their record entries. Computed bottom-up in the same rebuild pass, O(n).

Rendering: solid badges = own features (minus Transform, which is ubiquitous); hollow badges
with counts = rollup types not already solid. Overflow truncates with `+k`.

Query: `has:x` matches own ∪ rollup; `is:x` matches own only.

### 3.3 Archetypes

**Home: CkEcs (CkFoundation).** Two tiers over one registry:

- **Descriptor (data tier)** — `FCk_ArchetypeDescriptor`: `Name` (FName, e.g.
  `Rewind99.Shelf`), `DisplayName`, `FeatureIds` (FNames), optional `RequiredLabel`
  (gameplay label), optional `NamePattern`, `IconSvgPath` (relative to owning
  plugin/project `Resources/`), `Color`, `Priority`. Registered natively or via
  `UCk_ArchetypeDefinition : UDataAsset`, discovered through the asset registry (existing
  `UCkAssetRegistryConfig` pattern) — no manual registration call.

  **AngelScript authoring is the primary path for game archetypes** (a game's archetypes
  are expected to live entirely in its `Script/` folder). `Get_FeatureIds` is deliberately
  a `BlueprintNativeEvent` accessor rather than a plain `TArray` UPROPERTY because AS
  cannot brace-initialize TArray defaults — overriding imperatively is the codebase's
  established workaround:

  ```angelscript
  // Rewind99/Script/Archetypes/R99_Archetypes.as
  class UR99_Archetype_Shelf : UCk_ArchetypeDefinition
  {
      default Name        = n"Rewind99.Shelf";
      default DisplayName = "Shelf";
      default IconSvgPath = "Icons/Shelf.svg";   // resolved against the game plugin's Resources/
      default Color       = FLinearColor(0.85f, 0.65f, 0.28f);
      default Priority    = 10;

      UFUNCTION(BlueprintOverride)
      TArray<FName> Get_FeatureIds() const
      {
          TArray<FName> Features;
          Features.Add(n"Transform");
          Features.Add(n"Inventory");
          Features.Add(n"GameplayLabel");
          return Features;
      }
  }

  asset R99_Archetype_Shelf_DA of UR99_Archetype_Shelf {}
  ```

  AS code can also query at runtime (`utils_archetype::Get_Matches(Entity,
  n"Rewind99.Shelf")`); typed AS accessors remain a v1 non-goal. Phase A validates the
  authoring ergonomics end-to-end with a real test asset before the debugger consumes it.
- **Typed amalgamation (code tier, C++)** —
  ```cpp
  USTRUCT(BlueprintType)
  struct FCk_Archetype_Shelf
  {
      GENERATED_BODY()
      UPROPERTY() FCk_Handle_Transform     Transform;
      UPROPERTY() FCk_Handle_Inventory     Inventory;
      UPROPERTY() FCk_Handle_GameplayLabel Label;
  };
  CK_DEFINE_ARCHETYPE(FCk_Archetype_Shelf, "Rewind99.Shelf", (Transform)(Inventory)(Label));
  ```
  The macro generates `static TryCast(const FCk_Handle&) -> TOptional<FCk_Archetype_Shelf>`
  (all-or-nothing member casts) and auto-registers the equivalent descriptor.
  **Open verification:** whether typesafe-handle machinery already exposes a uniform
  member-type→Cast trait; if not, one trait template is added in CkEcs.

**Resolution split (keeps CkEcs dependency-free):** CkEcs owns storage + API only.
Consumers resolve matching: the debugger maps `FeatureIds` to bits in the feature-flag
cache (§5) so a match is `(bits & required) == required`; native registrations may supply
a direct `TFunction<bool(const FCk_Handle&)>` matcher (the macro wires TryCast in).
Precedence when several match: highest `Priority`, then most required features, then
registration order.

**Debugger consumption:** registered archetype beats name-inference for the group key,
row/card icon, color, and `arch:` token. Fallback (unregistered entities): inferred
archetype = cleaned base name + own-feature signature.

### 3.4 Computed names

Pipeline per entity (concept mode): registered archetype DisplayName → else cleaned raw
name (existing `ck::DebugNameClean` strip patterns) → internals get role names
(`Timer — <name>` / `Timer (unnamed)` dimmed / `Scene Node`). Raw name is always available
in tooltip, right-click Copy, and the inspector's Entity Info row. Today-mode renders raw
names untouched.

### 3.5 Query grammar

`<term> ::= has:<feat> | is:<feat|primary|aux> | net:<auth|proxy|none> | id:<n> |
arch:<substr> | <fuzzy-text>`; terms AND-compose; Filter input hides non-matches (groups
auto-expand up to 20 survivors and show "k match"), Highlight input dims non-matches.
Feature-token matching prefixes against inspector IDs and display names, case-insensitive.
Rail include/exclude composes with the query (rail exclusion wins, mirroring today's
exclusion-overrides-filter rule).

### 3.6 Overview & Activity

- **Overview:** total hero + 60-sample count sparkline; population-by-family bars
  (single-hue, direct-labeled); one card per archetype (icon, count, signature badges,
  sparkline, GAME tag for registered game archetypes); singletons list. Card click →
  hierarchy lens filtered to `arch:<name>` with the group expanded.
- **Activity:** derived **debugger-side by diffing consecutive entity-cache snapshots**
  (spawn/destroy) — no CkEcs hooks required. v1 commits to spawn/destroy rows only;
  state-machine transition rows are a stretch item behind the same feed API and ship only
  if the SM debugger's existing transition history is cheap to tap. Feed rows click-to-select;
  unseen-count badge on the tab; spawn/destroy flash rows green/red in the tree for one
  refresh interval.

## 4. UI contracts (binding, from CkDebuggerCommon/CLAUDE.md)

- Row items keep **stable TSharedPtr identity** across refreshes (reuse-by-key; refresh
  only on set change). Group nodes key by archetype name; entity nodes by handle.
- **No click-consuming widgets in `STableRow`** except bounded pills (EntityRef pattern);
  the `⊞ N` internals chip and badge strip must trap clicks only within their own bounds.
- Refresh honors `FCkDebuggerRefreshGate`; classification/rollup memoized per rebuild.
- All cached `FCk_Handle`s cleared on `EndPIE`; Slate teardown on `OnEnginePreExit`.
- No brush allocation in hot paths — all icons registered in `FCkDebuggerStyle` (SVG via
  `FSlateVectorImageBrush`, monochrome white, tinted per-feature at draw via
  `SImage.ColorAndOpacity`; stroke-to-outline converted assets, ~20 in plugin
  `Resources/Icons/`). Game archetype icons load from their owning plugin's Resources at
  definition registration, with dominant-feature glyph fallback.
- Every informational text remains copyable per the existing copy policy; group rows'
  context menu adds "Copy all N names/IDs".

## 5. Performance architecture & budget

The debugger measurably slows large maps **today**. Code-confirmed hot spots that this
campaign eliminates (they run per refresh, up to 10 Hz, on the game thread):

- `BuildEntityTree` empties and re-allocates every node each refresh (also violating the
  stable-`TSharedPtr` contract, forcing `STreeView` row regeneration).
- `ApplyFilterToNodes` derives `Get_DebugName` strings per entity per pass.
- `FCkDebuggerModel_InspectorFilter::Test_Entity` instantiates fresh inspector objects
  per inspector × per entity × per refresh when a filter is active.

**Fix architecture (binding):**

1. **Feature-flag cache** (CkEcs, Phase A): a registry-context bit table (dense array
   indexed by entity id — not a per-entity fragment) maintained by EnTT `on_construct` /
   `on_destroy` signals on each feature's stable *marker fragment* (Params/Current — never
   request/transient tags). Cvar-gated: signals connect when debugging activates (one O(n)
   seeding scan), disconnect after; zero cost when off. Consumers: inspector-filter tests,
   classification, rollup, and archetype matching — which compiles to
   `(bits & required) == required`. Inspector objects instantiate only for the selected
   entity's panel.
2. **Incremental model:** the world model applies spawn/destroy deltas (same diff that
   feeds the Activity tab) and reuses node `TSharedPtr`s by key; full rebuild only on world
   change or manual refresh. Display/cleaned names computed once per node, cached.
3. **Per-archetype shared work:** group members share signature/badge computation;
   collapsed groups do per-group work. Per-refresh cost tracks visible rows, not entity
   count (≤ 30 instance rows materialize per expanded group before "Show all").
4. **Refresh budget:** on top of `FCkDebuggerRefreshGate`, a per-refresh time budget —
   oversized delta queues spread across frames. Manual-refresh mode remains the escape
   hatch for pathological maps.

**Acceptance gates** (verified with Unreal Insights traces, not estimated): steady state
with the debugger open and no entity-membership churn performs **zero O(n) work**; Phase 0
captures a baseline trace on a large map, and Phases 1–2 must show the hot spots above
removed against that baseline. Sparkline buffers are fixed 60 samples; no per-frame work
when the tab is hidden (existing gate).

## 6. Phases (each ends: toolbox build green + debugger fully usable)

| Phase | Scope | Repo |
|---|---|---|
| **0** | SVG icon assets; `FCkDebuggerStyle` brush registration; inspector metadata gains icon + color (extends `FCkDebuggerInspectorMetadata` / InspectorFilter's color map); **perf baseline trace on a large map** | CkGameplayDebugger |
| **A** | **Feature-flag bit cache** (signal-maintained, cvar-gated) + archetype descriptor + registry + `UCk_ArchetypeDefinition` asset + AS authoring validation | CkFoundation (CkEcs) |
| **1** | Classification model (primary/internal), rollup, computed names, query tokenizer — pure logic, automation-spec tested; registry-first archetype keying; **incremental world model (spawn/destroy deltas, stable node identity, cached names); `Test_Entity` → bit tests** | CkGameplayDebugger |
| **2** | Tree rework: fold + `⊞` chip, coalescing, solid/hollow badges, identity icons, status-bar counts; **Insights trace vs Phase 0 baseline showing hot spots removed** | CkGameplayDebugger |
| **3** | Query bar tokens + help popover; feature rail; Pinned/Recent sections | CkGameplayDebugger |
| **4** | Overview page (cards/families/sparklines/click-through); Archetypes lens | CkGameplayDebugger |
| **5** | Activity feed (cache-diff), churn flashes, tab badge | CkGameplayDebugger |
| **B** | `CK_DEFINE_ARCHETYPE` typed struct + TryCast (+ cast trait if needed) + unit specs | CkFoundation (CkEcs) |

Phases 0/A/1 have no user-visible risk; the debugger changes appearance from Phase 2 on.
Any phase can ship alone; B can slip without affecting the debugger.

## 7. Verification

Per phase: toolbox build; automation specs for pure logic (tokenizer, classification,
rollup, group keying, archetype precedence — Phase 1/A/B); manual PIE checklist (fold/group
toggles, selection stability under refresh, EndPIE crash check, 5k-entity gym stress map)
recorded in the phase log. Baseline before Phase 2: current debugger behavior + existing
test suite green.
