# Baseline — debugger pane layout parity

Captured: 2026-08-23 09:30:05 America/Los_Angeles

## Gate state

- Command: detached `CkAuto/UnrealToolbox.exe --build --config=Development --target=Editor --test --discover-fresh --test-pattern Debugger --output=Saved/Logs/DebuggerPaneParity-Baseline.log --project=E:\Repos\BusterBlock_Other`
- Engine: `{F5534390-E8D1-4B55-83EF-4DA731819D09}` at `D:\Repos\UnrealEngineAngelscript_Other`
- Build: succeeded; Editor target was up to date.
- Log: `E:\Repos\BusterBlock_Other\Saved\Logs\DebuggerPaneParity-Baseline.log`
- Total: 256
- Passed: 255
- Failed: 1
- Skipped: 0
- Contaminated: 0
- Duration: 8m 09s
- Inherited failing set:
  - `Bb.Snapshot.SmallLoopGoapDebuggerLoadAcceptance`
  - Exact reason: `BB_RUN_SMALLLOOP_OLD_SAVE_ACCEPTANCE=1 is required; this explicit local-save gate must never false-green.`
- Fresh log scan: zero `Angelscript: Error` entries and zero compiler/linker error-like diagnostics. Existing AngelScript warnings are inherited and do not name CkGameplayDebugger files.

For this pass, no regression means the final Development Editor artifact reruns the same discovered `Debugger` cohort with the identical 256 / 255 / 1 / 0 / 0 totals and identical named failing set. Focused Common/style/construction tests must additionally pass.

## Base commits and generated-project ground

- BusterBlock root: `9b4094e20c394999ee27fcca7c065c004ebbac11`
- `Plugins/CkGameplayDebugger`: `e8d7bf38412668b5eab3343c87e7f5b50a131c0d`
- `Intermediate\ProjectFiles\BusterBlock.vcxproj`: 118 matching `_Other` lines / 346 `_Other` path occurrences / zero primary-only engine references.

## Dirty ownership

All pre-existing root dirt and the large CkGameplayDebugger campaign working set are not mine for cleanup, restore, stash, broad staging, or rewrite. The exhaustive inventory is the `git status --short --untracked-files=all` captured immediately before and after the gate; it matches the continuation prompt. This sub-pass begins with the two campaign documents added after this snapshot.

## Fixture ground

- Approved visual comparison: `C:\Users\sulfu\AppData\Local\Temp\codex-clipboard-41b67527-016a-4cc7-8ebd-b333e96832ea.png`, mtime 2026-08-23 09:09:28.
- Golden source composition: `Source/CkAiDebugger/Public/CkAiDebugger/Window/SCkAiDebuggerWindow.cpp`, mtime 2026-08-23 02:02:43.
- Historical gate log: `Saved/Logs/AiOverview-SuiteStyleMigration-Final-Debugger.log`, mtime 2026-08-23 03:41:27.
- Painted antialiasing, narrow-width behavior, and splitter drag remain `[EDITOR-VERIFY]`; automation can prove host topology and live geometry, not final pixels.
