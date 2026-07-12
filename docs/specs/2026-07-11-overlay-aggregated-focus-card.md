# Overlay Focus Card — Aggregated Single-Entity Debugger (Design Spec)

**Date:** 2026-07-11 · **Mode:** Fable e2e (maintainer-armed goal) · **Module:** CkEntityDebugOverlay
(+ a settings-editor popover in CkEcsDebugger).

## 1. Problem & goals

The focus card shows too little to debug an NPC: feature state (SM instances, GOAP planner,
crowd agent, attributes) lives on **sub-entities below** the focused entity, which the card
never visits; the card sits top-right squeezed beside engine on-screen text; and attribute
volume floods what little space there is.

Goal: the card becomes the one-screen compressed union of all debuggers **for the focused
entity's whole subtree** — top-left, tall (≤ 2/3 viewport), with engine on-screen debug text
suppressed while the overlay is active, and an attribute allow/deny filter editable from the
ECS Debugger mid-PIE.

Maintainer-confirmed calls: top-LEFT is the final anchor (the earlier top-right placement
existed only because on-screen text owned top-left); suppression mechanism =
`GEngine->bEnableOnScreenDebugMessages` toggle with prior-value restore; height grows as
needed up to the 2/3 cap (clip past it), not a fixed block; full-depth subtree, no cap;
filter = pattern list + invert bool, substring match; filter UI = picker-toolbar popover
backed by persisted config.

## 2. Design

### A. Subtree aggregation (`ck_debugoverlay::Build_EntityModel`)

- Collection walks the focus entity **plus all lifetime descendants**
  (`UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents`, recursive, visited-guard).
- Per (source entity × provider) with `CanProvide`: one section, same layout gating
  (`Resolve_EnabledFields` + EntryFilter) computed once per provider.
- `FCk_DebugOverlay_Section` gains `SourceName` (clean debug name, EMPTY for the focus
  entity itself) — the card renders it as a dim suffix on the section header so you can
  tell whose SM/attribute you're reading. Sections sort by (SortPriority, source order:
  focus first, then discovery order).
- History keys switch from `{FocusId, FieldTag}` to `{SourceId, FieldTag}` — same field on
  two sub-entities must not collide in the change-flash history.
- Compression guard: sections from sub-entities whose provider already emitted an identical
  row set for the focus entity are NOT deduped (they're distinct instances by definition);
  but a per-model row budget is unnecessary — the 2/3 plate cap + section compactness bound it.
- Candidate gathering (diamonds/world pills) is UNCHANGED — aggregation applies to the focus
  card (and pinned cards, which share `Build_EntityModel`).

### B. Plate anchor, height, on-screen text

- `UCk_DebugOverlay_Settings::PlateAnchor` default → `TopLeft` (comment updated; this
  project's DefaultGame.ini does not override it — verified).
- `SCkDebugOverlay_Root`: the card strip is wrapped in a max-height box bound to
  **2/3 of the root's cached geometry height** (live TAttribute — tracks viewport resizes);
  overflow clips (the root is hit-test invisible by design — a scrollbar would be
  uninteractable). New settings knob `PlateMaxHeightFraction` (default 0.66, clamp 0.2–0.95).
- Subsystem `DoActivate`: save `GEngine->bEnableOnScreenDebugMessages` → set false;
  `DoDeactivate`: restore saved value. Gated on the primary-console-owner instance so
  split-screen LP subsystems don't double-save. Covers `AddOnScreenDebugMessage` +
  `ck::Trace` (same channel).

### C. Attribute filter

- `UCk_DebugOverlay_Settings` gains (Config, "Attributes" category):
  - `TArray<FString> AttributeFilterPatterns` — case-insensitive SUBSTRING matches against
    the attribute's label-tag string (`Health` catches `Attr.Health.Max`).
  - `bool bAttributeFilterIsExclusion` — false: show ONLY matching (empty list ⇒ no
    filtering); true: show all EXCEPT matching.
- Applied inside all five attribute providers' `Collect` (shared helper in
  `CkDebugOverlay_Tags.h`-adjacent utility or a small `ck_debugoverlay::Matches_AttributeFilter`).
- Editor UI: the ECS Debugger picker-toolbar popover gains an "Overlay Attributes" block —
  comma-separated pattern text box + "Exclude listed" checkbox → writes the mutable settings
  CDO + `SaveConfig()`. Applied live (providers read settings per Collect).

### D. Providers

- **NEW `Crowd`** provider (module dep `CkCrowd` added): status, desired velocity/speed,
  target yaw, debug-override flag — mirroring the crowd debugger collector's read set.
  Registered in the "All" layout + abbrev map.
- **NEW `VectorAttributes` / `RotatorAttributes`** providers cloning the Float pattern
  (ForEach + label + final value; delta display where meaningful).
- **StateMachine**: `History` field flips to `DefaultEnabled=true` (the ring was already
  read; it just never showed by default).
- **Goap**: already Goal/Action/Plan/Cost — Plan/Cost fields flip to default-enabled so the
  planner reads at a glance.
- Layout ctor ("All" + themed): add the three new provider tags; `Get_ProviderAbbrev`
  entries for Crowd/VectorAttr/RotatorAttr.

## 3. Acceptance

| # | Check | How |
|---|---|---|
| A1 | Focusing an NPC shows sections sourced from sub-entities (dim source suffix), incl. attributes/SM/GOAP living below the root | `[EDITOR-VERIFY]` |
| B1 | Card anchored top-left, grows with content, never exceeds ~2/3 viewport height; engine on-screen text absent while overlay active and back after `ck.DebugOverlay 0` | `[EDITOR-VERIFY]` |
| C1 | Filter patterns + invert bool edited from the ECS Debugger popover take effect live on the card; persisted across sessions | `[EDITOR-VERIFY]` |
| D1 | Crowd section on crowd agents; Vector/Rotator attribute rows; SM history rows by default; GOAP plan visible | `[EDITOR-VERIFY]` |
| E1 | Existing overlay specs stay green (`Ck.DebugOverlay.*`) + full editor build | toolbox |

## 4. Risks / notes

- Aggregation cost: descendants × providers × CanProvide per tick — debug-only, bounded by
  MarkerMaxDist focus set; measured acceptable at review if an ISKM-stress NPC feels heavy.
- History memory: per-source keys multiply history buckets; ring sizes are small and reset
  on activation.
- `bEnableOnScreenDebugMessages` is engine-global: if PIE ends while active, Deactivate
  restores (subsystem deinit path) — verify restore ordering on abrupt PIE stop.
- Clip-not-scroll at the 2/3 cap is deliberate (hit-test-invisible root); if the maintainer
  wants scroll, the root would need selective hit-testing — recorded as a possible follow-up.

### PIE round finding (2026-07-11) — ensure storm, root-caused + fixed

Enabling the overlay in the crowd pathing gym collapsed frame time until WASD/mouse felt
dead. Log verdict: 3,194 identical `CK_ENSURE` fires (GameplayLabel `Has(InHandle)`), all
inside the overlay-active windows — the subtree BFS reached ability sub-entities owning
UNNAMED cooldown timers, and `UCk_Utils_Timer_UE::Get_Name` was an ungated
`GameplayLabel::Get_Label`. ~2 fires/frame × (native+BP+AS stack capture + dual-channel
log + editor-message push) ≈ 45–80 ms frames. Because the overlay ticks from FTSTicker
(outside the PIE world scope), CkEnsure took the editor-notification branch — no dialog,
no self-silencing, refires every frame. Fixed at the source: CkFoundation `cba35adf6`
(`Get_Name` label-gates; unnamed timers are a designed state per `Add`). Also defuses the
same latent storm in Gen-2 `CkInspector_Timer`.

**Blind-spot note (resolved):** the suppression in §B hides engine-channel ERROR text,
but ensure VISIBILITY is already owned by the CkWatermark panel — pure Slate reading
`UCk_Ensure_Subsystem_UE::Get_EnsureCount()` live, unaffected by
`bEnableOnScreenDebugMessages` (maintainer call: no error strip on the card; ensures that
appear get root-caused and fixed, not re-displayed). Follow-up recorded: regression
AutoTest (unnamed timer → `Get_Name` returns invalid tag, no ensure) once CkTests is free.
