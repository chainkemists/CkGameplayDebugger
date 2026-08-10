# Common-widget consolidation + suite-wide style axes (P5 of the Debugger UX campaign)

Design authored 2026-08-09 (Fable session) from a full-module census (verified file:line).
Brief (user): the Style Lab must drive ALL debuggers, and no debugger keeps bespoke widgets
unless genuinely specific to it — common widgets, styled once, across the suite.

## Census verdict (2026-08-09, all 18 non-legacy modules)

- Axis wiring: full = CkStyleLabDebugger only; partial = CkEcsDebugger (3 files), overlay
  (FocusCard only); the other 15 modules read zero axes.
- Palette isolation: AStar, Input, Jolt, Map, UI consume ZERO `CkStyle::` — each carries a
  local hardcoded color block. Input/UI/Jolt/Map share one verbatim copy-pasted
  `namespace style` block. Sm is worst-in-class (112 literals, 35-token local palette).
- Duplicates: 3 sparkline impls (common + ECS + Scheduler); 2 frame-strips
  (Scheduler `FrameHistoryBar`, Insights `SCkFrameBarChart`); 2 scrub-tracks (GOAP
  `ScrubTrack`, SM `Timeline`); 2 CkStyle-backed `FSlateStyleSet`s (ECS `FCkDebuggerStyle`,
  Launcher); AStar/ObjectPooling/UI/Input rebuild meters/dots/stat-tiles common already has.

## Rulings

1. **Role colors → `CkStyle::`.** Every local bg/text/status/accent token dies; missing roles
   are added to the palette (CkStyleSettings) — never a new hex at a use site.
2. **Domain ramps → common helpers.** Heat ramp (Scheduler), score gradient (Eqs), categorical
   palette (ECS inspector filter) become palette-derived helpers next to `ck::debug_axes`
   (e.g. `Get_HeatColor(Normalized)`, `Get_ScoreColor(Normalized)`, `Get_CategoricalColor(Index)`).
   Semantic canvas colors that mean something only inside one visualization (AStar grid cell
   states, Crowd navmesh paint, Map fog) stay local with a one-line justification comment.
3. **Brush/text style set → common.** ECS's `FCkDebuggerStyle` (brushes + text styles + SVG
   icon registry) is promoted to CkDebuggerCommon as the ONE style set; ECS and Launcher
   re-point; other modules adopt instead of inventing.
4. **Widget promotions** (b-class census findings):
   - `SCkDebug_FrameStrip` = merged FrameHistoryBar + FrameBarChart (bars, budget heat,
     scrub + pan + highlight filter).
   - `SCkDebug_ScrubTimeline` = merged GOAP ScrubTrack + SM Timeline core (segments, cursor,
     Live/Scrub modes, markers).
   - `SCkDebug_EventLog` = promoted Crowd EventLogPanel (scrolling capped event list).
   - `SCkDebug_SearchBar` = promoted single-mode search (ECS `SCkDebuggerWidget_SearchBar`)
     as the light sibling of `SCkDebug_DualSearchBar`.
5. **Widget retirements** (a-class): ECS + Scheduler sparklines → `SCkDebug_Sparkline`;
   ObjectPooling `SOccupancyBar` → `SCkDebug_MeterBar`; AStar StatsPanel internals →
   `SCkDebug_StatPair`/`SCkDebug_MeterBar`; AStar SearchHistory → `SCkDebug_HistoryRow` +
   `SCkDebug_RailContainer`; Input/UI ad-hoc dots → `SCkDebug_CategoryDot`/`SCkDebug_StatusPill`;
   Eqs QueryList rows → `SCkDebug_HistoryRow`.
6. **Axis wiring floor** (every module, including graph nodes): row padding =
   `Get_RowPadding`, section headers = `Make_SectionHeader`, chips/badges = `Make_Chip`/
   `Make_Badge`, icon sizes = `Get_IconSize`, separators = `Get_SeparatorThickness`,
   entity refs already axis-aware via `SCkDebug_EntityRef`.
7. **Regression bar:** under the Classic profile the suite is *visually equivalent* — layout
   identical; exact colors may shift where a hardcoded literal is replaced by its nearest
   `CkStyle::` role (that convergence is the point). Byte-identical is required only where a
   module already consumed `CkStyle::`.

## Task units (Opus; no builds — orchestrator gates)

- **U1 — foundation (blocks all):** promote the style set to common; add ramp/categorical
  helpers + any missing palette roles; promote `SCkDebug_SearchBar`; build
  `SCkDebug_FrameStrip`, `SCkDebug_ScrubTimeline`, `SCkDebug_EventLog` in common (Gallery rows
  for each, per the Gallery convention).
- **U2 — trivial cluster:** Jolt, UI, Aggro, Dialog, Launcher, Input.
- **U3 — small cluster:** Map, ObjectPooling, Insights (adopts FrameStrip).
- **U4 — Eqs + AStar.**
- **U5 — Scheduler (FrameStrip + sparkline retire + style-set split) + Crowd (EventLog).**
- **U6 — Sm (palette migration + ScrubTimeline adoption + graph node re-tone).**
- **U7 — Goap (breadth re-tone + ScrubTimeline adoption + graph nodes).**
- **U8 — EcsDebugger remaining surfaces (Pages, EntityList, Graph, Window) + overlay
  Root/WorldTag axis wiring.** Sequenced AFTER the P6 merge-policy unit lands (same module).

U2–U7 are file-disjoint and can run in parallel after U1. Gate after each wave: toolbox
build + `Debug`/`DebuggerLauncher`/module spec patterns; visual = `[EDITOR-VERIFY]` via the
Style Lab + Gallery.
