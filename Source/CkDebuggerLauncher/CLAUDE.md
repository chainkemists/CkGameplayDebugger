# CkDebuggerLauncher

`CkDebuggerLauncher` is the DeveloperTool front door for the standalone CK debugger suite. It owns one dockable editor Nomad tab or floating packaged-build window, a responsive vertical icon rail, and a launcher-local Slate style. It is available in editor and packaged Development/DebugGame targets, but excluded from Test and Shipping. It owns no gameplay data or ECS handles.

## Architecture

- `CkDebuggerCommon/Launcher/CkDebuggerToolRegistry` owns the plain-data catalog because every feature debugger already depends on Common.
- Each feature debugger registers its descriptor immediately after its tab spawner and unregisters it before removing the spawner.
- Registration returns a generation token. Unregister requires the matching token, so stale Shutdown code during live reload cannot remove a replacement entry.
- The launcher subscribes to catalog changes and rebuilds only when modules register or unregister (and when the rail's search query changes). It opens/focuses tools through `FGlobalTabmanager::TryInvokeTab` and detects open tabs with `FindExistingLiveTab`.
- The rail carries a quick-open search field (`SCkDebug_SearchBar`) at its top: typing filters the rail case-insensitively on display name (empty categories emit no header), Enter activates the top visible match, Escape clears. The filter is the pure `ck::debugger_launcher::Filter_Tools` (`Private/Window/CkDebuggerLauncherFilter.h`), pinned by `Ck.DebuggerLauncher.Filter.QueryNarrowsTheRail`. The field collapses together with the labels below the width hysteresis threshold, and its query is cleared when it collapses — an invisible filter must never read as "tools went missing".
- The rail has an **embedded mode**: binding `OnToolSelected` makes a click report the tool id to the host instead of invoking a global tab, and the active marker follows the host's `SelectedToolId` attribute. Unbound, the standalone rail behaves exactly as before.
- **The CK Debugger Suite** (`SCkDebuggerSuiteWindow`, tab id `CkDebuggerSuite`, `ck.DebuggerSuite`) is the one-window host: embedded rail on the left, the selected tool's content on the right. It never re-parents a live global `SDockTab` — a tool already open globally is focused (with a "lives in its own tab" card); a tool with a registered `TabFactory` is built once and cached by tab id so switching back preserves its state; a tool with no factory falls back to its global tab. Per-tool "Pop Out" releases the embedded instance (running its close callback) before invoking the global tab. The suite is deliberately NOT a catalog descriptor — the catalog spec pins the exact tool census, and the suite hosts tools rather than being one. Embedded tabs are released on suite close (close callbacks run) and on `OnEnginePreExit` (silently — owning modules may be mid-teardown).
- **Dock anchoring is same-category only** (`ck::debugger_tabs::Invoke_DebuggerTab`): with `DockNewDebuggersIntoExistingWindow` on, a newly opened tool docks next to a live tab of its own `ECkDebuggerToolCategory` and otherwise opens standalone. There is deliberately no cross-category fallback — that is what used to pile all 22 tools into one horizontally scrolling tab well.
- The launcher style scans `Resources/Icons/*.svg` and `Resources/Icons/General/*.svg`. SVGs must remain monochrome white and are tinted through Slate foreground colour.
- Editor targets attach the launcher to Tools > Debug. Packaged Development/DebugGame targets omit that editor workspace-menu dependency; `FGlobalTabmanager` opens the Nomad tab in a floating top-level Slate window when QA runs `ck.DebuggerLauncher 1`.

## Tool groups

| Group | Tools |
|---|---|
| Core | ECS, State Machine, Map, Dialog |
| AI | AI Overview, A*, GOAP, Crowd, EQS, Aggro |
| Systems | Scheduler, Object Pooling, Jolt Physics, Audio |
| Interface | UI Layer, Enhanced Input, Intent |
| Tools | Insights Analyzer, Style Lab, Save, Optimization, Texture & Surface |

(The exact 22-tool census, categories and sort orders are pinned by
`CkDebuggerLauncherCatalog.spec.cpp` — trust the spec over this table if they ever disagree.)

`CkInsightsDebugger` owns the Insights Analyzer tab spawner and registers its own descriptor.
The Foundation analysis module supplies trace parsing and report data; it does not own debugger UI.
In packaged Development/DebugGame targets, every catalog tool registers with the catalog. Their
editor workspace-menu wiring is compiled out; the launcher invokes their runtime Slate tab
spawners in floating windows. ECS, State Machine, Scheduler, and GOAP use the same runtime-Slate
graph canvases in editor and packaged targets.

## Adding another standalone debugger

1. Register the debugger's Nomad tab spawner.
2. Register an `FCkDebuggerToolDescriptor` with owner module, tab ID, localized label/tooltip, existing SVG basename, category, and deterministic order; retain the returned registration ID on the module instance.
3. In Shutdown, unregister the descriptor by tab ID + registration ID before unregistering the tab spawner.
4. Add the tab ID to `CkDebuggerLauncherCatalog.spec.cpp`; the exact census is deliberate discoverability enforcement.
5. Run the `Ck.DebuggerLauncher` automation filter and the manual editor checklist below.

## Slate contracts

- Compact/expanded label switching uses width hysteresis (expand at 180 px, collapse at 150 px) and invalidates layout without rebuilding the rail.
- Icon geometry is fixed and explicitly aligned; never allow fill alignment to stretch SVGs.
- Registry changes are structural and rare; do not rebuild the tool list from Tick.
- Buttons store tab IDs only. Do not capture feature module pointers or launch callbacks.
- Close and release the launcher widget tree before unregistering its style during module reload.

## Verification

Run the `Ck.DebuggerLauncher` automation filter after rebuilding the host editor. Then verify the
editor behavior:

1. Open **Tools > Debug > CK Debugger Launcher** and dock the tab as a narrow vertical rail.
2. Confirm every expected tool appears in the documented groups; icons remain centered, sharp, and unstretched.
3. Hover every button and confirm its debugger name and description are readable.
4. Widen the dock past 180 px and confirm labels appear; narrow it below 150 px and confirm labels collapse without flicker.
5. Reduce the dock height and confirm the full catalog remains reachable by scrolling.
6. Open a debugger and confirm its active indicator appears.
7. Click it again and confirm the existing tab receives focus without creating a duplicate.
8. Close it and confirm the active indicator clears.
9. Restart the editor and confirm the launcher restores at the chosen dock location.
10. Stage a Development package, launch it with `-ExecCmds="ck.DebuggerLauncher 1"`, confirm the floating launcher opens and lists every registered developer tool, then open/focus the target tool twice.
