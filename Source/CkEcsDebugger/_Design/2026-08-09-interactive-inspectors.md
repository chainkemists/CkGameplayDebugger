# Interactive inspectors — curated feature requests from inspector rows (P7 of the Debugger UX campaign)

Design authored 2026-08-09 (Fable session) from a full request-surface census of all 48
registered inspectors (47 `CK_REGISTER_DEBUGGER_INSPECTOR` + the GOAP gateway; the oft-quoted
"49" is wrong). Brief (user): "you see what is supported in the feature and whatever _can_ be
easily added to the Inspector IS added" — e.g. Transform editable. No complex requests.

## Ground rules

1. **Public Utils `Request_*`/`Set_*` only** — never fragment mutation. Handle idiom (from the
   existing precedent `CkInspector_Probes.cpp:145-165` and GOAP's agent column): copy the const
   handle, `Cast`, capture **by value**, `ck::IsValid`-check on every fire.
2. **Curation tiers from the census**: implement EASY (arg-free buttons, single
   scalar/bool/enum/name/tag) and cheap MODERATE (request struct wrapping 1–3 plain fields).
   SKIP tier stays skipped (asset refs, arrays, nested payloads, index-addressed APIs).
3. **Deliberately excluded** (census rationale): Jolt body/character writes (mid-frame physics
   pokes masquerade as gameplay bugs), Inventories' authority-heavy operations beyond the three
   easy item-trait calls, `Request_DestroyEntity` (destructive; no confirm affordance exists),
   SM `Request_Transition` (only a `TSubclassOf` API exists). Logged as deferred, not forgotten.
4. **Precedent to reuse, not reinvent**: GOAP `SCkGoapDebugger_AgentColumn.cpp` is the reference
   implementation — switch :1019, combo :1043 (ignores `ESelectInfo::Direct`), numeric editor
   :915, flat buttons :1139-1143, lock-icon for non-editable neighbors :997.

## Foundation (U0 — blocks the waves; sequenced AFTER P5-U1 since both touch CkDebuggerCommon)

- **Promote `SCkDebug_NumericEditor`** to CkDebuggerCommon from GOAP's file-local
  `MakeNumericEditor` (commit-on-enter/lost-focus, not per-keystroke). GOAP swaps over.
- **Builder vocabulary** on `FCkInspectorWidgetBuilder` (composing common widgets;
  `AddWidgetRow` already hosts arbitrary widgets so these are conveniences with consistent
  styling + axis awareness):
  - `AddActionRow(Label, {ButtonLabel, Tooltip, Callback}...)` — flat buttons.
  - `AddToggleRow(Label, Get, Set)` — `SCkDebug_Switch`.
  - `AddNumericRow(Label, Get, Commit)` float/int variants; `AddVectorRow`/`AddRotatorRow`
    (three aligned numeric editors, axis-colored per P3's RGB convention).
  - `AddEnumDropdownRow(Label, Options, Get, Set)` (combo; ignore `Direct`).
  - `AddNameEntryRow` / `AddTagEntryRow` (text entry; gameplay-tag picker only where the
    editor-only widget is available — gate `WITH_EDITOR`, fall back to text entry).
- **Rebuild guard** (the one real UI risk): a rebuild from `SCkDebuggerPanel_Inspector::Tick`
  (:263) destroys widgets mid-drag/mid-type. Builder-owned focus tracking: interactive rows
  register focus/capture state into a shared `FCkInspectorEditGuard`; the panel skips the
  rebuild (defers, doesn't drop, the dirty flag) while `Get_HasActiveEdit()`. Numeric editors
  commit on enter — never per keystroke.
- **Authority gate helper** `ck::DebugRequestGate`: per-control
  `{bEnabled, DisabledReason}` from the inspected handle's world net mode vs the request's
  requirement (`AuthorityOnly` / `CosmeticOnly` / `LocalOk`). Disabled controls render greyed
  WITH the reason tooltip — never a button that silently ensure-and-drops (SM/Aggro/
  MontagePlayer/Inventories are processor-enforced AuthorityOnly; TagSet is inconsistent —
  Remove gated, Add not; attributes are symmetric but server-stomped → tooltip "local only,
  server projection will overwrite" on non-auth worlds).
- **Stale-frame note**: most requests drain one frame later — controls display via the existing
  live value-getter rows, no write-then-read assumptions. Immediate exceptions per census
  (SceneNode Detach, Timer direction, ContextOwner, Probe Set_DebugInfo) need no special
  handling.

## Waves (Opus units after U0; per-inspector content = the census shortlist, EASY + cheap MODERATE)

- **W1 — proof + flagship**: Timer (6 buttons + direction dropdown + jump/consume numerics),
  **Transform** (SetLocation/Rotation/Scale via axis-colored vector rows + Local/World dropdown,
  AddOffset rows, ForceRefresh; honor the scale supersede rule `CkTransform_Utils.h:191`),
  Probes (visible debug-draw toggle + Set_DebugInfo colors/thickness + EnableDisable),
  Tween (Pause/Resume/Restart/Stop+behavior, TimeMultiplier), Vfx (Play/Stop),
  UI (SetEnabled + scaling/fading numerics).
- **W2 — high value**: Float/Integer/Byte attributes via ONE shared templated row helper
  (Override + component dropdown, per-modifier Override/Remove on existing rows, refill
  Pause/Resume, ClearAllModifiers), Variables (per-type `Set` editors for the 14 easy types —
  rows already exist, swap display→editor), Camera (7 flag toggles + SnapBoomRotation +
  yaw limits; CameraLayer Acquire→Override protocol deferred).
- **W3 — breadth**: StateMachine (Start/Stop/Pause/Resume, authority-gated), Aggro (toggle,
  clear buttons, per-target threat numerics + perception buttons; authority-gated),
  TagSet/EntityTag (add/remove entry rows + per-row remove), Physics (Euler Start/Stop,
  Override velocity/acceleration vector rows, per-modifier Remove), Compass, FogOfWar,
  InteractTarget, Objective (+Owner per-row remove), SceneNode (Detach + UpdateOffset),
  EntityInfo (Set_DebugName entry only), Audio (track play/stop/volume, director stop rows).
- **W4 — moderate sweep**: Shapes dimension numerics, Minimap, OverlapBody/ProbeTraces
  enable toggles, Iskm/IsmProxy easy set, AnimPlans tag dropdowns, InteractionResolver
  intents, EntityTagQuery requirement rows, EntityCollections/DynamicFragments per-row
  removes (struct ptr already in hand), Poi disabled-tag toggle, Inventories' three easy
  item-trait calls (authority-gated), MontagePlayer buttons (authority-gated),
  PathNetworkFollower (Remove/SetNetwork), **GOAP WorldState truth-table toggles** in the GOAP
  window (`Set_Value(H, tag, bool)` + override push/pop — the census's highest-leverage AI
  debug verb).
- Tier C (no easy surface): AStar, ActorRelay, IskmRenderer, Network, PathNetwork, Resolver,
  Relationships beyond Team assign/ContextOwner override — one lock-icon row states why
  where it isn't obvious.

## Style Lab tie-in

New axis `EditControlStyle` (Inline (def) / OnHover / Hidden) — Hidden restores fully
read-only inspectors; OnHover reveals edit affordances only on row hover. Ships with U0 so
the user can globally tune/disable interactivity from day one.

## Specs / gate

- Builder vocabulary + edit-guard: spec coverage in the ECS debugger test host (focus guard
  defers rebuild; authority gate truth table over net modes).
- Census spec: not enforceable per-inspector cheaply — the per-wave audit reviews the
  curated list against this doc instead.
- Everything visual/interactive in PIE = `[EDITOR-VERIFY]` (click a Timer pause button,
  drag Transform X, watch the entity move).
