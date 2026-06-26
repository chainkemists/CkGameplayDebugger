# CkEntityDebugOverlay — Improvements Design (2026-06-25)

Status: **IMPLEMENTED** (2026-06-25). All 9 items built (editor compiles + links) and the
7 CkEntityDebugOverlay automation specs pass. Runtime/visual behavior (fan-out layout,
multi-pin strip, ejected input, sub-SM nesting, per-feature pill values) is PIE-verified by
the user — see the session report. Note: item 6's multiple pinned cards stack **vertically**
(not strictly side-by-side) given the 720px default card width.

## Goal

Improve the on-screen entity debug overlay (the in-world diamonds + plates + focus
card driven by `UCk_DebugOverlay_Subsystem`) across nine areas the user raised while
using it in PIE:

1. A tunable range beyond which entity **diamonds** are culled (declutter).
2. A **pill per feature** (not just `PHY`) — e.g. `T` for Transform — covering the
   gameplay features an NPC actually carries.
3. Show **entity IDs** on the world plate.
4. A **max name length** so plates stop ballooning on long names.
5. **Sub-state-machine** info (full recursive descent), which is missing today.
6. **Multiple pinned cards** side-by-side (double-shift), with a release-all.
7. Pin (double-shift) + show-in-ECS-debugger (double-ctrl) must work **while ejected**.
8. **Co-located entity** disambiguation: hover-triggered, non-interactive **fan-out**
   with `i/N` badges, plus a count on the focus card.
9. An always-on **keyboard-hints** strip (expandable), derived from the live bindings.

All work lives in `Plugins/CkGameplayDebugger/Source/CkEntityDebugOverlay` and is
already `WITH_CK_DEBUG_OVERLAY`-gated (compiled out of Test/Shipping). A handful of new
`#include`s reach into feature plugins (CkInventory, CkInteraction, …) for the new
pill providers; those are UncookedOnly-safe in the configs the overlay ships to.

## Architecture orientation (verified by direct read)

Per-frame flow (`CkDebugOverlay_Subsystem.cpp::DoTick`):

1. `Gather_Candidates` — delegates enumeration to the shared `FCkDebug_EntityMarkers`
   snapshot, filtered to "≥1 provider `CanProvide`". **Candidates == markers == plates**
   (one set: "what you see is what you can focus"). Currently passes **no** distance cull.
2. Compute `FViewpoint` (player camera, or editor camera when ejected via F8 — detected
   by `TryGet_LevelEditorViewport`, sets `_ViewpointIsEjected`).
3. Resolve `bIsOnScreen` per candidate (PC `ProjectWorldToScreen`, or dot-product vs
   editor forward when ejected).
4. Pick focus (`_FocusLocked` + `_LockedCandidateIndex`, else `Pick_Best`).
5. Double-tap input via `PC->WasInputKeyJustPressed` for Lock/Cycle/EcsFocus keys.
6. Marker billboards + parent→child links; suppress the possessed pawn's marker.
7. `Build_Model` (focus entity) → `Build_EntityModel` (shared with the ECS picker).
8. `Push_ToRoot` → `Set_FocusCardContent` (one model) + `Build_WorldTags` →
   `Update_WorldTags`.

Key collaborators:

- **Providers** (`ICk_DebugOverlay_Provider`, `Private/Providers/*`): `CanProvide` +
  `Collect` (full focus-card section) + `Get_CompactToken` (one-line pill text).
  Registered via `CK_REGISTER_DEBUG_OVERLAY_PROVIDER`. Existing: EntityInfo, Transform,
  Physics, StateMachine, Goap, FloatAttributes, IntegerAttributes.
- **Pills** (`Build_WorldTags`, `CkDebugOverlay_Present.cpp:216-256`): every provider
  that `CanProvide` auto-emits a colored badge — **except `EntityInfo` and `Transform`**,
  skipped at line 229. Abbreviation via `ck_debugoverlay::Get_ProviderAbbrev`
  (`Tags.cpp:29-56`), which already pre-maps INV/AGRO/OBJ/ANIM/INTR for "providers still
  to be ported."
- **World plates** (`SCkDebugOverlay_Root::DoBuild_NearPlate`): near plates (Dist ≤
  `NearDist`) show a name header + colored badge row; far entities show a single-line
  token pill. De-overlap stacking at `Present.cpp:316-332`. Hard cap 16 tags.
- **Focus card** (`SCkDebugOverlay_FocusCard`): screen-anchored Slate, rebuilt each tick
  from one model; `bIsLocked` draws an amber ring. **Renders fine while ejected** (world
  tags do not — `Present.cpp:185` suppresses them when ejected/`!PC`).
- **Settings** (`UCk_DebugOverlay_Settings`): already has `NearDist`/`FarDist`/`MaxDist`
  (pills only), `DiamondScale`, `SmStateNameDepth`, and the `FKey` bindings
  (`LockKey=Shift`, `EcsDebuggerFocusKey=Ctrl`, `CycleCoLocatedKey=Alt`).
- **Markers** (`FCkDebug_EntityMarkers`, CkDebuggerCommon): `FGatherParams` already
  supports `CullOrigin`/`CullRadius` (unused by the overlay).

---

## Item 1 — Diamond visibility range

**Current:** `Gather_Candidates` sets only `GatherParams.Filter`; `CullOrigin`/`CullRadius`
left default → no distance cull on diamonds. `MaxDist` (5000) is applied only to pills in
`Build_WorldTags`.

**Change:**
- New setting `MarkerMaxDist` (default `3000.0`, `0` = unlimited).
- Reorder `DoTick`: compute the viewpoint **location** before `Gather_Candidates` (it
  depends only on PC/editor camera, not candidates), and pass it down.
- In `Gather_Candidates`, set `GatherParams.CullOrigin = ViewLocation` and
  `GatherParams.CullRadius = MarkerMaxDist` (when `> 0`). The existing cull in
  `FCkDebug_EntityMarkers::Gather` then drops far entities at gather time, shrinking the
  whole candidate/marker/plate/link set consistently.

**Edge cases:** `MarkerMaxDist == 0` → keep unlimited (don't pass a 0 radius, which the
gather treats as "no cull"). Ejected → origin = editor camera location.

**Test:** unit-cover the "compute cull params from settings + viewpoint" helper
(MarkerMaxDist 0 → unset; >0 → origin+radius). Visual confirmation in PIE.

---

## Item 2 — A pill per feature (curated providers)

**Current:** pills auto-emit per `CanProvide` provider; Transform/EntityInfo skipped.
Only 7 providers exist.

**Change:**
- **Un-skip Transform** in `Build_WorldTags` (`Present.cpp:229` → skip EntityInfo only).
  Rename its abbreviation **`XFM` → `T`** (`Tags.cpp:37`). EntityInfo stays skipped (it is
  the name/header itself; a pill would be redundant).
- **Add new providers**, one `.h`+`.cpp` each under `Private/Providers/`, mirroring
  `CkDebugOverlay_Provider_FloatAttributes` (Has-gate in `CanProvide`, cheap
  `Get_CompactToken`, `Collect` rows, `CK_REGISTER_DEBUG_OVERLAY_PROVIDER`). Verified APIs:

  | Provider | Abbrev | `CanProvide` | Compact token |
  |---|---|---|---|
  | ByteAttributes | `bATT` | `UCk_Utils_ByteAttribute_UE::Has_Any` | count via `ForEach` → `B:N` |
  | Inventory | `INV` | `UCk_Utils_Inventory_UE::Has_Any` | `Get_NumItems` → `Inv:N` |
  | Interaction | `INTR` | `UCk_Utils_InteractTarget_UE::Has` | `Get_CurrentInteractions().Num()` → `Int:N` |
  | Aggro (owner) | `AGRO` | `UCk_Utils_AggroOwner_UE::Has` | `Get_BestAggro` → score/target → `Aggro:<n>` |
  | Team | `TEAM` | `UCk_Utils_Team_UE::Has` | `Get_ID` enum → `Team:<id>` |
  | Objective | `OBJ` | `UCk_Utils_Objective_UE::Has` | `Get_Status` enum → `Obj:<status>` |
  | TagSet | `TAGS` | `UCk_Utils_TagSet_UE::Has` | `Get_NumTags` → `Tags:N` |
  | EntityCollection | `COL` | `UCk_Utils_EntityCollection_UE::Has` | `Get_NumEntitiesInCollection` (summed) → `Col:N` |
  | Animation | `ANIM` | `UCk_Utils_AnimPlan_UE::Has_Any` | plan count + current state → `Anim:N` |
  | Timer | `TIMR` | `UCk_Utils_Timer_UE::Has` | active count via `ForEach_Timer` → `Tmr:N` |
  | Label | `LABL` | `UCk_Utils_GameplayLabel_UE::Has` | `Get_Label` tag leaf → `Lbl:<role>` |
  | Variables | `VAR` | `Has_Any` across the 16 `FFragment_Variable_*` | summed count → `Var:N` |

  All getters are const, ensure-free behind their Has-gate, frame-safe. Each provider's
  `.Build.cs` dependency on its feature module is added to
  `CkEntityDebugOverlay.Build.cs`.
- Add the new leaf→abbrev rows to `Get_ProviderAbbrev` and provider colors to
  `SCkDebugOverlay_FocusCard::Get_ProviderColor`.

**Density:** bounded — full badge plates only within `NearDist` (600); far entities get a
single token pill; the 16-tag cap stays; item 1's range cull removes distant clutter.

**Decisions made (opt-out):** included all 12 above; left **EntityTag** out (overlaps the
name). Variables is the only "workable" (not "clean") one — slightly more code (16-type
`Has_Any`). Drop any the user doesn't want.

**Test:** each provider gets a tiny `CanProvide`/token smoke test where a gym entity with
the feature is available; otherwise PIE-verified.

---

## Item 3 — Entity IDs on the plate

**Current:** near-plate header is the clean name, or `#<num>` when unnamed
(`Present.cpp:295-298`). The focus-card model already formats `"%s [%d]"`
(`Present.cpp:67`).

**Change:** near-plate header **always** appends the entity number, reusing the focus-card
format: `Name [4123]`, and `[4123]` when unnamed. One-line change in `Build_WorldTags`.

**Note:** entity *number* only (not `version(raw)`) — matches the focus card and keeps the
plate compact. Full canonical id stays on the focus card via `SCkDebug_EntityRef`.

---

## Item 4 — Max name length

**Change:** new setting `MaxWorldTagNameChars` (default `24`, `0` = unlimited). In
`Build_WorldTags`, clamp the **name segment** of the near-plate header to N chars + `…`
before appending the `[id]`. Scoped to **world-tag plates** (the "too big" ones); the
focus card has room and stays full.

**Edge:** truncate the name only, never the `[id]` (the id must stay readable).

---

## Item 5 — Sub-state-machines (full recursive)

**Current:** `Provider_StateMachine` reads only top-level `FFragment_Sm_Current` (current
state + run status + flat history). No descent; stale `BATCH-VERIFY` notes about named SM
slots.

**Change:** recursive hierarchy in `Collect` (focus card) and `Get_CompactToken`:
- **Upward breadcrumb:** read `FFragment_Sm_ParentHierarchy` (if non-empty) to show
  parent context: `Parent ▸ … ▸ this`.
- **Downward descent:** walk active sub-SMs and render nested rows `▸ Child:State`,
  recursing to `SmMaxRecursionDepth` (new setting, default `3`) with a **visited-set cycle
  guard** (sub-SM entity numbers) so a malformed cycle can't stack-overflow.
- Compact token shows the active leaf chain (clamped), e.g. `SM:Locomotion.Chase ▸ Attack`.

**Open implementation detail (to pin in the plan, not the design):** the exact descent
API — candidates identified are (a) the SM debug cached-task records
(`FCk_SmDebug_CachedTask` carries `HasSubStateMachine` + `SubSmHandle`) and (b) the
`TUtils_Sm_OwningStateMachine` entity-holder walk. The plan will read both and choose the
one that enumerates *active* sub-SMs without an editor-only dependency.

**Test:** a SM gym with a known nested sub-SM; assert the provider's rows/token contain the
child state and respect the depth clamp; a synthetic cycle asserts the visited-set guard
terminates.

---

## Item 6 — Multiple pinned cards side-by-side

**Current:** one focus card; double-tap `LockKey` toggles `_FocusLocked`; `Push_ToRoot`
pushes one model.

**Change:**
- Subsystem: `TArray<FCk_Handle> _PinnedEntities`.
  - **Double-tap Shift now pins/unpins the focus entity** (add if new, remove if present).
    `_FocusLocked` is retained for Next/Prev/co-located/console `Lock` (the primary card),
    but Shift no longer toggles it.
  - **Release all:** console command `ck.DebugOverlay.UnpinAll` + an optional bindable
    `UnpinAllKey` (default `EKeys::Invalid` / unbound).
  - Prune invalid handles each frame (destroyed pins drop). Dedupe: skip a pinned card that
    equals the current primary focus this frame (avoids showing it twice).
- Root: replace the single card slot with a **card strip** (an `SHorizontalBox`/wrap of
  `SCkDebugOverlay_FocusCard`), `_FocusCard` (primary, live) + `TArray<…> _PinnedCards`,
  anchored to the same corner. New `Set_PinnedCards(TArray<FCk_DebugOverlay_EntityModel>,
  style, history, now)`. Pinned cards are **live-updating** and get a distinct ring tone
  (vs the amber primary lock).
- `Push_ToRoot`: build the primary model + one model per valid pinned entity; push both.

**Adversarial:** pins are `FCk_Handle` (stable across frames, unlike candidate indices);
reset on deactivate (PIE stop). Strip width: many pins could exceed the viewport — wrap and
cap (e.g. 6 pinned cards) with a "+N more" note rather than overflow.

---

## Item 7 — Pin / ECS-debugger work while ejected

**Root cause (confirmed):** all three double-taps (`Subsystem.cpp:489/521/563`) use
`PC->WasInputKeyJustPressed`, gated by `if (ck::IsValid(PC))`. When ejected (F8) keyboard
input routes to the editor viewport, so the PC never sees the key — markers still draw and
focus follows the editor camera, but input is dead.

**Change:** a global **Slate input pre-processor** (`IInputProcessor`) registered in
`DoActivate`, unregistered in `DoDeactivate`, owned only by the primary instance (mirror
`_bIsPrimaryConsoleOwner`). It records key-down events into a small `TSet<FKey>`
"pressed-since-last-poll" drained by `DoTick`, and **never consumes** input (handlers
return `false`). This observes keys regardless of game-vs-editor viewport focus, unifying
all three double-tap detections in both possessed and ejected modes. Replaces the
`WasInputKeyJustPressed` calls (single source — no double-fire).

**Edge:** also fixes the co-located cycle key when ejected. Number/scroll input for item 8
rides the same pre-processor.

**Test:** the pre-processor's "drain pressed keys" buffer is unit-testable
(press→poll→empty); the ejected behavior itself is PIE/editor-only.

---

## Item 8 — Co-located fan-out (non-interactive) + count

**Goal:** when the focused diamond is actually several overlapping entities, *see* them
and know which one you're on — without blind cycling.

**Change:**
- **Screen-space cluster detection:** group on-screen candidates whose projected screen
  positions fall within `CoLocatedScreenRadius` px (new setting, default `36`). Replaces
  the vertical-only de-overlap (`Present.cpp:316-332`) for clustered groups.
- **Hover-triggered fan-out:** when the **focus** entity belongs to a cluster of N>1, splay
  that cluster's plates apart (small arc/vertical fan from the cluster centroid) with short
  leader lines back to the shared world point. Non-focused clusters render compact as today.
- **`i/N` badges:** each fanned plate shows its index `2/5`; the **focus card** also shows
  `i/N` in its header when the focus is in a cluster.
- **Selection unchanged:** the existing double-Alt cycle moves focus through the cluster —
  now switched to iterate the same **screen-space** cluster so "what you see fanned == what
  you cycle." (The world-space `CoLocatedRadius` setting is retained only as a fallback.)
- **Non-interactive:** no number-key/click selection — the fan-out is pure visual feedback.

**Adversarial:** large clusters (crowds) — cap the fan to a sane count (e.g. 8) and badge
`1/12 (+4)`; fan spread must stay on-screen (clamp near viewport edges).

**Test:** the clustering + index-assignment is a pure function (screen positions → groups →
`i/N`), unit-testable. Fan layout/visuals are PIE-verified.

---

## Item 9 — Keyboard-hints strip (always-on + expandable)

**Change:** a new lightweight Slate widget in the Root, anchored to the corner **opposite**
the focus card (focus card default top-right → hints bottom-left).
- **Always-on compact strip:** one dim line of the core actions, e.g.
  `⇧⇧ pin · ⌃⌃ ECS · ⎇⎇ cycle · [/] layout`, built from the **live `FKey` settings** (never
  hardcoded), so rebinds reflect immediately.
- **Expandable legend:** a hotkey/console (`ck.DebugOverlay.Help`, plus a bindable
  `HelpKey`) toggles a full legend (all bindings + console commands + the new pin/unpin and
  fan-out behaviors).
- New setting `ShowKeyHints` (default `true`) to hide the strip entirely.

**Test:** the "settings → hint strings" formatter is unit-testable (binding → label);
placement/visibility PIE-verified.

---

## New / changed settings (`UCk_DebugOverlay_Settings`)

| Setting | Default | Purpose |
|---|---|---|
| `MarkerMaxDist` | `3000.0` | diamond/candidate hard cull range (`0` = unlimited) |
| `MaxWorldTagNameChars` | `24` | world-plate name truncation (`0` = unlimited) |
| `SmMaxRecursionDepth` | `3` | sub-SM descent depth clamp |
| `UnpinAllKey` | unbound | optional hotkey for release-all |
| `CoLocatedScreenRadius` | `36` | px radius for screen-space cluster grouping |
| `ShowKeyHints` | `true` | show the always-on key-hints strip |
| `HelpKey` | unbound | optional hotkey to toggle the full legend |

Transform abbreviation changes `XFM` → `T` (`Tags.cpp`). All settings live-applied like the
existing ones.

## Cross-cutting risks / adversarial review

- **Lifetime:** pins as `FCk_Handle`, pruned on invalidation, reset on deactivate. Pinned
  card deduped against the live primary focus.
- **Order of operations:** viewpoint computed before gather (item 1); no candidate
  dependency, so the reorder is safe.
- **Re-entrancy / cycles:** SM recursion clamped + visited-set guarded; the Slate
  pre-processor observes only (returns `false`), registered once.
- **PIE vs ejected:** input now via the pre-processor (works both); world tags still
  suppressed when ejected by design, but cards + key-hints + fan-out are screen-anchored and
  render ejected.
- **Density vs. the declutter goal:** more pills could re-clutter; mitigated by the range
  cull (item 1), `NearDist` gating of full plates, the 16-tag cap, and name truncation.

## Testing strategy

- **Automation (`*.spec.cpp`, run via the Unreal Toolbox):** pure helpers — marker cull
  params, name truncation, SM recursion + cycle guard, co-located clustering/`i/N`,
  pre-processor key-buffer drain, hint-string formatting, and per-provider `CanProvide`/token
  smoke tests. Capture the baseline pass/fail set **before** changes; re-run after; report
  the delta.
- **PIE/visual (user-verified):** diamond range, fan-out layout, multi-pin strip, ejected
  pin/ECS-focus, key-hints placement. These are the claims only the user can confirm
  on-screen.

## Out of scope / future

- Interactive (click/number) selection of co-located entities (kept non-interactive per
  request).
- Diamond-level fan-out (this design fans the plates; markers stay put).
- Per-feature focus-card section richness beyond the compact token for the new providers
  (the `Collect` rows can be expanded later).
