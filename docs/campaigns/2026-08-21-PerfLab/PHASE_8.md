# Phase 8 — Viewport heatmap EdMode (new CkOptimizationDebuggerEditor module)

> **Status:** ✅ Done (2026-08-22)
> **Depends on:** Phase 6 ✅ (independent of Phase 7; either order)
> **Estimate:** 1 session
> **Change class:** 2 (new Editor module; additive)

## Goal

After this phase: with a session selected, every measured position renders over the level-editor
viewport as a marker encoding result by colour (heat ramp), shape (severity band), and size
(over-budget magnitude), with a legend contract; clicking a marker selects the position in the page
model; nothing is spawned, nothing dirtied.

## Entry criteria

- [ ] Read `CkSaveDebuggerEditor` end to end — module files, `CkSaveDebugger_VisualizerEdMode.{h,cpp}`,
      its slot publish/subscribe seam, its Build.cs and uplugin entry (the ONLY Editor-module
      precedent in this plugin; PLAN.md decision #2 sanctions Editor modules exactly for a reflected
      EdMode).
- [ ] Read `ck::debug_axes::Get_HeatColor` + `Get_ToneIconId`; read
      `ck::debug_draw::Is_SuppressedForStreamerMode()`.
- [ ] Branch `perflab/phase-8`; baselines as Phase 7 roots.

## Work items

1. **Module**: `Source/CkOptimizationDebuggerEditor/` — `"Type":"Editor"` uplugin entry (the
   `CkSaveDebuggerEditor` entry is the verified shape); `Build.cs` inherits **`CkModuleRules`**
   (confirmed Phase 0 — every module in this plugin does) with per-dependency justification
   comments; deps from CkSaveDebuggerEditor's shape (`UnrealEd`, `EditorFramework`, …) plus
   `CkCore`, **`CkEcs`** (mandatory — SharedPCH ECS registrations), `CkPerfLab`,
   `CkOptimizationDebugger`, `CkDebuggerCommon`.
2. **Slot seam**: mimic the SaveDebugger slot (`ck::save_debugger_viz` pattern) — an immutable
   published snapshot: positions + per-position (score band, worst metric, magnitude, selected flag).
   Publisher = the Performance page model (host side); the EdMode is stateless and only reads the
   slot. Clicks push position-id selection back through the slot.
3. **EdMode**: `UCk_PerfLabHeatmap_EdMode : UBaseLegacyWidgetEdMode` (hidden, auto-discovered, per
   the precedent) — `Render(...)`: PDI markers at position locations; colour =
   `Get_HeatColor(normalised over-budget ratio)` (normalisation via `FCk_ValueRange`); shape by
   severity band (sphere/diamond/star per the DebugDraw primitive set); size by magnitude
   (clamped range); depth-tested with a screen-size floor so distant markers stay legible.
   `HandleClick` + `HCk_PerfLabHeatmapViz_HitProxy` → slot write.
4. **Legend**: the encoding (colour axis, shape axis, size axis + their ranges) is data on the
   published snapshot, rendered by the page (Phase 7 adds the strip when this lands — coordinate via
   the slot struct so neither phase blocks the other) and embedded in the HTML export (Phase 9).
   House rule from SnapshotLens: a lens without a legend line does not ship.
5. **Suppression**: streamer-mode gate honoured; heatmap off by default, toggled from the page;
   EdMode inert when the selected session's map ≠ the open map (marker positions would lie —
   compare map path, show nothing + reason on the page).
6. **Specs**: marker projection/normalisation/banding logic factored pure and spec-tested
   (`Ck.PerfLab.Heatmap.*` or `Ck.OptimizationDebuggerEditor.*` root — mimic how
   CkSaveDebuggerEditor's testable parts are covered; if it has none, the pure-math specs land in
   CkPerfLab where the shared banding lives). Rendering itself is `[EDITOR-VERIFY]`.

## Expected observations — branches

| I will run | I expect | If instead | Response |
|---|---|---|---|
| `--build --target=Editor` | Compiles; module loads editor-only | Link error into CkPerfLab | Check module API macros (`CKPERFLAB_API`) on the slot types |
| Package check (`ck-change-control` env matrix) | Editor module absent from any packaged build | It packages | uplugin Type wrong — must be `Editor` |
| Gates | All green vs baselines | Δ in `Ck.SaveDebugger` roots | You copied more than the pattern — restore; the precedent is read-only |

## Exit criteria — same commit

- [ ] Gates green vs baselines on final binary; pure specs green.
- [ ] `[EDITOR-VERIFY]` rows in VALIDATION.md: markers render over the open map; colour/shape/size
      track the data; click selects; nothing dirtied (`git status` + editor dirty state clean after
      toggling); wrong-map guard shows reason.
- [ ] `Source/CkOptimizationDebuggerEditor/CLAUDE.md` created (module contract, slot seam, the
      decision-#2 justification line).
- [ ] PLAN.md row + Status header + PROGRESS.md entry.

## Fences

- No actor spawning, no components added to the world, no transactions — PDI drawing only.
- No direct dependency from CkOptimizationDebugger (DeveloperTool) onto this Editor module — the
  slot seam keeps the arrow Editor → DeveloperTool. (The packaging tripwire the capability review's
  dependency-fork table warns about.)
- Colour never encodes alone — shape+size are mandatory redundant axes (a11y, D9).
