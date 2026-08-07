# Gate 03 - Insights Analyzer ownership

> **Status:** Review remediation implemented; post-rebase automation and editor acceptance pending
> **Depends on:** Gate 02 implementation verified; editor acceptance remains pending
> **Estimate:** 1 day, entered 2026-08-05

## Goal

After this gate, the Insights Analyzer opens as a normal CkGameplayDebugger-owned tab with shared window chrome and icon-toggle presentation, while CkFoundation exposes only the trace-analysis, report, logging, and commandlet implementation consumed by that UI.

## Entry criteria

- [x] Current tips recorded: root `590cc4e`, CkGameplayDebugger `0c71382`, CkFoundation `5f8fcb4`.
- [x] Baseline captured: `Ck.DebuggerLauncher` passed 2/2 with zero failures in `Saved/Logs/InsightsAnalyzerMigration-Baseline-20260805.log`.
- [x] Neighboring patterns re-read: `CkMapDebugger` module registration/lifecycle, `SCkDebug_WindowChrome`, `SCkDebug_IconToggle`, and the launcher catalog test.
- [x] Dirty ownership checked: Foundation contains unrelated untracked docs only; none overlap the analyzer module or descriptor files.

## Work items

1. Keep `CkInsightsAnalyzer` in CkFoundation as the UI-free analysis/report/commandlet module and remove its tab spawner plus Slate/editor dependencies.
2. Add the UncookedOnly `CkInsightsDebugger` module, move the analyzer tab/chart Slate sources into it, wrap the tab in `SCkDebug_WindowChrome`, and render `Show all` through `SCkDebug_IconToggle`.
3. Preserve tab ID `CkInsightsAnalyzerTab`; self-register its launcher descriptor from `CkInsightsDebugger` and remove the launcher's proxy ownership.
4. Update the exact catalog test to assert the analyzer descriptor owner and refresh permanent module/launcher doctrine.
5. Cancel the async ticker and pending trace session explicitly when the tab is destroyed; keep `CkInsightsDebugger` free of unused ECS coupling.

## Expected observations and branches

| Run | Expected observation | If instead | Response |
|---|---|---|---|
| Development Editor build | Foundation core and the new debugger UI module compile with no reverse dependency | UHT/linker/include failure | Correct the module/API boundary; do not restore UI dependencies to Foundation as a shortcut. |
| `Ck.DebuggerLauncher` | 2/2 pass; `CkInsightsAnalyzerTab` has one spawner and exact owner/name/tooltip/icon/category/order contract from `CkInsightsDebugger` | Missing, duplicate, or drifted descriptor/spawner | Audit startup/shutdown registration order and remove the competing owner. |
| Dependency/source census | No Slate/tab source or editor UI dependency remains in Foundation's analyzer module | UI symbol remains | Move that symbol into `CkInsightsDebugger` or expose a UI-free result type from Foundation. |
| `[EDITOR-VERIFY]` open/focus/cancel | Launcher opens/focuses one Analyzer tab; common Debuggers menu appears; Show all is an icon toggle; cancelling Open Trace is safe | Duplicate tab, missing chrome/toggle, or async teardown issue | Keep the gate open and instrument the tab/module lifecycle before changing behavior. |

## Exit criteria

- [x] Expected automated observations pass on the final binary and are recorded in `PROGRESS.md`.
- [x] `ck-change-control` Class 3 checklist is satisfied for the moved ownership boundary.
- [x] Editor-only checks are listed with exact steps and remain explicitly pending until observed.
- [x] `PLAN.md`, this status header, permanent module docs, and `PROGRESS.md` are updated together.
- [x] Final diff passes style/comment audit and an independent adversarial review.
