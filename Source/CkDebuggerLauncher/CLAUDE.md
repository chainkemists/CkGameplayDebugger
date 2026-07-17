# CkDebuggerLauncher

`CkDebuggerLauncher` is the UncookedOnly editor front door for the standalone CK debugger suite. It owns one dockable Nomad tab, a responsive vertical icon rail, and a launcher-local Slate style. It owns no gameplay data or ECS handles.

## Architecture

- `CkDebuggerCommon/Launcher/CkDebuggerToolRegistry` owns the plain-data catalog because every feature debugger already depends on Common.
- Each feature debugger registers its descriptor immediately after its tab spawner and unregisters it before removing the spawner.
- Registration returns a generation token. Unregister requires the matching token, so stale Shutdown code during live reload cannot remove a replacement entry.
- The launcher subscribes to catalog changes and rebuilds only when modules register or unregister. It opens/focuses tools through `FGlobalTabmanager::TryInvokeTab` and detects open tabs with `FindExistingLiveTab`.
- The launcher style scans `Resources/Icons/*.svg` and `Resources/Icons/General/*.svg`. SVGs must remain monochrome white and are tinted through Slate foreground colour.

## Tool groups

| Group | Tools |
|---|---|
| Core | ECS, State Machine |
| AI | A*, GOAP, Crowd, EQS |
| Systems | Scheduler, Object Pooling |
| Interface | UI Layer, Enhanced Input |
| Tools | Insights Analyzer |

The Insights Analyzer descriptor is registered by the launcher module itself (by proxy): its tab
spawner lives in CkFoundation's `CkInsightsAnalyzer` module, which cannot depend on
`CkDebuggerCommon` without a plugin cycle.

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
editor-only behavior:

1. Open **Tools > Debug > CK Debugger Launcher** and dock the tab as a narrow vertical rail.
2. Confirm every expected tool appears in the documented groups; icons remain centered, sharp, and unstretched.
3. Hover every button and confirm its debugger name and description are readable.
4. Widen the dock past 180 px and confirm labels appear; narrow it below 150 px and confirm labels collapse without flicker.
5. Reduce the dock height and confirm the full catalog remains reachable by scrolling.
6. Open a debugger and confirm its active indicator appears.
7. Click it again and confirm the existing tab receives focus without creating a duplicate.
8. Close it and confirm the active indicator clears.
9. Restart the editor and confirm the launcher restores at the chosen dock location.
