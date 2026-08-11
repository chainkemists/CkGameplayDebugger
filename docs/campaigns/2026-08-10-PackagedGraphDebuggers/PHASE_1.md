# Phase 1: Runtime-safe packaged surfaces

## Goal

Remove editor-module closure from the four debugger modules and provide the same active runtime graph UI
in Editor and packaged Development builds.

## Steps

- Separate non-reflected GraphEditor surfaces from reflected runtime-safe graph models.
  -> verify: non-editor source closure contains no GraphEditor, UnrealEd, SGraphEditor, SGraphNode,
  FEdGraphUtilities, or FConnectionDrawingPolicy references.
- Render ECS, Scheduler, and GOAP graphs through the shared runtime canvas in both targets.
  -> verify: their packaged construction paths contain the same active graph panes and controls without editor graph types.
- Render State Machine graph, details, timeline, history, Preview, and Test mode through runtime-safe Slate.
  -> verify: packaged construction exposes the shared window without GraphEditor, UnrealEd, or UEdGraph authoring APIs.
- Promote all four descriptors to DeveloperTool and update packaging automation/docs.
  -> verify: descriptor tests expect cooked Development inclusion and Test/Shipping exclusion.
- Publish the debugger tip and root gitlink.
  -> verify: both feature branches are exact with their remotes and the root gitlink points at the pushed tip.

## Exit criteria

- Static editor-dependency audit is clean for non-editor paths.
- Existing editor graph registration remains present behind WITH_EDITOR.
- Diff checks are clean and unrelated workspace dirt is excluded.
- Development Editor and Game targets compile through UnrealToolbox.
- BM packaged Development build lists and opens the four tools, or returns a new concrete runtime/visual error.
