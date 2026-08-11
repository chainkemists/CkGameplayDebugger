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
- The full packaged-tool completion audit also closes three non-graph parity gaps: GOAP pause-on events pause the
  selected packaged world, Crowd omits Editor Preview controls that cannot operate outside the Editor, and the ECS
  dynamic-fragment inspector resolves cooked fragment/property/enum labels through CkDynamic's value-owned schema.
- Static verification is clean: module JSON parses as 18 DeveloperTool + 3 Runtime, editor-only dependency
  surfaces are guarded, reflected graph classes remain UHT-visible, and `git diff --check` passes.
- The previously published graph/runtime-view revision passed Development Editor and Win64 Development Game builds
  through UnrealToolbox, plus a clean compile/cook/stage/pak/archive package. Tests were not launched by explicit CTO
  instruction.

## In flight

- Compiling the completion-audit fixes in Development Editor and Win64 Development Game, then publishing the
  CkFoundation/CkGameplayDebugger commits and refreshed root gitlinks on `feature/packaged-debugger-tools`.

## Blockers

- None. The new completion-audit slice still requires compile evidence and matched Editor/package observation.

## Next step

- Build the completion-audit slice, refresh the package, and complete the matched visual/interaction checklist for the
  high-risk tools before declaring exact parity complete.
