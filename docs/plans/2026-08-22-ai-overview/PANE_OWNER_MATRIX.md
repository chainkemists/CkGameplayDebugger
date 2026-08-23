# Debugger pane-owner matrix

> **Captured:** 2026-08-23 before the pane-host contract changed.
> **Visual ground:** AI Overview is the golden Cards reference; Workbench is contiguous, square, and splitter-owned. Outlined remains retired.

## Shared diagnosis

`SCkDebug_Card` currently paints `GlowWrap -> rounded ring -> rounded surface -> arbitrary child` (`SCkDebug_Card.cpp:27-85`). It does not clip descendants. A zero-padding opaque child can therefore overwrite the rounded body corners, while retained child borders/separators compete with the Common ring and splitter in Workbench.

The neighboring feature to mimic is AI Overview: every pane is an ordinary Common card with a non-zero body inset and a transparent layout root (`SCkAiDebuggerWindow.cpp:301-473`). Its spatial viewport remains opaque only inside the card-owned inset.

## Owner matrix

| Debugger / pane family | Immediate child/perimeter evidence | Redundant outer chrome | Semantic fill to preserve | Proposed owner | Frozen geometry |
|---|---|---|---|---|---|
| **AI Overview — all panes (golden)** | Cards contain plain `SVerticalBox` roots; roster `:308-318`, evidence/events `:341-368`, spatial `:372-380`, topology `:432-459`. | None. | Spatial viewport inside the existing card inset. | Existing `SCkDebug_Card`; no source change. | H 20/80; nested V 52/48; diagnostics H 42/58 and V 58/42; overview V 46/54, H 38/34/28 and 50/50; all 5px handles. |
| **ECS — left/center/inspector** | Three zero-padding cards at `CkDebuggerWindow_Main.cpp:193-237`; center nests `CkDebugger.Border` plus 1px horizontal padding at `:211-220`. | Center legacy border. | Content pages, graph/canvas pages, inspector behavior. | Common pane host; transparent center root. | H 20/50/30, mins 200/400/250, physical 3px / hit 5px. |
| **GOAP — agent/planner/center/world/timeline** | `WrapPane` zero-padding cards `SCkGoapDebuggerWindow.cpp:980-1048`; center root `CkGoap.Bg.Root` `:1102-1105`. | Center root fill; any panel root proven to be pane-only chrome. | Center tab strip, graph canvas, search/decision bodies, timeline. | Common pane host; framed center/timeline where opaque. | Outer V 70/30; top H 28/49/23; left V 62/38; mins 260/220/120/100; default handles. |
| **State Machine — graph/history/detail** | Graph direct at `SCkSmDebuggerWindow.cpp:1083-1091`; cards at `:1182-1205`; detail root `SBorder Bg1/pad8` at `:2652-2654`. | Detail root fill; retain its padding as content spacing. | Runtime graph and scrub timeline. | Framed pane host for graph; passive hosts for history/detail. | Root H 100%; left V 65/35; bottom H 60/40; default handles. |
| **Scheduler — tree/graph/detail** | Three card children immediately wrap `ToolPanel.GroupBorder`, `CkSchedulerDebuggerPage_TreeView.cpp:62-84`. | All three `GroupBorder` roots. | Center graph canvas. | Passive hosts on rails, framed host on graph. | H 25/50/25, mins 240/400/260; default handle. |
| **AStar — grid/stats/history** | Grid is direct semantic leaf `SCkAStarDebuggerWindow.cpp:146-154`; stats/history cards `:167-182`. | None in stats/history. | Grid paints full `BgRoot`, `SCkAStarDebugger_GridView.cpp:127-165`. | Framed pane host for grid; passive hosts for stats/history. | Main H 70/30; right V 60/40; default handles. |
| **EQS — query/candidates/tests** | Cards nest padding-only borders at `SCkEqsDebuggerWindow.cpp:145-171`; splitter has explicit 6px outer padding `:138-140`. | 6px layout gutter and border shells; retain 4px as content padding. | Query/candidate/test widgets. | Passive pane hosts. | H 30/35/35; default handle. |
| **Intent — top row/timeline/key-state/devices** | Top row sits inside square `SBorder Bg2`, `SCkIntentDebuggerWindow.cpp:249-321`; individual card children are transparent layouts/timeline. | Top `Bg2` shell. | Timeline and toolbar grouping. | Common pane hosts; timeline framed only if its paint reaches the host edge. | Outer V 30/70; top H 34/36/30; bottom V 35/65; bottom H 38/62. |
| **Dialog / Aggro** | Single zero-padding card with transparent `SVerticalBox`/scroll roots, Dialog `:109-132`, Aggro `:158-173`. | None proven. | Existing list/scroll content. | Passive pane host, preserving single-surface information architecture. | No splitter. |
| **Crowd — nav/list/stats/events/viewport/detail** | Every card child has opaque `SBorder Bg1`; window hosts at `SCkCrowdDebuggerWindow.cpp:153-189`, panel roots at Navmesh `:40`, list `:150`, stats `:24`, events `:61`, detail `:53`. 2D viewport root `SBorder BgRoot`, `SCkCrowdDebugger_ViewportPanel.cpp:74-81`. | Plain panel root fills. | Interactive 2D viewport custom paint. | Passive hosts for plain panels; framed host for viewport. | Outer H 20/52/28; left V 22/46/14/18; existing dynamic handle/min constants preserved. |
| **Jolt — outliner/3D/stats/detail** | Window body `SBorder Bg1` `SCkJoltDebuggerWindow.cpp:502-505`; outliner root `SBorder Bg1`, `OutlinerPanel.cpp:154-160`; stats/detail roots are plain. | Window body and outliner fills. | `SCkDebug_3dPreviewViewport` black scene ground. | Framed host for 3D; passive hosts elsewhere. | Main H 22/53/25; right V 62/38; default handles. |
| **Map — rails/canvas/status** | Fixed rail cards `SCkMapDebuggerWindow.cpp:791-806`; body `SBorder Bg1` `:781-784`; canvas is `OnPaint` leaf; status card nests `SBorder BgRoot` `:715-743`. | Body/status fills; separators must be the sole Workbench boundaries. | Map canvas. | Passive rail/status hosts; framed canvas host. | No splitter: fixed 250/280 rails and existing single separators. Do not silently change topology. |
| **Object Pooling — overview/list/inspector** | Full-window `SBorder BgRoot` `SCkObjectPoolingDebuggerWindow.cpp:204-208`; nested cards and explicit separator `:219-267`. | Window root; retain only one Workbench boundary. | Lists/rows. | Passive hosts. | Fluid list + fixed 300px inspector; no splitter. |
| **Input / UI** | Input page root `SBorder Bg1` `SCkInputDebuggerWindow.cpp:180-198`; section cards wrap `SExpandableArea Bg1` `:362-386`. UI history card wraps `SExpandableArea BgRoot`, `SCkUIDebuggerWindow.cpp:288-310`; overall UI card root is clean `:349-369`. | Page/expandable outer fills, not disclosure behavior. | Timeline/history contents and search-toolbar surfaces. | Common pane/card owns section perimeter; disclosure widgets become transparent at the edge. | No splitter. |
| **Insights — hot path/results** | Outer `SBorder BgRoot` `SCkInsightsAnalyzerTab.cpp:451-486`; result cards `:1060-1082`; hot-path root `SBorder Bg1` `:1126-1144`. | Outer/hot-path fills after preserving content padding. | Tree/table results and local settings/status frames. | Passive pane hosts. | H 62/38, physical handle 2px. |
| **Optimization / Save** | Representative hosts already use transparent `SVerticalBox`, `SScrollBox`, or `SWidgetSwitcher` roots; Optimization `:1728-1852`, Save `:721-882`. | None proven at host edge. | Save blob/diff visualization; optimization lists/details. | Common pane host; frame only opaque viewers. | Optimization H 62/38 + right V 45/35/20. Save outer V 72/28 + top H 32/36/32. |
| **Audio** | Transparent page switcher; purpose-specific cards and bespoke radar/curve, `SCkAudioDebuggerWindow.cpp:258-340`, `:469-490`, `:832`, `:887`, `:1443-1451`. | None justified. | Radar, curve, lanes, event log. | Already responsive/bespoke; no pane-host migration. | No splitter. |

## Contract decision

Use a dedicated `SCkDebug_PaneHost`, not another `SCkDebug_Card` mode and not a convention-only cleanup:

- **Cards:** Common owns one rounded ring and surface; passive content is transparent at the edge; opaque canvas/viewport content opts into a deliberate inset frame.
- **Workbench:** Common paints a square, ringless, zero-extent pane surface; the existing splitter handle or one retained fixed-layout separator owns the shared boundary.
- **AI Overview:** remains on ordinary `SCkDebug_Card` and is regression-compared as the golden reference.
- **Outlined:** remains a hidden legacy wire value resolving to Workbench/Flat and is never exposed.

This is the narrowest enforceable scope: `SCkDebug_Card` remains a generic card primitive, while pane-shell ownership becomes explicit and testable.
