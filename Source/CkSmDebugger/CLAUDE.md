# CkSmDebugger — module notes

## Handle lifetime contract across PIE

CkSmDebugger UI state (Slate window, ViewModel, DataCollector, runtime graph model, and the
editor-only legacy UEdGraph adapter) can outlive PIE session boundaries. ECS entities and
their backing `FCk_Registry` do not — each PIE session brings up a new world
with a new registry, and the prior registry is destroyed on PIE Stop.

Every `FCk_Handle*` holds a `TOptional<FCk_Registry>` **by value**. If a
handle outlives its source registry, its destructor will release a shared ref
into freed memory and crash. `operator==` on handles is also unsafe post-
teardown because it constructs temporary `FCk_Handle` copies that destruct at
end-of-statement.

### Rule

Any `FCk_Handle*` stored in a field that outlives a PIE session **must** be
cleared from `SCkSmDebuggerWindow::HandleWorldTornDown()`. That function runs
on `FEditorDelegates::EndPIE` and `BeginPIE`, while the registry is still live,
so assignment-replacing or `.Empty()`-ing handle-bearing containers releases
their shared refs cleanly.

Handle-bearing state to audit when adding new fields:

- Direct `FCk_Handle*` members on persistent UObjects / Slate widgets / shared
  view-model structs.
- `TArray` / `TMap` whose value types are `FCkSmDebugger_*Info` structs —
  those structs embed handles (see `CkSmDebugger_Types.h`).
- Cached "last seen" or "selection" handles on UEdGraph subclasses.

Currently covered by the EndPIE path:

- `UCkSmDebugGraph::_CachedSubSmOwner`, `_TransitionData`, `_CachedSubSmData`
  (cleared by `ForceRebuild()`).
- `FCkSmDebugger_ViewModel::_CurrentSmInfo`, `_SelectedSmHandle`
  (cleared by `Reset_ForWorldChange()`).
- `FCkSmDebugger_DataCollector::_StateMachines`
  (cleared by `Reset()`).
- `FCkSmRuntimeGraphFacade` and `SCkSmRuntimeGraph` handle-bearing scene data
  (cleared by `ResetForWorldChange()` / `Clear()`).

### Symptom if broken

Editor crash on second (or later) PIE start with the debugger window open.
Stack shows `~FCk_Handle → TOptional<FCk_Registry>::DestroyValue →
TReferenceControllerBase::ReleaseSharedReference` with `SharedReferenceCount=0`
at an access violation. Entry point is usually
`UCkSmDebugGraph::RebuildFromSmInfo` via the window's `Tick`.

Fix: add the new field's clear/reset to `HandleWorldTornDown()` (directly or
via an existing cascade like `ForceRebuild`/`Reset_ForWorldChange`/`Reset`).
