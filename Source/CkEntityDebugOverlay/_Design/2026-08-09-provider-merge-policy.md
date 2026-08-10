# Provider-aware merge behavior for the focus card (P6 of the Debugger UX campaign)

Design authored 2026-08-09 (Fable session), from recon verified against dev + f1529a2.
Brief: the ×N merge (f1529a2) must not lose per-sub-entity information for providers where
every row matters (GOAP named by the user). "Either they all need to be displayed OR their
information condensed for all the sub-entities" — resolved as a per-provider policy.

## Problems with the current pass (all recon-verified)

1. Merge key = `Provider|FieldTag|Value` across ALL sources (`CkDebugOverlay_Present.cpp:241-272`).
   For every provider except Label/Team, identical values on different sub-entities are
   semantically distinct facts — merging destroys them (GOAP idle sub-planners, attribute
   values, SM states, timer summaries…).
2. The survivor keeps its own section's `SourceName`/`SourceEntityId`, so the ×N row renders
   under one arbitrary sub-entity's source chip — active mis-attribution.
3. The key ignores `Severity` and `ExplicitHistory`: rows with identical `(N entries)` values
   but different history trails collapse; only the survivor's trail renders (StateMachine).

## The design — `ECk_DebugOverlay_MergeBehavior`

New enum (Model header), one value per provider via a new virtual on
`ICk_DebugOverlay_Provider` (sits next to `Get_CollectsFromSubSources()`,
`Provider/CkDebugOverlay_Provider.h:53`):

| Behavior | Meaning | Assigned to |
|---|---|---|
| `MergeWithinSource` (**new default**) | Duplicate `(FieldTag, Value)` rows collapse only inside one source's section (genuine redundancy). Cross-source rows NEVER merge — "all displayed", each under its own source chip. | Every provider not listed below (attributes family, Timer, Inventory, Objective, Jolt, PathNetworkFollower, Crowd, Aggro, AnimPlans, Physics, EntityCollection, InteractTarget, Variables, TagSet…) |
| `MergeAcrossSources` | Today's ×N behavior — value is subtree-wide, duplicates are true noise. | **Label, Team** only (the recon's only SAFE classifications) |
| `CondensePerSource` | Focus entity's own section stays full; all sub-source sections of this provider collapse into ONE section with one summary row per sub-entity. | **Goap, StateMachine** (hierarchical/noisy) |

"NeverMerge" is deliberately absent: `MergeWithinSource` IS never-merge across sources, and
within-source duplicates are true duplicates by construction (one section per provider×source).

## Mechanics

**Stamping (keeps the model pass pure).** `FCk_DebugOverlay_Section` gains
`ECk_DebugOverlay_MergeBehavior MergeBehavior` (default `MergeWithinSource`), stamped in
`CollectInto` (`Present.cpp:123-130`) from the provider virtual — same spot that sets
`SourceName`/`SourceEntityId`. `Prepare_FocusCardModel` stays model→model; specs keep
hand-building sections.

**Condense pass** runs in `Build_EntityModel` right after section collection (provider pointer
is live there), BEFORE `Prepare_FocusCardModel`, gated by the same `bMergeDuplicateRows`:
- For each provider with `CondensePerSource`: the provider's **first section in
  BFS/SourceOrder order stays full** — for a focus entity that provides directly, that is its
  own section; for hierarchical features whose fragments live on child entities (GOAP planner,
  SM — their `CanProvide` is never true on the focus NPC itself), BFS nearest-first makes the
  first section the PRIMARY planner/SM, which must keep its full detail. The remaining
  sections are replaced by one section {same ProviderTag/SortPriority,
  `SourceName = "N sub-entities"`, `SourceEntityId` = focus id (stable history bucket),
  `SourceOrder` = min of collapsed} holding one row per collapsed source.
- Per-source summary row comes from a new provider virtual
  `Get_CondensedSourceRow(SourceName, Rows) -> FCk_DebugOverlay_Row` with a generic default:
  first row's FieldTag; `Value = "SourceName: v1 · v2 · …"` (non-empty values joined);
  Severity = max of the source's rows.
  - **Goap override**: idle → `"Name: Idle"`; otherwise `"Name: Status — ActiveAction"` with
    the plan string as the row's `ExplicitHistory` (renders as the existing muted breadcrumb).
  - **StateMachine override**: `"Name: CurrentState"`, the sub-SM's transition trail as
    `ExplicitHistory`.
- Testability seam: the collapse itself is a pure free function
  `Condense_ProviderSections(Sections, CondenseFn)` in the Presentation namespace so specs can
  exercise it without live providers.

**Merge pass rework** (`Prepare_FocusCardModel`), still gated by `bMergeDuplicateRows`:
- Pass 1 — per-section dedup (all behaviors): identical `(FieldTag, Value)` within a section →
  `MergedCount`, Severity = max.
- Pass 2 — cross-section merge ONLY between sections whose `MergeBehavior == MergeAcrossSources`
  (same key as today). Survivor attribution unchanged (Label/Team values are source-agnostic,
  so the old spec assertion stays meaningful).
- **History guard (both passes): a row with non-empty `ExplicitHistory` never merges** — trails
  are never identical-by-value evidence.
- Strip pass, budget, legend: unchanged. GOAP/SM remain budget-protected
  (`FocusCardBudget.cpp:5-10`); a condensed section costs 1 slot per sub-entity instead of 4.

**Settings/axes:** `bMergeDuplicateRows` stays the single master gate (off = raw, all
behaviors moot). The Style Lab `MergeCountDisplay` axis keeps styling the ×N; no new axis.

## Spec plan (extend `Private/Tests/`)

- `CkDebugOverlay_FocusCardModel.spec.cpp`: rework case (a) to mark sections
  `MergeAcrossSources`; add — within-source dedup collapses + counts; cross-source with
  `MergeWithinSource` does NOT merge; severity max on merge; history-carrying rows never merge.
- New condense coverage via the pure `Condense_ProviderSections` seam: N sub-sections → 1
  section, row-per-source, focus section untouched, SourceOrder/bucket rules.
- `CkDebugOverlay_ProviderCompatibility.spec.cpp`: census test asserting each registered
  provider's declared `Get_MergeBehavior()` against the table above (launcher-catalog pattern) —
  a new provider must consciously pick a behavior.

## Acceptance

- Label/Team keep the ×N declutter (P0's win preserved).
- GOAP: focus entity's planner section full; sub-planners = one compact line each, nothing
  mis-attributed. `[EDITOR-VERIFY]` in PIE.
- StateMachine: no cross-sub-SM history loss (guard + condense).
- Gates: overlay spec patterns green; Classic look otherwise unchanged.
