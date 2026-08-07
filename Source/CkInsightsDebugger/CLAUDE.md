# CkInsightsDebugger

`CkInsightsDebugger` is the UncookedOnly CkGameplayDebugger-owned Slate front end for Unreal Insights trace analysis. CkFoundation's `CkInsightsAnalyzer` module owns trace parsing, structured reports, JSON, logging, and the `-run=CkInsightsAnalyzer` commandlet.

## Ownership boundary

- This module owns the Nomad tab spawner, `CkInsightsAnalyzerTab` tab ID, launcher descriptor, file dialogs, chart, toolbar, and all Slate presentation.
- It depends one-way on CkFoundation's `CkInsightsAnalyzer` public analysis/report APIs. Foundation must never depend on this module or regain Slate/tab ownership.
- The tab ID is a compatibility contract for saved layouts and launcher discovery. Do not rename it.
- The commandlet remains in CkFoundation so headless analysis does not require CkGameplayDebugger.

## UI contracts

- `SCkInsightsAnalyzerTab` is the plugin-owned window root and wraps its feature content in `SCkDebug_WindowChrome`.
- Window-wide boolean controls use `SCkDebug_IconToggle` or `SCkDebug_IconToolbar`; `Show all` is the current icon-toggle action. The analysis state remains owned by the tab.
- The custom status content reports trace-open/loading/analysis state through the common chrome's status slot.
- Async trace loading uses a weak shared-pointer ticker delegate. Closing the tab releases the widget and pending session; do not introduce raw callbacks that can outlive it.
- `SCkFrameBarChart` is interactive Slate presentation and remains in this module even though its frame data comes from Foundation.

## Registration lifecycle

1. Register the `CkInsightsAnalyzerTab` spawner.
2. Register the `FCkDebuggerToolDescriptor` with owner `CkInsightsDebugger`.
3. On shutdown, unregister the descriptor before removing the spawner.
4. Preserve the `Insights Analyzer` label, `Hourglass` icon, Tools category, and sort order 10 unless the launcher catalog deliberately changes.

## Verification

- Rebuild the Development Editor target through `CkAuto/UnrealToolbox.exe` after any module, Build.cs, or uplugin change.
- Run `Ck.DebuggerLauncher`; the catalog test asserts the exact tool census, tab spawner, icon, and `CkInsightsDebugger` descriptor owner.
- `[EDITOR-VERIFY]` Open the launcher, click Insights Analyzer twice, confirm one tab opens and then focuses, confirm the common Debuggers menu and status strip, toggle `Show all`, open and cancel the `.utrace` dialog, and close the tab during or after loading without a crash.
