# Gate 0 — Common chrome and control primitives

> **Status:** Complete
> **Depends on:** baseline captured

## Goal

After this gate, every existing debugger has a consistent right-side Common action lane, the obsolete dropdown is gone, the launcher is one icon, and every existing picker is represented by one cursor-icon control in the same lane.

## Entry criteria

- [x] Baseline captured on `e8d7bf38412668b5eab3343c87e7f5b50a131c0d`.
- [x] All 20 WindowChrome and 7 picker-control consumers inventoried.
- [x] Existing typed icons verified: `Diagnostics`, `SelectInViewport`, `Settings`.

## Work items

1. Add reusable `SCkDebug_IconButton` to Common; migrate the encountered picker settings gear and use it for the launcher.
2. Add `CommonActionsContent` to WindowChrome utility content; remove `Build_DebuggerMenu` and the text combo.
3. Move the launcher tab id into the Common tab route and make the Launcher module use that same id.
4. Convert the picker toggle to `SCkDebug_IconToggle` with `SelectInViewport`, preserving active/disabled tooltips and settings popover.
5. Move all 7 picker controls from feature lanes to `CommonActionsContent`.
6. Add focused construction/layout/route tests and update Common/Launcher docs.

## Expected observations

| Run | Expected | If instead | Response |
|---|---|---|---|
| Common/Launcher automation | icon button validates atomically; launcher route opens registered spawner; picker state remains live | partial action layout or stale launcher callback | fail closed and use the generation-safe registry pattern |
| Live narrow and normal window review | feature controls remain left; picker, speed placeholder, refresh, launcher remain right and single-line | wrapping or clipped identity chrome | adjust Common command-bar utility layout only |

## Exit criteria

- [ ] Focused Common + Launcher tests pass on final Gate 0 source.
- [ ] All 20 windows compile and all 7 pickers use the common lane.
- [ ] No bare icon button remains in the touched picker/chrome surfaces.
- [ ] `[EDITOR-VERIFY]` normal and narrow debugger-window review recorded.
- [ ] PLAN, this status, PROGRESS, and Common docs updated together.
