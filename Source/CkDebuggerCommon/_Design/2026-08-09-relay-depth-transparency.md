# ActorRelay depth transparency (P2 of the Debugger UX campaign)

Design authored 2026-08-09 (Fable session). User decision: first-class entities sometimes live
under an ActorRelay; everything under a relay should be TREATED as depth 0 — relay complexity
hidden by default, revealable on demand. Applies to BOTH the ECS tree and the overlay's depth
reasoning.

## Concept: depth-transparent owners

An owner entity the depth/nesting logic looks THROUGH. v1 has exactly one built-in rule:
an entity whose owning actor is `ACk_ActorRelay_UE` (the exact check
`CkInspector_ActorRelay.cpp` already uses: `TryGet_EntityOwningActor` + `Cast`). No
per-project rule list yet — a bool gate suffices until a second transparent type exists.

## Shared predicate (CkDebuggerCommon — the ONE authority both surfaces call)

New `Public/CkDebuggerCommon/Classification/CkDebug_DepthTransparency.h/.cpp`:

```cpp
namespace ck::DebugDepthTransparency
{
    // True when depth/nesting logic should look through InOwner: the per-user setting is on
    // AND InOwner's owning actor is an ACk_ActorRelay_UE. Safe on invalid handles (false).
    CKDEBUGGERCOMMON_API auto Get_IsTransparentOwner(const FCk_Handle& InOwner) -> bool;

    // The raw relay check without the settings gate (for surfaces that need "is relay"
    // regardless of the transparency toggle, e.g. the tree's affordance row).
    CKDEBUGGERCOMMON_API auto Get_IsRelayEntity(const FCk_Handle& InEntity) -> bool;
}
```

Requires `CkActorRelay` in `CkDebuggerCommon.Build.cs` (Runtime→Runtime T4, legal).

Setting: `UCkDebuggerSettings` (existing per-user class) gains
`bool bActorRelayDepthTransparency = true` (Category "Hierarchy", tooltip explaining both
surfaces). Default ON — the new behavior is the decided default; the setting is the escape
hatch back to literal nesting.

## Overlay / markers (CkDebuggerCommon `Markers/CkDebug_EntityMarkers.cpp` ~:150-186)

In the depth walk, skip the `++Depth` increment for a hop whose `Owner` satisfies
`Get_IsTransparentOwner` (still advance the walk; still respect `MaxDepthWalk` as an iteration
bound). Consequence: relay children count as depth 0 → with `ck.Debug.EntityMarkers.MaxDepth`
default 0 they become focusable/markered by default, which is the point. `OwnerEntityNum`
(used for co-location grouping) keeps the LITERAL owner — only Depth changes meaning.

## ECS tree (CkEcsDebugger `CkDebuggerWidget_EntityTree.cpp`)

- `DoLinkNode`: when the lifetime owner satisfies `Get_IsTransparentOwner` AND that relay is
  not in `RevealedRelays` (new `TSet<FCk_Handle>` member, cleared in `Reset_ForWorldChange`),
  link the node to `RootNodes` and set a new node flag `HoistedFromRelay` (+ `RelayOwner`
  handle for navigation). Otherwise nest as today.
- Hoisted rows render a small muted "via relay" affordance chip after the name (STableRow-safe
  — visual only; the row context menu gains "Reveal relay nesting" which toggles that relay in
  `RevealedRelays` and calls `RebuildHierarchyLinks`).
- The relay's own row (it is transient-owned → already a root) renders compact/muted when its
  children are hoisted: name plus "via ActorRelay — N hoisted" count, an expander-style click
  affordance that toggles `RevealedRelays` for it (reveal = children re-nest under it, row
  returns to normal weight). This is the "expandable affordance" the user asked for —
  per-relay, not global.
- Selection identity: nodes are per-entity and unique — hoisting only changes linking, never
  duplicates nodes, so the stable-TSharedPtr contract holds.

## Classification / rollups (CkEcsDebugger `Classification/`)

Where the tree builds the owner-index array fed to `ComputeRollups` (and any
classification-driven presentation that walks owners), a transparent owner is passed THROUGH:
a relay child's effective owner index = the relay's own owner (transient → root/none). Relay
children therefore become primary roots; internals beneath them still roll up to them; the
status-bar counts shift accordingly. `ComputeRollups` itself is untouched — the pass-through
happens where its inputs are built.

## Specs

The four EcsDebugger specs (`Classification`, `Query`, `Dashboard`, `EntitySelection`) will
shift where they encode literal-nesting assumptions — update them DELIBERATELY (state each
change). Add new coverage: (a) predicate false when setting off; (b) hoist linking — a child
of a transparent owner roots, others nest (if the tree's linking is spec-testable at the
node-model level; otherwise cover the owner-index pass-through, which is pure); (c) rollup
pass-through: internal under relay-child rolls to the child, not lost.

## Style Lab

The affordance chip renders via `ck::debug_axes` (tone Neutral, chip per ChipStyle axis). No
new axis in P2 — transparency is behavior (a settings bool), not styling; revisit if the user
wants it surfaced in the Lab.
