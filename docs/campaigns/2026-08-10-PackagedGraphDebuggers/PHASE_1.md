# Phase 1: Runtime-safe packaged surfaces

## Goal

Remove editor-module closure from the four debugger modules and provide useful packaged UI without
regressing editor graph behavior.

## Steps

- Separate non-reflected GraphEditor surfaces from reflected runtime-safe graph models.
  -> verify: non-editor source closure contains no GraphEditor, UnrealEd, SGraphEditor, SGraphNode,
  FEdGraphUtilities, or FConnectionDrawingPolicy references.
- Retain ECS, Scheduler, and GOAP non-graph panels in packaged builds.
  -> verify: their packaged construction paths contain the expected pages/panels and omit only graph UI.
- Add a State Machine packaged list/detail fallback using its existing collector/view-model data.
  -> verify: the fallback exposes target selection, active state, transitions, and history without graph types.
- Promote all four descriptors to DeveloperTool and update packaging automation/docs.
  -> verify: descriptor tests expect cooked Development inclusion and Test/Shipping exclusion.
- Publish the debugger tip and root gitlink.
  -> verify: both feature branches are exact with their remotes and the root gitlink points at the pushed tip.

## Exit criteria

- Static editor-dependency audit is clean for non-editor paths.
- Existing editor graph registration remains present behind WITH_EDITOR.
- Diff checks are clean and unrelated workspace dirt is excluded.
- BM packaged Development build lists and opens the four tools, or returns a new concrete compiler/runtime error.
