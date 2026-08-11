# Packaged Graph Debuggers

## Mission

Make the CK ECS, State Machine, Scheduler, and GOAP debuggers available and useful in packaged
Development and DebugGame builds while preserving their existing full GraphEditor experiences in
editor builds.

## Success criteria

- The packaged launcher lists all four tools in the same categories and order as the editor launcher.
- Each tool opens without loading GraphEditor, UnrealEd, or another editor-only module.
- ECS retains its entity tree, inspector, Dashboard, Archetypes, and Activity pages.
- Scheduler retains its frame history, statistics, and processor tree.
- GOAP retains its agent, decision, search trace, timeline, world-state, squad, and catalog surfaces.
- State Machine provides a useful Slate state/transition/history fallback in packaged builds.
- Editor builds retain the current graph pages, graph panes, previews, and visual node factories.
- Development and DebugGame include the tools; Test and Shipping exclude them.

## Constraints

- Intent Debugger is explicitly out of scope because it is not in this checkout yet.
- Do not make a packaged target depend on GraphEditor or UnrealEd.
- Do not place reflected UCLASS declarations inside WITH_EDITOR blocks.
- Do not replace missing tools with launcher-only placeholders.
- The build machine is the packaged compile/runtime acceptance gate; no local build is required.
