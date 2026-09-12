# CkDialogDebugger

**Purpose:** UncookedOnly Slate debugger for the `CkDialog` system. A read-only inspector window listing the
world's dialogue-line registry (lines + banks + readiness) and every dialog emitter (tags, active cooldowns,
last-query pass/fail summary). Opened via the `ck.DialogDebugger` console command or the shared debugger launcher.

**Depends on:** `CkCore`, `CkEcs`, `CkRecord`, `CkDialog`, `CkEntityTag`, `CkDebuggerCommon`, `CkEditorTools`, `CkSlateLayout`, `Projects`
(+ Slate/GraphEditor/UnrealEd editor modules). Mirrors `CkSmDebugger`'s module registration.

---

## Anatomy

- `CkDialogDebugger_Module.h/.cpp` — registers the nomad tab + `ck.DialogDebugger` console command + the shared
  `FCkDebuggerToolRegistry` entry. No `FGraphPanelNodeFactory` (this build ships no UEdGraph — see below).
- `Data/CkDialogDebugger_Types.h` — POD view structs (`FCkDialogDebugger_{LineInfo,CooldownInfo,QueryHistoryEntry,EmitterInfo,RegistrySnapshot}`).
- `Data/CkDialogDebugger_DataCollector.h/.cpp` — rebuilds a snapshot each refresh: reads the registry subsystem
  (`Get_AllLines`/`Get_RegisteredBanks`/`Get_IsReady`) and walks emitter entities read-only
  (`TransientEntity.View<FFragment_DialogEmitter_Params, _Current>()`), reading the
  `FFragment_DialogEmitter_Debug` ring for query history.
- `Window/SCkDialogDebuggerWindow.h/.cpp` — `SCkDebugger_WindowBase` subclass. Gated `Tick`
  (`FCkDebuggerRefreshGate::Should_RefreshNow`) collects a live snapshot and drives the retained authored
  four-region UI: cooldown controls, runtime commands, search, and main content. The main region owns the live
  collection projection, grouped cooldown records, empty/count state, and diagnostic text. The authored path has
  focused real-RHI production-window coverage for controls, collection identity, reload, and teardown.

---

## Deferred: the bipartite graph

The plan's Part 2 called for a bipartite Event/Line UEdGraph with a live overlay (per CkSmDebugger's graph). That
`UEdGraph` + `SGraphNode` layer (~14 files) is **not** in this build: it is the most intricate, Slate-heavy, and
unverifiable-without-an-editor part, and a single error there would break the whole `CkGameplayDebugger` editor
build. The inspector window here delivers the core value (registry + emitter + cooldown + query-history
introspection); the graph is a follow-up. The runtime data-hook it would need already exists
(`ck::FFragment_DialogEmitter_Debug`).

---

## Lifetime / safety

- The window is a `SCkDebugger_WindowBase` — call `Register_WithGate()` in `Construct`; the base auto-unregisters
  from the refresh gate in its destructor. Synchronous session/world invalidation must clear the live collection,
  disable world-gated commands, and advance the view generation before detaching or replacing authored widgets.
  Bindings and actions capture only weak world/owner state plus the generation gate; they must reject after teardown
  or target replacement. Release retained views, collections, and world handles from the destructor before the
  owner disappears. A fresh collector snapshot per tick does not replace this explicit EndPIE/session lifetime
  handling.
- All world access is read-only; the collector never mutates ECS state.
