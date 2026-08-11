# Runtime graph parity — mission brief

> **Written:** 2026-08-10. Stable content only; current state lives in [PROGRESS.md](PROGRESS.md).
> **This doc dies when:** all graph debuggers use the shared runtime graph surface in Editor and packaged Development builds, and the permanent module documentation records the resulting contract.

## Goal

The ECS, Scheduler, GOAP, and State Machine debuggers expose the same graph content, visual states,
layout, controls, selection behavior, live updates, and lifecycle behavior in Editor and packaged
Development/DebugGame builds. Both targets use the same runtime-safe Slate renderer so parity is a
code-level invariant rather than two independently maintained implementations.

## Success criteria

1. Every graph pane and graph-related control currently visible in Editor is present in packaged builds in the same location.
2. Given the same debugger snapshot and settings, Editor and packaged builds produce the same nodes, edges, labels, dimensions, positions, colors, badges, tooltips, and selection state.
3. Pan, cursor-centered zoom, frame-all, node selection, empty-space deselection, double-click behavior, context-copy actions, and cross-pane selection synchronization match for each debugger.
4. Live values update without recreating unchanged node widgets or resetting pan/zoom; topology changes preserve stable identity and apply each debugger's existing refit policy.
5. State Machine parity includes compound states, task rows, transition badges and conditions, self/reverse edges, history and scrub coupling, breakpoints, details, timeline, preview, and keyboard commands.
6. Packaged Game targets do not compile or link GraphEditor, UnrealEd, UEdGraph authoring APIs, graph factories, or editor connection policies.
7. Automated snapshot/layout/interaction contracts pass, a Development Editor build passes, and the BM Development packaged build is manually verified against the same parity checklist.

## Constraints and locked decisions

| Decision | Choice | Why |
|---|---|---|
| Renderer ownership | One shared runtime Slate graph surface in CkDebuggerCommon | A single renderer makes Editor/package parity enforceable. |
| Editor behavior | Migrate Editor to the shared renderer | Retaining SGraphEditor in Editor would create two rendering and interaction implementations. |
| Data boundary | Value-only snapshots with stable IDs | Runtime widgets must not retain UEdGraph objects or stale ECS handles across session teardown. |
| Layout | Preserve each debugger's existing algorithm and measured footprints | Replacing topology with a generic approximation would violate visual parity. |
| Scope | Read-only diagnostic graphs | Existing debugger graphs do not require authoring, transactions, or link mutation. |
| Acceptance authority | Editor behavior before this campaign is the oracle; BM proves packaged compilation and QA performs the final side-by-side observation | Static tests cannot prove pixel and interaction parity. |

## Non-goals

- Packaging Unreal's GraphEditor or UnrealEd modules; their target contract forbids Game targets.
- Adding graph authoring, pin dragging, node creation, transactions, or asset serialization.
- Treating the current list-only packaged fallbacks as acceptable parity.
- Changing gameplay or debugger data collection merely to simplify rendering.

## Reading list

- [PLAN.md](PLAN.md)
- `CkDebuggerCommon/Graph/CkDebugGraphLayout.*`
- ECS: `CkDebuggerPage_Overview.*`, `CkEcsDebugGraph.*`, `SGraphNode_EcsEntity.*`, `CkEcsDebugConnectionPolicy.*`
- Scheduler: `CkSchedulerDebuggerPage_TreeView.*`, `CkSchedulerDebugGraph.*`, `SGraphNode_SchedulerProcessor.*`
- GOAP: `SCkGoapDebugger_GraphPane.*`, `CkGoapDebugGraph.*`, `SGraphNode_Goap*`, `CkGoapDebugConnectionPolicy.*`
- State Machine: `SCkSmDebuggerWindow.*`, `CkSmDebugGraph.*`, `SGraphNode_Sm*`, `CkSmDebugConnectionPolicy.*`, `SCkSmDebugger_PreviewPane.*`

## Things ruled out — do not re-investigate

| Ruled out | Why | Evidence |
|---|---|---|
| Add GraphEditor to packaged Game targets | GraphEditor depends on UnrealEd; UnrealEd rejects non-editor targets, and UEdGraphNode authoring APIs compile out under `WITH_EDITOR`. | BM Game-target compiler failures and engine module rules. |
| Keep SGraphEditor in Editor and independently imitate it in packaged builds | Separate renderers cannot guarantee exact long-term parity. | Product requirement locks identical behavior in both targets. |
| Preserve only topology while simplifying cards or interactions | The requirement explicitly includes everything as it was, not merely equivalent data. | Success criteria 1–5. |
