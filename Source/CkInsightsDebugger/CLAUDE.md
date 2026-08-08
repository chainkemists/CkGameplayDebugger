# CkInsightsDebugger

`CkInsightsDebugger` is the UncookedOnly CkGameplayDebugger-owned Slate front end for Unreal Insights trace analysis. CkFoundation's `CkInsightsAnalyzer` module owns trace parsing, structured reports, JSON, logging, and the `-run=CkInsightsAnalyzer` commandlet.

## Ownership boundary

- This module owns the Nomad tab spawner, `CkInsightsAnalyzerTab` tab ID, launcher descriptor, file dialogs, chart, toolbar, and all Slate presentation.
- It depends one-way on CkFoundation's `CkInsightsAnalyzer` public analysis/report APIs. Foundation must never depend on this module or regain Slate/tab ownership.
- The tab ID is a compatibility contract for saved layouts and launcher discovery. Do not rename it.
- The commandlet remains in CkFoundation so headless analysis does not require CkGameplayDebugger.

## UI contracts

- `SCkInsightsAnalyzerTab` is the plugin-owned window root and wraps its feature content in `SCkDebug_WindowChrome`.
- Window-wide boolean controls use `SCkDebug_IconToggle` or `SCkDebug_IconToolbar`. Trace capture, named events, common stat profiles, and `Show all` stay directly visible in that shared action row.
- `FCkInsightsCaptureController` is module-owned rather than tab-owned so closing the tab never silently stops a user-started capture. Ownership is established only by an `OnTraceStarted` callback received while this controller is starting a trace, and stopping becomes available after Unreal's `OnConnection` readiness event. Any later start/stop event clears ownership; an externally started trace is displayed as active but is not controllable from this tool.
- Capture writes an exact file under `Saved/Profiling` with an explicit CPU/GPU/frame/stats/object/RHI/render channel profile. Once Unreal's writer has fully closed it, the tab opens that exact capture automatically. Named events use Unreal's `stats.NamedEvents` command. Grouped stat toggles retain process-global collection intent through the stat-group manager and execute the corresponding `stat <group>` commands so the active viewport reflects the same state as console use. The adjacent `Max/group` control writes and verifies `stats.MaxPerGroup` (minimum 1; Unreal defines no upper limit).
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
- Run `Ck.InsightsDebugger.Capture` for trace-channel and common-stat-profile mapping coverage, and `Ck.DebuggerCommon.IconToolbar` for direct-action sizing and validation coverage.
- `[EDITOR-VERIFY]` Open the launcher, click Insights Analyzer twice, confirm one tab opens and then focuses, confirm the common Debuggers menu and status strip, and verify every action icon plus the `Max/group` field remain compact. Toggle named events and each stat profile; change `Max/group` and confirm the viewport row cap changes; start/stop a capture and confirm that exact trace opens automatically; confirm an externally started trace cannot be stopped from this tab; close/reopen the tab during a capture; then close the tab during or after trace loading without a crash.
