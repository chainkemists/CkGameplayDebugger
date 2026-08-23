# AI Overview Style Lab adoption baseline — 2026-08-23 01:59:31 PDT

## Gate state

- Command surface: UnrealToolbox test-only `Debugger` gate against the already-built Development Editor artifact.
- Log: `E:\Repos\BusterBlock_Other\Saved\Logs\AiOverview-StyleLab-Baseline-Debugger.log`
- Result: 256 total, 255 passed, 1 failed, 0 skipped, 0 contaminated, 4m17s.
- Failing set: `{Bb.Snapshot.SmallLoopGoapDebuggerLoadAcceptance}`.
- Exact inherited failure: `BB_RUN_SMALLLOOP_OLD_SAVE_ACCEPTANCE=1 is required; this explicit local-save gate must never false-green.`

For this style pass, “no regressions” means rerunning this same `Debugger` gate on the final built artifact and observing the same named failing set with no new failures.

## Repository identity

- BusterBlock root HEAD: `9b4094e20c394999ee27fcca7c065c004ebbac11`
- CkGameplayDebugger HEAD: `e8d7bf38412668b5eab3343c87e7f5b50a131c0d`

## Dirty tree at entry

### BusterBlock root

- Not mine / unrelated owner: `Config/DefaultGameplayTags.ini`.
- Existing AI Overview campaign: `Plugins/BusterBlockTests/Script/Generated/BusterBlockTests_AutoTestActors.as`.
- Existing AI Overview campaign: `Plugins/CkGameplayDebugger` gitlink dirt.
- Existing AI Overview campaign: `Script/Npc/AI/BB_NpcAI_Processor_StuckMovement.as`.
- Existing AI Overview campaign: `Source/BusterBlock/BusterBlock.Build.cs`.
- Existing AI Overview campaign: `Source/BusterBlock/BusterBlock.cpp`.
- Existing AI Overview campaign: `Content/__ExternalActors__/BusterBlock/Map/AutoTests/AutoTests_BB_MAP/1/RL/`.
- Existing AI Overview campaign: `Plugins/BusterBlockTests/Script/Tests/NpcAI/BB_AutoTest_NpcAI_StuckRecoverySuppression.as`.
- Existing AI Overview campaign: `Source/BusterBlock/AI/Debug/`.
- Existing AI Overview campaign: `Source/BusterBlock/Tests/Debugging/`.

### CkGameplayDebugger campaign dirt

- Modified: `CkDebugger.uplugin`.
- Modified debugger windows: `Source/CkAStarDebugger/Public/CkAStarDebugger/Window/SCkAStarDebuggerWindow.cpp`, `Source/CkCrowdDebugger/Public/CkCrowdDebugger/Window/SCkCrowdDebuggerWindow.cpp`, `Source/CkEcsDebugger/Public/CkEcsDebugger/Window/CkDebuggerWindow_Main.cpp`, `Source/CkGoapDebugger/Public/CkGoapDebugger/Window/SCkGoapDebuggerWindow.cpp`, `Source/CkIntentDebugger/Public/CkIntentDebugger/Window/SCkIntentDebuggerWindow.cpp`, `Source/CkJoltDebugger/Public/CkJoltDebugger/Window/SCkJoltDebuggerWindow.cpp`, and `Source/CkSmDebugger/Public/CkSmDebugger/Window/SCkSmDebuggerWindow.cpp`.
- Modified Crowd facade/docs: `Source/CkCrowdDebugger/CLAUDE.md`, `Source/CkCrowdDebugger/Public/CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h`, and `Source/CkCrowdDebugger/Public/CkCrowdDebugger/Viewport/SCkCrowdDebugger_3dViewport.h`.
- Modified Common docs/chrome/picker/card/tab files: `Source/CkDebuggerCommon/CLAUDE.md`, `Source/CkDebuggerCommon/Public/CkDebuggerCommon/Launcher/CkDebuggerTabUtils.{h,cpp}`, `Source/CkDebuggerCommon/Public/CkDebuggerCommon/Picker/CkDebug_ViewportPicker.{h,cpp}`, `Source/CkDebuggerCommon/Public/CkDebuggerCommon/Picker/SCkDebug_ViewportPickerControls.cpp`, `Source/CkDebuggerCommon/Public/CkDebuggerCommon/Widgets/SCkDebug_Card.{h,cpp}`, and `Source/CkDebuggerCommon/Public/CkDebuggerCommon/Window/SCkDebug_WindowChrome.{h,cpp}`.
- Modified launcher files: `Source/CkDebuggerLauncher/CkDebuggerLauncher_Module.{h,cpp}` and `Source/CkDebuggerLauncher/Private/Tests/CkDebuggerLauncherCatalog.spec.cpp`.
- Modified overlay topology files: `Source/CkEntityDebugOverlay/Public/CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h` and `Source/CkEntityDebugOverlay/Public/CkEntityDebugOverlay/Presentation/CkDebugOverlay_Present.{h,cpp}`.
- Untracked campaign modules/tests/widgets: `Source/CkAiDebugger/`, `Source/CkDebuggerCommon/Behavior/`, `Source/CkDebuggerCommon/Private/Tests/CkDebug_BehaviorOverrideRegistry.spec.cpp`, `CkDebug_EvidenceList.spec.cpp`, `CkDebug_ViewportPickerAvailability.spec.cpp`, `CkDebug_WorldSpeed.spec.cpp`, `CkDebuggerTabUtils.spec.cpp`, `Source/CkDebuggerCommon/Public/CkDebuggerCommon/Utils/CkDebug_WorldSpeed.{h,cpp}`, `SCkDebug_EntityHealthList.{h,cpp}`, `SCkDebug_EvidenceList.{h,cpp}`, `SCkDebug_IconButton.{h,cpp}`, `SCkDebug_StageStrip.{h,cpp}`, `SCkDebug_WorldSpeedControl.{h,cpp}`, `Source/CkEntityDebugOverlay/Private/Tests/CkDebugOverlay_SourceTopology.spec.cpp`, and `docs/plans/2026-08-22-ai-overview/`.

All listed campaign dirt is preserved in place. No stash, reset, clean, worktree, clone, commit, or push is authorized by this baseline.

## Fixture ground

- Live comparison screenshot: `C:\Users\sulfu\AppData\Local\Temp\codex-clipboard-be4de209-5b07-4bf6-a0ce-c148792859af.png`, 156,007 bytes, modified 2026-08-23 01:49:35 PDT.
- Immediately preceding built gate: `E:\Repos\BusterBlock_Other\Saved\Logs\AiOverview-FlushPanes-BuildTest.log`, 215,662 bytes, modified 2026-08-23 01:40:16 PDT; build succeeded and focused AI gate passed 6/6.
