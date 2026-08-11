# Progress: Packaged Graph Debuggers

## Done

- Screenshot comparison identified exactly four in-scope omissions: ECS, State Machine, Scheduler, and GOAP.
- Root cause confirmed: all four descriptors are UncookedOnly and each module unconditionally depends on
  GraphEditor; GraphEditor unconditionally depends on UnrealEd and cannot instantiate for Game targets.
- UHT constraint confirmed: reflected graph classes must not be wrapped wholesale in WITH_EDITOR.
- Architecture locked: all active graph panes use runtime-safe value models and the shared Slate graph canvas
  in both Editor and packaged builds. GraphEditor sources and dependencies remain editor-only reference code.
- ECS retains its Graph page plus Dashboard, Archetypes, Activity, entity tree, and inspector.
- Scheduler retains its processor tree, inspector, and selected-processor dependency graph.
- GOAP retains Plan Graph plus agent, decision, search, timeline, world-state, squad, catalog, and inspector surfaces.
- State Machine uses the shared graph window in both targets, including world and state-machine selection,
  state/transition/compound cards, details, history/scrub/timeline, runtime pause, Preview, and Test mode.
- All four module descriptors are DeveloperTool and the launcher descriptor test expects Development cooked
  inclusion with Test and Shipping exclusion.
- Static verification is clean: module JSON parses as 17 DeveloperTool + 3 Runtime, editor-only dependency
  surfaces are guarded, reflected graph classes remain UHT-visible, and `git diff --check` passes.
- Development Editor and Win64 Development Game builds succeed through UnrealToolbox. Deterministic coverage
  compiles in both targets; tests were not launched by explicit CTO instruction.

## In flight

- Publishing the debugger commit and root gitlink on `feature/packaged-debugger-tools`.

## Blockers

- None for compilation. Matched Editor/package visual and interaction acceptance remains for the BM output.

## Next step

- Build packaged Development on the BM, open all four graph tools, and complete the matched visual/interaction checklist.
