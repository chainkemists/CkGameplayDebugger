# Baseline — pane corner cleanup and Outlined removal

Captured: 2026-08-23 03:52:26 America/Los_Angeles

## Gate state

- Reused the immediately preceding final `Debugger` gate because no source changed after it; the next input was live visual feedback against that artifact.
- Log: `E:\Repos\BusterBlock_Other\Saved\Logs\AiOverview-SuiteStyleMigration-Final-Debugger.log`
- Log mtime: 2026-08-23 03:41:27
- Total: 256
- Passed: 255
- Failed: 1
- Skipped: 0
- Contaminated: 0
- Inherited failing set:
  - `Bb.Snapshot.SmallLoopGoapDebuggerLoadAcceptance`
  - Exact reason: `BB_RUN_SMALLLOOP_OLD_SAVE_ACCEPTANCE=1 is required; this explicit local-save gate must never false-green.`

For this sub-pass, no regression means the final artifact reruns the same `Debugger` gate with the identical named failing set and no count delta. Focused Common/Style Lab tests must additionally pass.

## Base commits

- BusterBlock root: `9b4094e20c394999ee27fcca7c065c004ebbac11`
- `Plugins/CkGameplayDebugger`: `e8d7bf38412668b5eab3343c87e7f5b50a131c0d`

## Dirty ownership

The checkout remains intentionally dirty. Nothing below is authorized for cleanup, restore, deletion, or broad staging.

### Root — active campaign plus unrelated owning-session work

- `Config/DefaultGameplayTags.ini`
- `Plugins/BusterBlockTests/Script/Generated/BusterBlockTests_AutoTestActors.as`
- `Plugins/CkGameplayDebugger` gitlink/worktree
- `Script/Npc/AI/BB_NpcAI_Processor_StuckMovement.as`
- `Source/BusterBlock/BusterBlock.Build.cs`
- `Source/BusterBlock/BusterBlock.cpp`
- `Content/__ExternalActors__/BusterBlock/Map/AutoTests/AutoTests_BB_MAP/1/RL/`
- `Plugins/BusterBlockTests/Script/Tests/NpcAI/BB_AutoTest_NpcAI_StuckRecoverySuppression.as`
- `Source/BusterBlock/AI/Debug/`
- `Source/BusterBlock/Tests/Debugging/`

### CkGameplayDebugger — active AI Overview/style campaign

- `CkDebugger.uplugin`
- migrated window/page sources in AStar, Aggro, Crowd, Dialog, ECS, EQS, GOAP, Input, Insights, Intent, Jolt, Map, ObjectPooling, Optimization, Save, Scheduler, SM, and UI
- Common style/card/glow/window-chrome/picker sources and their tests
- Style Lab axis metadata, controls, sample, window, and tests
- launcher, overlay, AI Overview, and campaign documentation sources
- pre-existing Crowd viewport/view-model and module documentation edits

The exhaustive path inventory is the live `git status --porcelain` captured with this baseline and the immediately preceding `BASELINE_20260823-031422_SUITE_STYLE_MIGRATION.md`; this pass will touch only explicit Common style/card, Style Lab, affected pane-host, test, and campaign-doc paths.

## Fixture ground

- Visual input: `C:\Users\sulfu\AppData\Local\Temp\codex-clipboard-e222bad5-5ebc-4fbc-bdfc-14e3950acda7.png` supplied after the gate above.
- Behavioral ground: the live `SCkDebug_Card`, GOAP wrapper panes, and AI Overview native card composition in this checkout.
- Painted corner acceptance remains an editor observation; automation can prove composition and option inventory, not antialiasing quality.
