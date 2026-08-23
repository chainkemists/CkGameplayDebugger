# Style live-ness + Style Lab completeness (P8 of the Debugger UX campaign)

Design authored 2026-08-09 (Fable session) from three verified audits (live-ness matrix, coverage
gap census, EntityRef/Singleton/icons recon). Brief (user, post-PIE): (1) profile changes only
visibly move the ECS debugger — every debugger must respond, common widgets everywhere possible;
(2) the Style Lab is missing style dimensions (named example: icons and their backgrounds) —
full completeness pass; (3) the ECS "Singleton" section is incomprehensible; (4) every inspector
group gets an icon (Relationships, EntityInfo named); (5) `SCkDebug_EntityRef` gets a real look
and becomes Style-Lab-stylable (info density, pill, monochrome, …).

## Root cause of (1) — audit-confirmed, write path exonerated

Profile apply + axis cycle + Editor Preferences all call `SaveConfig()` + `NotifyChanged()`
(`SCkStyleLab_ControlsPane.cpp:411-416, :370-375`; `CkDebuggerStyleSettings.cpp:50-60`). The
asymmetry is 100% read-side, ranked:

1. **One structural revision watcher outside the Lab** — the ECS entity tree
   (`CkDebuggerWidget_EntityTree.cpp:758-777`). `SCkDebug_WindowChrome`/`SCkDebugger_WindowBase`
   have zero axis reads and zero revision watch, so no window inherits live-apply.
2. **Six of seven `Make_*` helpers bake the axis at call time**
   (`CkDebuggerAxes.cpp:124-357`); only `Make_AxisSeparator` (:455-477) is lambda-bound.
3. **`Apply_RowDensity` in value form at ~60 of ~65 sites** — `.Padding(FMargin)` is a constant
   attribute. Reference live form: `CkDebuggerWidget_EntityTree.cpp:369-372`.
4. **Live primitives barely adopted**: `SCkDebug_Icon` used by 2 modules; most modules have one
   `EntityRef` pill in a toolbar; bare `SImage` at 42 sites despite the doc rule.
5. **`SCkDebug_Chip` ignores `ChipStyle`** (:20-50 own tone table); **`SCkDebug_KeyValueRow`
   (26 sites, THE inspector row) ignores `RowDensity` + `ValueAlignment`** (:50,109,120,137).
6. Rebuild-on-data modules (Aggro, Dialog, Eqs, GOAP rails, overlay card) mask the gap while
   PIE data churns and freeze at rest.

## Rulings

R1. **Live-by-construction becomes the contract.** Every axes-lib helper that emits a widget is
    attribute/lambda-bound (pattern: `SCkDebug_SectionHeader.cpp:74-76`,
    `SCkDebug_CountBadge.cpp:85-104`). A helper that cannot be live (returns raw values into a
    non-attribute Slate arg) must say so in its doc comment.
R2. **One suite-wide structural watcher.** `SCkDebugger_WindowBase` (fallback: WindowChrome)
    polls `Get_Revision()` inside the existing refresh-gated tick and calls a new overridable
    `OnStyleRevisionChanged()`; each window routes it to its existing Rebuild entry point.
    Mirrors `SCkStyleLabWindow.cpp:100-118`. The overlay focus card re-applies its model on
    revision bump (it already rebuilds on `Set_Model`).
R3. **Padding goes lambda.** Mechanical rewrite `.Padding(Apply_RowDensity(X))` →
    `.Padding_Lambda([]{ return ck::debug_axes::Apply_RowDensity(X); })` across all modules.
R4. **Common primitives everywhere**: bare `SImage` glyph sites → `SCkDebug_Icon`
    (keep click-passivity); hand-rolled uppercase headers → `SCkDebug_SectionHeader`;
    baked `Make_SectionHeader` call sites → the widget (or the now-live helper).
R5. **Helper signatures widen**: `Make_Chip`/`Make_Badge` gain `(FText, FLinearColor Ink,
    FLinearColor Fill, TOptional<int32> FontSize)` overloads (tone overloads kept) — reclaims
    `SCkDebug_CountBadge`, the overlay card's three local renderers, `SCkDebug_Chip`.
    `Make_SectionHeader` gains an optional tooltip parameter.
R6. **Classic must remain visually equivalent** after every unit (same regression bar as P5:
    layout identical; exact colors may converge onto roles only where already ruled).
R7. **No per-frame brush allocation** — new visual variants (outline pills, icon wells, corner
    styles) come from brushes registered once in `FCkDebuggerStyle`.

## New axes (schema v4) + Layer-A roles

Axis defaults are today's look; ini stores names, so fallback-on-unknown is free. Profiles
(Classic/Dense/Presentation/MinimalInk) each take an explicit stance on every new axis.

| Axis | Options (default first) | Reach contract |
|---|---|---|
| `IconTreatment` | Plain / Well / Ring | Every `SCkDebug_Icon` site; the two existing `Card.IconWell` sites become Well explicitly. `SCkDebug_Icon` grows an optional `.Accent` so the well can tint. |
| `TextScale` | Normal / Small / Large (×1.0 / ×0.875 / ×1.125) | New `ck::debug_axes::ScaledFont(face, roleSize)`; the five `CkDebugger.Text.*` styles + module sweeps migrate font literals onto `CkStyle::FontSize*` roles as they go. Overlay's own `FontScale` composes on top, unchanged. |
| `EntityRefStyle` | Flat / Pill / OutlinePill / Monochrome | Visual TREATMENT of `SCkDebug_EntityRef` (see P8-U3). `EntityIdStyle` shrinks to pure text composition; `HashTintedChip` migrates here as Pill+hash-tint, with an ini back-compat note. |
| `CornerStyle` | Rounded / Sharp / Pill | Via new `Get_ChipBrush()/Get_BadgeBrush()/Get_CardBrush()` selectors replacing direct `GetRoundedBrush*()` in axes lib + common widgets. Inline-radius sites migrate opportunistically in sweeps. |
| `SurfaceElevation` | Layered / Flat | Scoped to the common surface widgets only (Card, WindowChrome strips, InspectorPanel, LabeledGroup, ExpandableColumn) via `Get_SurfaceBrush(Depth)`/`Get_SurfaceTint(Depth)` — NOT a 230-site SBorder sweep. |
| `GraphNodeStyle` | Card / Minimal / Dense | SM + GOAP `SGraphNode` subclasses route their fill/border/radius constants through the existing `CkStyle::NodeBorderThickness()/NodeInactiveOpacity()` roles + this axis. Delete the dead `FCkDebuggerStyle` graph block. |
| `RowBanding` | Off / Zebra / Hairline | List/tree surfaces; the dead `Row.Even/Odd` brushes finally earn their keep. Fixes `SCkSmDebugger_HistoryList.cpp:309` engine-style row. |

Layer-A additions (CkFoundation `CkStyle`): alpha ladder `AlphaFaint/Dim/Soft/Strong`,
`SelectionOverlayAlpha`/`HoverOverlayAlpha`, radius value tokens `RadiusS/M/L/Pill`,
`IconWellAlpha`, `RingWidth`. Consumption migrates opportunistically in sweeps (no big-bang).

Style-set cleanup (with U2): delete or wire the 21 dead keys (13 `Color.*`, `Row.Even/Odd` →
RowBanding, 3 `Graph.NodeBackground*` → delete, `Panel.Background`, `Selection`, `Hover`,
`Background.Light`, `Text.Monospace`); text styles onto `FontSize*` roles; reconcile GOAP's
padding ladder (Large 14 → 16).

## P8-U3 — EntityRef flagship

Constraints from recon: the widget is already fully attribute-driven — extend the getters
(`Get_ChipColor/Get_ChipPadding/Get_TextColor` + new brush getter), never restructure; keep
`SBorder` + `OnMouseButtonDown` (an `SButton` wrapper breaks tree-row selection); font becomes a
live attribute (currently construct-baked); outline variant uses a registered brush (R7).

- `EntityIdStyle` = composition only (NameAndId/CompactId/NameOnly). `HashTintedChip` removed
  from it; schema bump covers ini fallback. `EntityRefStyle` = treatment: **Flat** (today),
  **Pill** (rounded fill wash — hash-tinted, today's HashTintedChip look), **OutlinePill**
  (1px ring, transparent fill), **Monochrome** (no accent color, `TextDim` ink, id in mono font).
- Look upgrade (all treatments): hover affordance (brighten/underline via `IsHovered()` in the
  existing color attributes — no new widgets), invalid-handle stays muted "None".
- Style Lab SamplePane previews the real widget (today it previews text-only via
  `Make_EntityIdText` — it would lie about treatments).
- Hand-rolled sites: U3 fixes the ECS-owned ones that duplicate composition
  (`CkDebuggerPanel_Inspector.cpp:742-750` Format_EntityDisplayName routes through
  `Make_EntityIdText`; `CkDebuggerPage_Activity.cpp:138`), plus
  `SCkGoapDebugger_SquadTable.cpp:301` (documented stale-format duplication) and
  `SCkDebug_UseEcsSelection.cpp`. Collector-side name-only strings stay (they never claimed to
  be entity refs). Module sweeps adopt `SCkDebug_EntityRef` where a real ref is hand-rolled.

## P8-U4 — icons for every inspector + Singleton clarity

- Census: 27/47 inspectors have working icons; 20 declare none; PathNetwork +
  PathNetworkFollower declare ids with NO svg on disk (silent latent bug).
- Author monochrome-white SVGs at `Resources/Icons/` TOP LEVEL (never `General/` — that pool
  reshuffles every hash-picked archetype glyph) for the 18 missing + 2 phantom ids. Free wins:
  Tween.svg, EntityCollection.svg already exist — override only. Adoptable spares:
  Attribute/Eqs/Goap/Grid/Input/Perception/Pin/World/Cube.svg.
- One-line `Get_IconName()` override per inspector (+ `Get_FeatureColor()` where a feature color
  exists). Editor restart is part of the normal gate rebuild.
- **Guard spec** (launcher-catalog pattern, `CkDebuggerLauncherCatalog.spec.cpp:69`): every
  registered inspector declares a non-None icon AND it resolves via
  `FCkDebuggerStyle::Get_IconBrush`. Kills the silent-nullptr class of bug permanently.
- **Singleton section** (`CkDebuggerPage_Dashboard.cpp:216`): it lists archetypes whose current
  population == 1 (`:397-406`) — nothing to do with ECS singletons. Rename header to
  `UNIQUE ARCHETYPES`, add tooltip via the R5 `Make_SectionHeader` tooltip param:
  "Archetypes with exactly one live entity in this world right now — a population count, not a
  registry singleton. Click the pill to open the instance." Add the missing `+N more` overflow
  indicator (cap is 8, currently silent). Update `CkEcsDebugger_Dashboard.spec.cpp` if member
  names shift.

## Task units + sequencing (Opus implements; orchestrator gates; no builds during edits)

- **U1 — live-ness foundation** (solo; CkDebuggerCommon + CkDebuggerAxes + WindowBase):
  R1 helper conversion (6 helpers → live), R2 watcher + `OnStyleRevisionChanged`, R5 signature
  widening (+ SectionHeader tooltip param), wire `SCkDebug_Chip`→ChipStyle,
  `SCkDebug_KeyValueRow`→RowDensity+ValueAlignment, `SCkDebug_IconToggle`+chevrons+search-bar
  clear glyph→IconSize, `SCkDebug_EventLog` row regeneration on revision. Spec: helper
  live-ness (axis flip changes emitted visual without re-calling the helper) where testable.
- **U2 — axes + roles + cleanup** (solo, after U1; CkDebuggerCommon + CkFoundation CkEditorTools
  + Style Lab previews): 7 new axes (table above), schema v4, profile stances, Layer-A roles,
  style-set cleanup, `ScaledFont`, brush selectors, SamplePane preview rows for new axes.
  CkFoundation commit separate (submodule).
- **Wave 3 (parallel, file-disjoint, after U2):**
  - **U3 EntityRef** — widget + StyleLab SamplePane (sole owner in this wave) + the named
    hand-rolled sites.
  - **U4 icons + Singleton** — Resources/Icons, inspector `.h` one-liners, Dashboard page,
    guard spec, Dashboard spec.
  - **U5a sweep**: Jolt, UI, Input, Launcher, Insights, ObjectPooling, Map.
  - **U5b sweep**: Eqs, AStar, Aggro, Dialog, Scheduler, Crowd.
  - **U5c sweep**: Sm + Goap (incl. GraphNodeStyle routing, GOAP axes-value-form → live,
    padding-ladder reconciliation).
  - **U5d sweep**: EcsDebugger pages/panels remaining value-form sites + CkEntityDebugOverlay
    revision re-apply.
  Sweep checklist per module: R3 padding lambdas, R4 primitive adoption (SImage→Icon,
  headers→widget), font literals→`ScaledFont`/roles, hook `OnStyleRevisionChanged` to the
  window's rebuild, adopt EntityRef where hand-rolled, apply new axes at the module's obvious
  sites, IconTreatment via SCkDebug_Icon adoption.
- Gate after U1, after U2, after wave 3 (toolbox `--test-pattern Debug`; expect spec Total to
  grow with the new guard/liveness specs; stale-green check via Total).

## Acceptance

- Flipping any profile in the Style Lab visibly changes EVERY open debugger window within one
  gated tick — no reopen. `[EDITOR-VERIFY]` per window.
- Icons: every inspector header shows a glyph; guard spec enforces it forever.
- EntityRef: 4 treatments × 3 compositions live-switchable; hover affordance; Lab previews truth.
- Singleton section renamed + tooltip + overflow count.
- Classic profile: visually equivalent to pre-P8 (R6).
