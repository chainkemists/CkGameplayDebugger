# CkGoapDebugger — "GOAP Mission Control"

> **Read `CkDebuggerCommon/CLAUDE.md` first.** It covers shared conventions: copy-selectable text, entity-ref pills, list/tree row contracts, search bars, PIE world lifecycle, and safety rules. This file only covers CkGoapDebugger's own architecture.

---

## What this module does

`CkGoapDebugger` is the editor debugger window for the `CkGoap` module — a designer-first "Mission Control" that answers, per agent: *what is it doing, why this plan, what does it believe, and what just happened?* It opens as a tab via `CK GOAP Debugger` (registered by `CkGoapDebugger_Module`).

The layout, feature set, and visual language are a faithful Slate port of the interactive HTML mockup at [Mockups/mockup_d_mission_control.html](Mockups/mockup_d_mission_control.html) — treat that file as the visual spec. The port plan + phase history live in `docs/plans/2026-07-18-goap-mission-control/PROGRESS.md` (repo root docs).

---

## Window anatomy (top to bottom)

```
SCkGoapDebuggerWindow
├── Chrome bar        product mark · world selector · agent EntityRef pill ·
│                     LIVE/PAUSED StatusPill + ⏮⏭ scrub steps · REC-clear ·
│                     Nerd-mode Switch · name-depth cycle
├── Nerd strip        (nerd-gated) plan µs · budget µs + MeterBar + slices ·
│                     states expanded · state pool · parent-gate note ·
│                     planner/WS ids — all live off the selected PlannerInfo
├── Alert strip       live-visibility AlertRows: Sandbox banner (DebugUI layer
│                     key count + shared-WS blast radius + Pop action) ·
│                     Fallback-plan-active warning
├── Top tabs          Squad (count) | Agent Inspector | Catalog Audit
└── Active view (SWidgetSwitcher)
    ├── SQUAD         SCkGoapDebugger_SquadTable — one row per top-level
    │                 Planner world-wide: avatar+name · planner label ·
    │                 status pill (all 5 states) · active chain · cost ·
    │                 attempts · replans-60s Sparkline · alert tags ·
    │                 Inspect ▸ (selects + flips to Inspector)
    ├── INSPECTOR     vertical splitter:
    │   ├── top: horizontal splitter
    │   │   ├── LEFT   vertical: SCkGoapDebugger_AgentColumn (agent card,
    │   │   │          chain crumb w/ GlowWrap leaf, goal chips w/ live
    │   │   │          satisfaction, plan step cards w/ NOW stripe + nested
    │   │   │          sub-plan + fallback note, Settings drawer w/ all
    │   │   │          params + RO locks + live requests + Replan/Cancel/
    │   │   │          Reset) over SCkGoapDebugger_Sidebar (Planner tree —
    │   │   │          the selection surface)
    │   │   ├── CENTER SCkDebug_UnderlineTabs (Decision | Plan graph |
    │   │   │          Search trace*) + switcher:
    │   │   │          · SCkGoapDebugger_DecisionPanel — every candidate,
    │   │   │            scored (DecisionModel): in-plan/viable/blocked/
    │   │   │            fallback groups, why-lines w/ forced-first Δ, cost
    │   │   │            steppers (Request_SetChildActionCost) + edited
    │   │   │            badges + reset, cond chips w/ trace highlight,
    │   │   │            cross-tier notes, per-composite sub-groups
    │   │   │          · SCkGoapDebugger_GraphPane — SGraphEditor tree
    │   │   │            (glow halo + role line on nodes, exp→cur pre dots,
    │   │   │            live costs, in-plan edge pulse, dashed tree edges)
    │   │   │          · SCkGoapDebugger_SearchTracePanel* — regressive-A*
    │   │   │            constraint-set rows + SearchStats strip (*nerd)
    │   │   └── RIGHT  SCkGoapDebugger_WorldStateRail — SectionHeader +
    │   │              Sandbox switch, nP·mE usage chips, layer shadow
    │   │              badges, "just changed" chips, editable ValuePills
    │   │              (DebugUI layer), click-to-trace rows, layer stack +
    │   │              base store, keys n/64 + subscribers footer
    │   └── bottom: SCkGoapDebugger_TimelineDock — SCkDebug_EventTimeline
    │              (WS/REPLAN/ACTIVE lanes, selectable replan diamonds,
    │              coalesce ×N) + jump-to-replan buttons + Pause-on
    │              checkboxes (bDebugPauseExecution) + diff card (trigger/
    │              old→new) + event log (click-to-scrub)
    └── CATALOG       SCkGoapDebugger_CatalogPanel — persistent SSplitter
                      chrome (drag positions survive rebuilds): action cards
                      per tier left (default half-window) · right = vertical
                      splitter of health checks (fallback guarantee, cycles,
                      goal keys, key budget n/64, cross-tier + dead-effect
                      INFO via DecisionModel::Lint) over the key×action
                      matrix (P/E cells, goal badges, active-chain column
                      tint, click-to-trace); every pane resizable + scrolls
```

---

## Data flow

```
UWorld tick (gated by FCkDebuggerRefreshGate)
  └── FCkGoapDebugger_ViewModel::Tick
        ├── FCkGoapDebugger_DataCollector::Collect(World)
        │     ├── per entity → FCkGoapDebugger_EntitySnapshot
        │     │     └── TopLevelPlanners: FCkGoapDebugger_PlannerInfo forest
        │     │         (settings block, LastReplanCause, SearchStats,
        │     │          SearchDebug, RecentWorldStateChanges, KeyUsage
        │     │          census, ChildActions/ChildPlanners recursion)
        │     └── DetectAndPushEvents → per-entity history ring
        │         (FCkGoapDebugger_HistoryEvent: +WorldStateChanged w/ change,
        │          +Replanned w/ CauseAtEvent, PlanFound/Failed w/ snapshot)
        ├── OnChanged.Broadcast()
        └── window fan-out: Sidebar / AgentColumn / DecisionPanel /
            SearchTracePanel / TimelineDock / SquadTable / CatalogPanel /
            WorldStateRail all RefreshFromViewModel()
```

- **DecisionModel** (`Data/CkGoapDebugger_DecisionModel.*`) — pure, Slate-free,
  headless-tested miniature of ck::goap's regressive A* (FName-keyed). Powers
  the Decision panel's forced-first deltas, cross-tier notes, and the Catalog
  lint. It must never drive gameplay. Specs:
  `Tests/CkGoapDebugger_DecisionModel.spec.cpp` (FEAR-gym truth table).
- **Engine hooks consumed** (added in CkGoap for this debugger): params
  getters, `Get_LastReplanCause`, `Get_LastSearchStats`/`Get_LastSearchDebug`,
  WS `Get_RecentChanges` (change-log ring), `Get_SubscriberCount`,
  `Get_TopOverrideLayerForKey`, `Get_LayerValues`/`Get_LayerKeyCount`.
- **Agent enumeration covers BOTH Planner installation paradigms.** Create-style
  planners live in the owner's `FFragment_RecordOfGoapPlanners`; Add-style stamps
  the Planner role directly on the owner and writes NO record entry — and nearly
  every gym installs via `Add`. `CollectSnapshots` therefore walks the record
  view first, then a `FFragment_Goap_Planner_Params` view filtered to
  non-Action-role, non-record-registered entities (the Add-style owners, each
  its own agent). `BuildEntitySnapshot` appends the owner's own Planner role
  before record entries. A record-only walk renders the entire debugger empty
  on Add-style content (2026-07-19 defect).

### Cross-pane channels on the ViewModel

- **Selection**: `SetSelectedEntity` / `SetSelectedActionSet(Planner)` — the
  Sidebar tree and the Squad table's Inspect both drive it.
- **Scrub**: `SetMode(Scrub)` + `SetScrubEventIndex` — timeline diamonds, jump
  buttons, and event-log rows all scrub; the chrome LIVE pill returns to live.
- **Key trace**: `Set_TracedWsKey` / `Get_TracedWsKey` — WS-rail row clicks and
  matrix key rows set it; condition chips (Decision) highlight live, the WS
  rail row tints, the footer shows the trace hint. Cleared on world change.

---

## Sandbox (WS override) semantics

The rail's Sandbox switch arms editing: value-pill flips push single-key
overrides into the reserved `"DebugUI"` layer
(`Push_Override_SingleKey`); switching Sandbox OFF pops the layer. Reads are
shadowed, the base store stays truthful. The alert strip surfaces the blast
radius — a shared WS (subscribers > 1) means every subscribed planner replans
under the sandbox. Gameplay-pushed layers ("FlierAttract", …) are never popped
by the sandbox; per-layer Pop buttons live in the rail's layer stack.

---

## Refresh discipline (unchanged contracts)

- **Hash-debounce everywhere** — every panel computes a content hash and
  early-outs; live values (status, satisfaction, override state, trace
  highlight, editability) are `TAttribute`-bound so they track without
  rebuilds. Never `ChildSlot[...]` per tick.
- **`FCkDebuggerRefreshGate::Should_RefreshNow(WindowId)`** gates the window
  Tick.
- **EndPIE / world teardown**: `HandleWorldTornDown` resets AgentList,
  Sidebar, AgentColumn, DecisionPanel (edited-cost planner handles!),
  TimelineDock, SquadTable, CatalogPanel, GraphPane, then the ViewModel — the
  ViewModel reset broadcasts, which clears the WS rail's cached WS handle
  while the registry is alive. Every new pane holding `FCk_Handle` state must
  join this chain.
- **Stable `TSharedPtr` identity** in the Squad table (rows keyed by planner
  handle) and Agent list. Weak-capture (`WeakRail`/`WeakPanel` — NEVER
  `WeakThis`, it shadows `TSharedFromThis::WeakThis` and C4458 is an error)
  in attribute lambdas; feature handles captured by value + `ck::IsValid`
  checked per read.

---

## Legacy / transitional notes

- `SCkGoapDebugger_AgentListPanel` is constructed but **unslotted** — it still
  receives cross-debugger selection-sync broadcasts. Re-home the sync
  subscription (window- or SquadTable-side) before deleting it.
- `SCkGoapDebugger_Sidebar` still owns an internal history widget that is no
  longer slotted (the TimelineDock replaced it); its tree remains the planner
  selection surface until a chrome planner-picker exists.
- `FCkGoapDebuggerStyle` (legacy token set) was retuned to the Mission Control
  palette (hex literals — static-init-safe). New code should use `CkStyle::`
  tokens + `FCkDebuggerCommonStyle` (glow brushes) directly.
- **Name shortening is the common widget's job.** `SCkDebug_NameLabel`
  (CkDebuggerCommon) renders every pure class-name/tag label (Decision +
  Catalog card titles, plan steps, Planner-tag / World-state settings rows,
  WS-rail header) with depth tuning + »/« expand; composite strings (crumbs,
  Squad chains, timeline/event-log lines, graph text measurement) go through
  `SCkDebug_NameLabel::Get_ShortName`. The old
  `FCkGoapDebugger_NameParams::ComputeDisplayName` is deleted.
- Deleted in the port: `SCkGoapDebugger_Breadcrumb`, `SCkGoapDebugger_PrimaryPane`,
  `SCkGoapDebug_PlanStrip` (superseded by AgentColumn + DecisionPanel), the old
  ModeBar/Toolbar/Legend chrome, and the old right-stack panels
  (WorldStatePanel/StatsPanel/FailureAnalysisPanel → WS rail / nerd strip /
  Decision panel).
- Known data gap: `FCkGoapDebugger_ActionInfo` has no `HasCostProvider` flag —
  the mockup's "cost provider" badge (Decision + Catalog cards) needs a
  collector read of the provider registration.

---

## Common gotchas

- **Anonymous-namespace collisions under unity build** — use per-file named
  namespaces (`ck_goap_debugger_<file>`); the module compiles unity.
- **`UEdGraph::Nodes` iteration** — `for (UEdGraphNode* Node : Graph->Nodes)`,
  never `auto&` (TObjectPtr cast quirks).
- **Refresh thrash** — no unconditional `ChildSlot[...]`/`ClearChildren()` in
  hot paths; gate on hashes.
- **SLeafWidget tooltips** (`SCkDebug_EventTimeline`) — per-marker tooltips go
  through a hover-tracked widget-level tooltip; there are no child widgets to
  attach to.
- **Pause-on** uses `GEditor->PlayWorld->bDebugPauseExecution = true` (the
  blueprint-breakpoint pause), same as CkSmDebugger.

---

## See also

- `CkDebuggerCommon/CLAUDE.md` — shared widget catalog (StatusPill, Chip,
  ValuePill, Switch, MeterBar, Card, AlertRow, UnderlineTabs, Stepper,
  GlowWrap, EventTimeline, Sparkline, SectionHeader …) + safety rules.
- `CkGoap/CLAUDE.md` (in `CkFoundation`) — the Planner/Action model, WS
  override stack, replan policies, always-valid-plan tenet.
- `Mockups/mockup_d_mission_control.html` — the interactive visual spec.
