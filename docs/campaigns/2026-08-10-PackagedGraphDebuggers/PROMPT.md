# Packaged Graph Debuggers

## Mission

Make the CK ECS, State Machine, Scheduler, and GOAP debuggers available and useful in packaged
Development and DebugGame builds. Editor and packaged targets must use the same runtime-Slate graph
surfaces so behavior cannot drift between two renderers.

## Success criteria

- The packaged launcher lists all four tools in the same categories and order as the editor launcher.
- Each tool opens without loading GraphEditor, UnrealEd, or another editor-only module.
- ECS retains its entity tree, inspector, Dashboard, Archetypes, and Activity pages.
- Scheduler retains its frame history, statistics, and processor tree.
- GOAP retains its agent, decision, search trace, timeline, world-state, squad, and catalog surfaces.
- State Machine provides its graph, state/transition/history, Preview, Test, breakpoint, and timeline surfaces in packaged builds.
- Editor builds use the same active runtime graph surfaces; legacy GraphEditor sources remain editor-only reference code.
- Development and DebugGame include the tools; Test and Shipping exclude them.

## Constraints

- Intent Debugger is explicitly out of scope because it is not in this checkout yet.
- Do not make a packaged target depend on GraphEditor or UnrealEd.
- Do not place reflected UCLASS declarations inside WITH_EDITOR blocks.
- Do not replace missing tools with launcher-only placeholders.
- UnrealToolbox Editor and Win64 Development Game builds are required before publishing; the build-machine package is the final runtime/visual acceptance gate.
