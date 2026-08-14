# Gate 02 - Dense debugger migrations

## Scope

Migrate Crowd, State Machine, GOAP, and Insights after Gate 00 compiles.

## Exit criteria

1. Crowd separates context, operations/source, display menus, camera, and status without repeating `CK Crowd Debugger`.
2. State Machine replaces textual separators and ad hoc distribution with context, graph/layout, and playback/history groups.
3. GOAP removes the adjacent product mark and separates context, playback/history, and detail controls without changing transport behavior.
4. Insights separates trace/file, analysis, and export controls while retaining capture state and commandlet-compatible analysis behavior.
5. Dense groups remain direct on single-line horizontally scrollable lanes rather than wrapping or entering a generic overflow menu.

## Verification

- Static four-window migration census.
- Focused module automation plus broad `Debugger` gate.
- Exact editor rows in `PROGRESS.md` remain pending until manually exercised.

## Result

Complete on 2026-08-14. All four dense windows use direct semantic groups and compiled in the fresh Editor build. Interaction and narrow-width appearance remain explicitly `[EDITOR-VERIFY]`.
