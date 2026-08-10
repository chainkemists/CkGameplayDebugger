# CkInsightsDebugger

`CkInsightsDebugger` is the DeveloperTool CkGameplayDebugger-owned Slate front end for Unreal Insights trace analysis. It is available in editor and packaged Development/DebugGame targets, but excluded from Test and Shipping with Unreal's developer-tool policy. CkFoundation's `CkInsightsAnalyzer` module owns trace parsing, structured reports, JSON, logging, and the `-run=CkInsightsAnalyzer` commandlet.

## Ownership boundary

- This module owns the Nomad tab spawner, `CkInsightsAnalyzerTab` tab ID, launcher descriptor, file dialogs, chart, toolbar, and all Slate presentation.
- It depends one-way on CkFoundation's `CkInsightsAnalyzer` public analysis/report APIs. Foundation must never depend on this module or regain Slate/tab ownership.
- The tab ID is a compatibility contract for saved layouts and launcher discovery. Do not rename it.
- The commandlet remains in CkFoundation so headless analysis does not require CkGameplayDebugger.

## UI contracts

- `SCkInsightsAnalyzerTab` is the plugin-owned window root and wraps its feature content in `SCkDebug_WindowChrome`.
- Window-wide boolean controls use `SCkDebug_IconToggle` or `SCkDebug_IconToolbar`. Named events, common stat profiles, and `Show all` stay directly visible in that shared action row. Timed trace capture is a feature-local progress button with its editable seconds and screenshot-count fields immediately adjacent because it represents a stateful operation rather than a boolean preference.
- `FCkInsightsCaptureController` is module-owned rather than tab-owned so closing the tab never silently stops a user-started capture. Ownership is established only by an `OnTraceStarted` callback received while this controller is starting a trace, and the timed deadline starts only after Unreal's `OnConnection` readiness event. Any later external start/stop event clears ownership; an externally started trace is displayed as active but is not controllable from this tool. Completed captures remain retained until a tab successfully begins opening them.
- Capture writes under `Saved/Profiling` with an explicit CPU/GPU/frame/stats/object/RHI/render/screenshot channel profile. Timed capture defaults to 30 seconds and three named screenshots at 10%, 50%, and 90%; its QA-facing screenshot-count field is tunable from 0 to 12. Zero disables automated screenshots, one uses 50%, two use 10% and 90%, and three through twelve are evenly distributed through the inclusive 10-90% capture window. A timed capture can be stopped early from the progress button. Once Unreal's writer has fully closed it, the tab opens that exact capture and exports `<trace-stem>.report.md` plus `<trace-stem>.report.json` beside it. Named events use Unreal's `stats.NamedEvents` command. Grouped stat toggles retain process-global collection intent through the stat-group manager and execute the corresponding `stat <group>` commands so the active viewport reflects the same state as console use. The adjacent `Max/group` control writes and verifies `stats.MaxPerGroup` (minimum 1; Unreal defines no upper limit).
- The custom status content reports trace-open/loading/analysis state through the common chrome's status slot.
- Async trace loading uses a weak shared-pointer ticker delegate. Closing the tab releases the widget and pending session; do not introduce raw callbacks that can outlive it.
- `SCkFrameBarChart` is interactive Slate presentation and remains in this module even though its frame data comes from Foundation. It is the ruled EXCEPTION to the 2026-08-09 common-widget consolidation's "2 frame-strips -> 1" retirement: the QA capture work landed frame-linked screenshot markers + a thumbnail rail inside it, which is genuinely Insights-specific. Porting those onto the shared `SCkDebug_FrameStrip` (and then retiring this chart) is a logged follow-up, not a standing rule violation.
- Trace screenshots are rendered as bright exact-frame markers plus a synchronized thumbnail rail in `SCkFrameBarChart`. Clicking either the marker or thumbnail centers and selects its mapped game frame, runs the existing single-frame analysis, and shows the decoded image beside the existing frame details. Keep decoding and dynamic-brush creation outside Slate paint/input paths. When TraceServices only supplies a preceding-frame mapping, label it as nearest rather than claiming exact containment.
- Editor targets place the Nomad tab under Tools > Debug. Packaged Development/DebugGame targets omit the editor workspace-menu dependency; the runtime Slate tab manager opens the same Nomad content in a floating top-level window. Use `ck.InsightsAnalyzer 1` to open it directly or `ck.DebuggerLauncher 1` to open the packaged launcher.

## Registration lifecycle

1. Register the `CkInsightsAnalyzerTab` spawner.
2. Register the `FCkDebuggerToolDescriptor` with owner `CkInsightsDebugger`.
3. On shutdown, unregister the descriptor before removing the spawner.
4. Preserve the `Insights Analyzer` label, `Hourglass` icon, Tools category, and sort order 10 unless the launcher catalog deliberately changes.

## Verification

- Rebuild the Development Editor target through `CkAuto/UnrealToolbox.exe` after any module, Build.cs, or uplugin change.
- Build and stage a Development game target after changing packaged support. Launch the staged executable with `-ExecCmds="ck.InsightsAnalyzer 1"` and confirm a floating analyzer window opens; repeat through `ck.DebuggerLauncher 1`.
- Run `Ck.DebuggerLauncher`; the catalog test asserts the exact tool census, tab spawner, icon, and `CkInsightsDebugger` descriptor owner.
- Run `Ck.InsightsDebugger.Capture` for trace-channel and common-stat-profile mapping coverage, and `Ck.DebuggerCommon.IconToolbar` for direct-action sizing and validation coverage.
- `[EDITOR-VERIFY]` Open the launcher, click Insights Analyzer twice, confirm one tab opens and then focuses, confirm the common Debuggers menu and status strip, and verify every action icon plus the capture-duration, screenshot-count, and `Max/group` fields remain compact. Confirm screenshot count defaults to 3, accepts 0-12, and is disabled while tracing, auto-opening, or loading. Toggle named events and each stat profile; change `Max/group` and confirm the viewport row cap changes. Start a short timed capture, confirm progress and remaining time update, stop another capture early, and verify the exact trace plus same-stem Markdown/JSON reports. For a capture long enough to cross all thresholds, confirm three named screenshot markers and thumbnails follow the chart while panning/zooming; repeat with a count of 5 and confirm five markers/thumbnails are distributed through the capture window. Click each tiny thumbnail and confirm the chart centers/selects that game frame, the full screenshot appears beside the existing frame analysis, and exact-versus-nearest mapping is stated accurately. Confirm an externally started trace cannot be stopped from this tab; close/reopen the tab during a capture and confirm completion is recovered; then close the tab during or after trace loading without a crash.
