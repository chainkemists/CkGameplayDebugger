# CkAiDebugger

## Purpose

`CkAiDebugger` is the low-noise cross-system AI overview. It complements rather than replaces GOAP, State Machine,
Crowd, AStar, and ECS debuggers.

## Architecture contracts

- Build selected-entity facts through `CkEntityDebugOverlay` providers and the authored AI layout. Do not add direct
  GOAP, State Machine, Crowd, Objective, or navigation fragment reads here.
- Reuse `FCkCrowdDebugger_ViewModel` plus `SCkCrowdDebugger_3dViewport` for value-only roster/spatial snapshots. Do not
  copy Crowd collectors, scene adapters, or rendering.
- Every reusable row, card, control, or list belongs in `CkDebuggerCommon`, even when this is initially its only
  consumer. The AI window is composition/layout only.
- Use `SCkDebug_WindowChrome::CommonActionsContent` for the shared picker. World speed and launcher access are supplied
  automatically by WindowChrome.
- Clear all `FCk_Handle` state on `DebugSessionLifecycle` invalidation and release the handle-bearing Slate tree on
  `OnEnginePreExit` before registry/module teardown.
- Keep the module `DeveloperTool`: Development/DebugGame only, not Test/Shipping.

## Common overview surfaces

- `SCkDebug_EntityHealthList` — selectable NPC health roster.
- `SCkDebug_StageStrip` — fixed decision-to-motion stage cards with live attributes.
- `SCkDebug_BehaviorOverridePanel` — generic session-only behavior controls.
- `SCkDebug_EvidenceList` — keyed Current Evidence plus GOAP/State Machine source hierarchies; retain empty valid source
  sections in the AI model so every nested runtime instance remains visible.
- `SCkDebug_EventLog` — added/changed/resolved cross-system deltas only; selecting an entity seeds without a snapshot flood.
- `SCkDebug_EntityDebuggerLinks`, `SCkDebug_EntityRef` — specialist drill-down and selected identity.

## Layout contract

- Every major boundary is a 5px `SSplitter`: roster/detail, overview/diagnostics, GOAP/State Machine,
  current-evidence/events, and diagnostics/spatial.
- Do not add slot padding between peer panes. Cards own their internal padding; Spatial Evidence receives the larger
  lower-workspace share rather than a narrow full-height rail.
