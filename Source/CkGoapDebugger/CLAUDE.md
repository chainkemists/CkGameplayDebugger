# CkGoapDebugger

**Purpose:** Editor-time debugger UI for the `CkGoap` ActionSet/Action runtime. Shows per-entity Goap state — every registered ActionSet, the catalog of Actions inside each one, the active chain (root → leaf), per-Action plan/goal/WorldState/diagnostics, the action graph rendered as a layered DAG, and a history scrub for time-travel inspection of plan and chain transitions. Also exposes a small summary card in the CkEcsDebugger entity inspector with a one-click jump to the standalone window.

**Depends on:** `CkCore`, `CkEcs`, `CkAStar`, `CkGoap`, `CkEntityExtension`, `CkRecord`, `CkLabel`, `CkDebuggerCommon`, `GraphEditor`, and the editor-only Slate stack (`Slate`, `SlateCore`, `EditorStyle`, `ToolMenus`, `WorkspaceMenuStructure`, `AppFramework`).
**Private dep:** `CkEcsDebugger` — for `FCkDebuggerInspectorRegistry` registration of the inspector gateway. Kept private so consumers of `CkGoapDebugger` don't transitively pull the ECS-debugger module.
**Used by:** No runtime code. The debugger is invoked from the editor menu or the `ck.GoapDebugger` console command.

---

## Where it lives

- **Editor-only Slate**, loaded as part of the `CkGameplayDebugger` plugin.
- **Menu:** `Window → Developer Tools → Debug → CK GOAP Debugger`.
- **Console:** `ck.GoapDebugger` toggles the tab. `ck.GoapDebugger 1` opens, `0` closes.
- **External entry point:** `SCkGoapDebuggerWindow::OpenForEntity(InEntity)` invokes the tab and pre-selects the given entity — used by the inspector gateway's "Open in Goap Debugger" button.

---

## Architecture in one diagram

```
   DATA LAYER                          VIEW LAYER                       OUTER SHELL
  ──────────────                      ─────────────                    ─────────────

  FCkGoapDebugger_DataCollector       FCkGoapDebugger_ViewModel        SCkGoapDebuggerWindow
   (singleton, file-static)            (held by the window)             (Nomad tab content)
   - CollectSnapshots(World)           - selection: Entity /            - hosts Sidebar
   - per-entity history ring             ActionSet / Action               + Breadcrumb
   - PIE BeginPIE/EndPIE clears        - Live / Scrub mode               + PrimaryPane
     prev-snapshot caches              - OnChanged broadcast             + GraphPane
                                       - Tick(World) re-pulls and        + WorldStateRail
                                         hash-debounces broadcast        - PIE Begin/End
                  │                                  │                     teardown path
                  └─── Tick from window ─────────────┘                     (HandleWorldTornDown)
                                       │
                                       │  OnChanged
                                       ▼
   ┌──────────────────────────────────────────────────────────────────────┐
   │                            CENTER COLUMN                              │
   │  SCkGoapDebugger_Breadcrumb     — active-chain segments               │
   │  SCkGoapDebugger_PrimaryPane    — header / plan strip / goal /        │
   │                                   drilldown / right rail              │
   │  SCkGoapDebugger_GraphPane      — UCkGoapDebugGraph + SGraphEditor    │
   │                                                                       │
   │  SCkGoapDebugger_Sidebar        — ActionSet tree + history list +     │
   │                                   scrub track                         │
   │  SCkGoapDebugger_WorldStateRail — WS keys with recently-changed       │
   │                                   indicators                          │
   └──────────────────────────────────────────────────────────────────────┘

   GRAPH LAYER (UEdGraph)                          INSPECTOR GATEWAY (D7)
  ───────────────────────                         ────────────────────────
   UCkGoapDebugGraph                              FCkGoapInspector_Gateway
     RebuildFromSnapshot(ActionSet, Selected)       (ICkDebuggerComponentInspector_Base)
     - layered DAG over preconditions/effects       CK_REGISTER_DEBUGGER_INSPECTOR
     - Action nodes + Goal anchor                   - CanInspect: UCk_Utils_Goap_UE::Has
     - cycle fallback: single-row layout            - Build_Inspector: hosts
                                                       SCkGoapDebugger_InspectorGateway
   UCkGoapDebugNode_Action / UCkGoapDebugNode_Goal
     Snapshot of FCkGoapDebugger_ActionInfo +      SCkGoapDebugger_InspectorGateway
     render hints (IsInPlan, PlanStepIndex,          - compact card: ActionSet list,
     IsSelected, IsFailureBlocked)                     active chain, leaf action,
                                                       plan preview
   SGraphNode_GoapAction / SGraphNode_GoapGoal       - "Open in Goap Debugger" button
     Slate visuals (rounded border tinted by         - independent snapshot fetch
     selection/plan/failure/composite state)           (does NOT subscribe to the
                                                       standalone window's ViewModel)
   FCkGoapDebugGraphFactory
     FGraphPanelNodeFactory mapping
     U*Node → SGraphNode_Goap*
     (registered in StartupModule)

   FCkGoapDebugConnectionPolicy
     FConnectionDrawingPolicy override
     for plan-edge tint + thickness
```

All Slate widgets are pure mirrors of the snapshot types. The data layer never touches Slate; the view layer never touches the `CkGoap` ECS fragments directly.

---

## Key files

### Module shell

| File | Role |
|---|---|
| `CkGoapDebugger.Build.cs` | Module deps (public deps listed above; `CkEcsDebugger` private). |
| `CkGoapDebugger_Module.{h,cpp}` | `IModuleInterface`. Registers Nomad tab spawner, `FCkGoapDebuggerStyle`, `FCkGoapDebugger_DataCollector`, graph node factory. Hosts the `ck.GoapDebugger` console command. |
| `Public/CkGoapDebugger/CkGoapDebuggerStyle.{h,cpp}` | `FSlateStyleSet` with mockup-F palette (Color_Bg_*, Color_Status_*), brushes, text styles, layout constants. |

### Data

| File | Role |
|---|---|
| `Data/CkGoapDebugger_Types.h` | Plain (non-UObject, non-USTRUCT) display structs: `FCkGoapDebugger_Condition`, `_WorldStateEntry`, `_CycleInfo`, `_ActionInfo`, `_ActionSetInfo`, `_EntitySnapshot`, `_HistoryEvent`. Lifetime contract documented at top. |
| `Data/CkGoapDebugger_DataCollector.{h,cpp}` | Singleton (file-static state). `CollectSnapshots(World)` walks every entity with a Goap root and produces `FCkGoapDebugger_EntitySnapshot` list. Diffs against prev tick to populate per-entity history ring buffers (`GetHistory`, `ClearHistory*`). Initialize/Shutdown from the module; PIE Begin/End clears prev-snapshot + history maps. |

### ViewModel

| File | Role |
|---|---|
| `ViewModel/CkGoapDebugger_ViewModel.{h,cpp}` | Holds selection (Entity / ActionSet / Action), Live/Scrub mode, scrub event index, the latest snapshot batch. `Tick(World)` re-pulls and hash-debounces `OnChanged`. `Reset_ForWorldChange` drops handle-bearing state. `ForceReplanOnSelected` issues `Request_Plan` on the selected Action. |

### Window (outer shell + center column)

| File | Role |
|---|---|
| `Window/SCkGoapDebuggerWindow.{h,cpp}` | Top-level Slate. Owns ViewModel, ticks it, hosts sidebar + center column + WS rail. Mode bar / toolbar (entity picker, Live/Scrub, Force replan, Simulate PlanFailed stub) / legend. Editor PIE Begin/End delegates call `HandleWorldTornDown`. Static `OpenForEntity` for external entry. |
| `Window/SCkGoapDebugger_Breadcrumb.{h,cpp}` | "Chain: [Root] ▸ [Mid] ▸ [Leaf]" with click-to-select segments. Hash-debounced rebuild. |
| `Window/SCkGoapDebugger_PrimaryPane.{h,cpp}` | Header + plan strip + goal section + drilldown + right rail. Right rail has Normal vs Failure variants. |
| `Window/SCkGoapDebugger_PlanStrip.{h,cpp}` | Horizontal card row of the selected Action's `Plan[]` entries. Self-refreshes on ViewModel changes. |
| `Window/SCkGoapDebugger_Sidebar.{h,cpp}` | STreeView (ActionSets → ordered chain) + SListView (history, reverse chronological) + scrub track. Click-to-select drives ViewModel; history row selection drives scrub. |
| `Window/SCkGoapDebugger_GraphPane.{h,cpp}` | Hosts `UCkGoapDebugGraph` (AddToRoot in Construct, RemoveFromRoot in dtor) inside an `SGraphEditor`. Rebuilds on `OnChanged`, restores selection by Action handle. `Reset_ForWorldChange` clears the graph during PIE teardown. |
| `Window/SCkGoapDebugger_WorldStateRail.{h,cpp}` | Right rail: resolved WS keys for the selected Action (or ActionSet WS as fallback). Recently-changed entries are tinted + star-prefixed. |
| `Window/SCkGoapDebugger_InspectorGateway.{h,cpp}` | The compact summary card embedded in CkEcsDebugger's entity inspector. **Does not** subscribe to the standalone window's ViewModel; pulls its own snapshot from the DataCollector and hash-debounces rebuilds. |

### Graph layer

| File | Role |
|---|---|
| `Graph/CkGoapDebugGraph.{h,cpp}` | `UEdGraph` subclass. `RebuildFromSnapshot` lays out a layered DAG (precondition → effect chain) with goal anchor on the right. Cycle fallback: single-row layout. `ForceClear` drops nodes during PIE teardown. |
| `Graph/CkGoapDebugNode_Action.{h,cpp}` | One per catalog Action. Pins: one input per precondition, one output per effect. Stores `FCkGoapDebugger_ActionInfo` snapshot + render hints (IsInPlan, PlanStepIndex, IsSelected, IsFailureBlocked). |
| `Graph/CkGoapDebugNode_Goal.{h,cpp}` | Single anchor node placed one column right of the deepest action layer. Pins: one input per goal condition. |
| `Graph/SGraphNode_GoapAction.{h,cpp}` | Slate visual. Rounded border tinted by state (amber selected / red failure / blue in-plan / purple composite / dim default). Header (class name + cost), composite badge, precondition+effect columns, plan-step badge. |
| `Graph/SGraphNode_GoapGoal.{h,cpp}` | Slate visual for the goal anchor. |
| `Graph/CkGoapDebugGraphFactory.{h,cpp}` | `FGraphPanelNodeFactory` mapping the two node UClasses to their Slate visuals. Registered once in `StartupModule`. |
| `Graph/CkGoapDebugGraphSchema.{h,cpp}` | `UEdGraphSchema` override — read-only graph (no user-initiated connections / deletions). |
| `Graph/CkGoapDebugConnectionPolicy.{h,cpp}` | `FConnectionDrawingPolicy` override for plan-edge tint + thickness. |

### Inspector

| File | Role |
|---|---|
| `Inspector/CkGoapInspector_Gateway.{h,cpp}` | `ICkDebuggerComponentInspector_Base` subclass. Registered with `CK_REGISTER_DEBUGGER_INSPECTOR(FCkGoapInspector_Gateway)`. `CanInspect` returns true when `UCk_Utils_Goap_UE::Has(Entity)`. `Build_Inspector` hosts an `SCkGoapDebugger_InspectorGateway` widget. |

---

## Extension points

### Add a custom Action visual

The graph uses `FGraphPanelNodeFactory` so a downstream module can ship a richer `SGraphNode_GoapAction` variant for a project-specific Action subclass:

1. Subclass `SGraphNode` (or extend `SGraphNode_GoapAction` if you only need to override `BuildBody`).
2. Register a new `FGraphPanelNodeFactory` via `FEdGraphUtilities::RegisterVisualNodeFactory` during your module's `StartupModule`. Return `nullptr` from `CreateNode` for nodes you don't own — the existing `FCkGoapDebugGraphFactory` will handle them.
3. Choose your discriminator inside `CreateNode`: typically `Cast<UCkGoapDebugNode_Action>(InNode)` then inspect `Get_Snapshot().ActionClass` or `ActionTag`.

Unregister the factory in your `ShutdownModule` (mirror the existing pattern).

### Add an extra section to the inspector gateway

The gateway is currently a single `ICkDebuggerComponentInspector_Base` that hosts one widget. If you want to add an additional ECS-inspector section that surfaces something Goap-adjacent (e.g. an EQS-driven goal hint), register a separate inspector via `CK_REGISTER_DEBUGGER_INSPECTOR`. Use the existing `FCkGoapInspector_Gateway` as the template — Sort priorities live in `Get_SortPriority()` (Goap gateway is `65`).

Do **not** add it as a child widget inside `SCkGoapDebugger_InspectorGateway` — that widget is owned by the gateway's lifecycle and won't see CkEcsDebugger selection changes outside the `Set_Entity` re-entry point.

---

## Anti-patterns

- **Per-frame allocations inside `FCkGoapDebugger_DataCollector::CollectSnapshots`.** It runs on every UI Tick over every Goap entity in the world. Keep scratch buffers `static` (they're already file-statics) or reuse pre-sized `TArray`s — never `MakeShared` per Action.
- **Holding `FCk_Handle*` across PIE boundaries.** Every handle holds a `TOptional<FCk_Registry>` by value. The data layer clears history maps on `BeginPIE`/`EndPIE`, the ViewModel calls `Reset_ForWorldChange`, the sidebar / graph pane drop their cached structures — keep new fields participating in this teardown cascade. See the sister `CkSmDebugger/CLAUDE.md` for the canonical write-up.
- **Subscribing the inspector gateway widget to the standalone window's ViewModel.** The gateway is owned by CkEcsDebugger's inspector lifecycle, not by the Goap window. It pulls its own snapshot from the DataCollector and hash-debounces rebuilds. Cross-wiring causes refresh loops and outlives the standalone tab.
- **Iterating `UEdGraph::Nodes` with `auto*`.** `UEdGraph::Nodes` is `TArray<TObjectPtr<UEdGraphNode>>`. `for (auto* Node : Graph->Nodes)` compiles but `Node` is a `TObjectPtr`, not a raw pointer, which breaks downstream `Cast<>` and pointer arithmetic. Use `for (UEdGraphNode* Node : Graph->Nodes)`.
- **`FString::FindLastChar(c, OutIdx)`.** The second parameter is `int32&`. Always initialise the locals: `int32 OutIdx = INDEX_NONE;` before passing in — uninitialised reads otherwise.
- **Anonymous-namespace function names clashing across `.cpp` files.** Adaptive Unity merges anonymous namespaces in the same translation unit batch; two `.cpp` files with `namespace { auto MakeRow(...) { ... } }` and the same `MakeRow` signature collide with C2374/C2086. Prefix per-file (`MakeRow_PrimaryPane`, `MakeRow_Sidebar`) or move them into method bodies. Recurring offenders in similar modules: `MakeBadge`, `MakeRow`, `MakeHeader`, `Duration_OneFrame`, `Thickness`.
- **Calling `_Graph->NotifyGraphChanged()` mid-rebuild.** `UCkGoapDebugGraph::SetSuppressNotifications(true)` is the recommended gate while batch-clearing and re-populating nodes. The pane already uses this; preserve it when adding new rebuild paths.
- **Returning a Slate widget tree that captures `this` by raw pointer in `OnClicked` lambdas without weak-ptr guards.** Pane refresh paths recreate widget trees; old lambdas can fire from queued events after the panel was rebuilt. Use `TWeakPtr<SCkGoapDebugger_XXX>(SharedThis(this))` and an `IsValid()` guard.

---

## Module conventions

- **Plain display types, not USTRUCT.** Everything in `CkGoapDebugger_Types.h` is a `struct {}` — no reflection, no serialisation. They mirror live ECS state for a single frame of rendering.
- **Hash-debounce all `RefreshFromViewModel`.** Every view widget computes a structural hash (handle set + selection) and skips rebuild when unchanged. Tick is cheap; rebuild is not.
- **Subscribe via `ViewModel->OnChanged` from `Construct`; unbind in destructor.** All view widgets follow this pattern with a stored `FDelegateHandle`.
- **Use the style set.** Colours and layout constants come from `FCkGoapDebuggerStyle::Color_*` and `Padding_*` / `CornerRadius_*` — do not hardcode `FLinearColor` or `FMargin` in widget code.

---

## See also

- `Plugins/CkFoundation/Source/CkGoap/CLAUDE.md` — the runtime model the debugger reflects (Add vs Create, ActionSet hierarchy, replan policy, world-state resolution, fragment table).
- `Plugins/CkGameplayDebugger/Source/CkSmDebugger/CLAUDE.md` — sister debugger; canonical write-up of the PIE handle-lifetime contract that this module follows.
- `Plugins/CkGameplayDebugger/Source/CkEcsDebugger/` — host of `FCkDebuggerInspectorRegistry` and the inspector entry point the gateway plugs into.
- `Plugins/CkGameplayDebugger/Source/CkDebuggerCommon/` — `SCkDebugger_WindowBase` and shared row patterns; read its CLAUDE.md before authoring new list/tree rows.
