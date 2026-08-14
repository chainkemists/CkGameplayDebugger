# Debugger command bar campaign - mission brief

> **Written:** 2026-08-14. Stable scope only; live state belongs in [PROGRESS.md](PROGRESS.md).

## Goal

Give every registered standalone CK debugger one professional, consistent top area: directly visible high-frequency actions, explicit semantic command groups, predictable alignment, a stable debugger switcher, and no debugger identity repeated beneath the dock tab.

## Success criteria

1. All 18 registered standalone debugger windows use `SCkDebug_WindowChrome` and the shared command-bar presentation contract.
2. Common code owns group spacing, visual separators, single-line horizontal overflow, accessibility labels, and the single flexible alignment boundary; feature modules continue to own state and behavior.
3. Every feature command is placed in an explicit semantic group. Primary and context lanes never wrap; constrained lanes scroll horizontally, separators appear only between rendered groups, and direct high-frequency actions are never hidden in a generic overflow menu.
4. The dock tab is the sole debugger identity surface. Chrome does not render a debugger title or identity icon; domain/context labels remain only where they add information.
5. Refresh controls remain present only for windows that currently support refresh, but appear in the same trailing utility position.
6. Existing control behavior, selection, capture, playback, filters, status, and tool switching remain unchanged.
7. A fresh Editor build and the baseline-comparable `Debugger` automation gate have no failing-set regression from the recorded baseline.
8. Wide and narrow-docked visual behavior has an exact `[EDITOR-VERIFY]` checklist and is not claimed complete through automation alone.

## Locked decisions

- The UI term is **command bar**: it contains contextual tool commands rather than an application menu hierarchy.
- `SCkDebug_CommandBar` lives in `CkDebuggerCommon` and renders an ordered array of semantic groups containing already-constructed Slate widgets.
- The abstraction boundary is the group, not a universal tagged union of every possible button, selector, stepper, and popover.
- `SCkDebug_IconToolbar` remains the shared declarative surface for homogeneous boolean icon actions.
- The dock tab owns debugger identity. `SCkDebug_WindowChrome` owns the Tools switcher and never repeats the debugger name or identity icon.
- Separators are styled Slate visuals, never text glyphs such as `|`.
- Existing feature delegates and state lifetimes remain feature-owned; Common does not capture feature window instances.
- No worktree, clone, commit, push, or unrelated cleanup is part of this campaign.

## Non-goals

- Do not redesign debugger bodies or data collection.
- Do not change capture, playback, refresh, or selection semantics.
- Do not modify the legacy packaged State Machine window that does not use `SCkDebug_WindowChrome`.
- Do not add mobile behavior; the supported narrow target is an Unreal desktop dock.

## Evidence boundary

Automation can prove descriptor validation, ordering, the non-wrapping icon-row type, separator planning, compilation, and existing debugger behavior tests. Actual visual density, contrast, horizontal scrolling, and control usability remain `[EDITOR-VERIFY]`.
