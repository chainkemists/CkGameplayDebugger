# Progress: Packaged Graph Debuggers

## Done

- Screenshot comparison identified exactly four in-scope omissions: ECS, State Machine, Scheduler, and GOAP.
- Root cause confirmed: all four descriptors are UncookedOnly and each module unconditionally depends on
  GraphEditor; GraphEditor unconditionally depends on UnrealEd and cannot instantiate for Game targets.
- UHT constraint confirmed: reflected graph classes must not be wrapped wholesale in WITH_EDITOR.
- Architecture locked: keep reflected UEdGraph model classes runtime-safe; compile non-reflected GraphEditor
  widgets/factories/panes only for editor; provide non-graph packaged construction paths.
- ECS now omits only its Graph page in packaged targets; Dashboard, Archetypes, Activity, entity tree,
  and inspector remain available.
- Scheduler keeps its processor tree and inspector in packaged targets and substitutes a clear message
  for the selected processor's dependency graph.
- GOAP omits only Plan Graph in packaged targets; agent, decision, search, timeline, world-state, squad,
  catalog, and inspector surfaces remain available.
- State Machine uses a packaged runtime-Slate window with world and state-machine selection, live status,
  states, transitions and condition results, and transition history. Editor retains the existing graph window.
- All four module descriptors are DeveloperTool and the launcher descriptor test expects Development cooked
  inclusion with Test and Shipping exclusion.
- Static verification is clean: module JSON parses as 17 DeveloperTool + 3 Runtime, editor-only dependency
  surfaces are guarded, reflected graph classes remain UHT-visible, and `git diff --check` passes.

## In flight

- Publishing the debugger commit and root gitlink on `feature/packaged-debugger-tools`.

## Blockers

- None locally. Packaged compile/runtime verification is intentionally delegated to the BM; static descriptor
  eligibility does not prove the complete UBT dependency graph compiles.

## Next step

- Build packaged Development on the BM, open all four tools from the launcher, and report any concrete
  compiler or runtime error against the published tips.
