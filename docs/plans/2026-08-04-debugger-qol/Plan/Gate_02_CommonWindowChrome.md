# Gate 02 - Common window chrome

> **Status:** Implementation verified; editor acceptance pending
> **Depends on:** Gate 01 done
> **Estimate:** 2 implementation days; re-date at entry.

## Goal

After this gate, every CkGameplayDebugger-owned standalone debugger uses one shared top/content/status frame while preserving feature-specific controls, nested toolbars, and panel layout.

## Entry criteria

- [x] Gate 01 exit evidence re-run on current HEAD.
- [x] Launcher catalog census and all top-level window construction sites re-verified.
- [x] ObjectPooling and Map footer patterns re-read as reference adopters.

## Work items

1. Add `SCkDebug_WindowChrome` with named top, main, and optional status slots plus common separators, padding, and empty-slot collapse.
   -> verify: pure construction/visibility test and Widget Gallery example cover all slot combinations.
2. Put `Use ECS Selection` in the common status composition only when the current tool has a registered entity-target route; retain caller-provided status content.
   -> verify: non-entity tools show no dead control and entity tools show the current ECS id/name state.
3. Adopt low-risk windows first: ObjectPooling, Map, EQS, Input, Jolt, A*, Crowd, UI, Dialog, and Aggro.
   -> verify: existing controls and footers remain reachable; no duplicate status rows.
4. Adopt structured windows last: ECS, Scheduler, State Machine, and GOAP.
   -> verify: picker/popovers, stats/tab tiers, timeline toolbar locality, and Mission Control strips remain structurally unchanged inside the common frame.
5. Update the launcher catalog/window authoring docs so new standalone debuggers must use the shared frame.
   -> verify: catalog count remains exact and docs name the exception for the external Insights Analyzer proxy.

## Expected observations and branches

| Run | Expected | If instead | Response |
|---|---|---|---|
| Build and Widget Gallery | all slot combinations compile/render without blank gaps | wrapper changes desired size or clips content | fix shell slot sizing before further adoption |
| `[EDITOR-VERIFY]` all plugin tabs | top and status placement is consistent; feature controls unchanged | flagship layout shifts or loses actions | keep feature bar as slot content; do not force common action semantics |
| `[EDITOR-VERIFY]` narrow docking | bars remain readable/reachable | controls overflow irrecoverably | preserve existing scroll/wrap behavior or add shell overflow policy with evidence |
| PIE stop/restart with windows open | no retained-handle teardown crash | crash on next PIE | audit new widget attributes/providers and clear before registry teardown |

## Exit criteria

- [x] Every plugin-owned catalog window uses the common frame; external proxy exclusion documented.
- [x] Development editor build and both baseline test slices pass after final edit.
- [x] Fresh logs contain no new ensures or diagnostics naming touched files.
- [x] Full `[EDITOR-VERIFY]` matrix is supplied with exact clicks and expected observations.
- [x] Comment audit and adversarial review complete.
- [x] `PLAN.md`, this status header, `PROGRESS.md`, and permanent docs update together.
