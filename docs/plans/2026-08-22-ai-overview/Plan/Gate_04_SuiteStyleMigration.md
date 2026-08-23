# Gate 4 — suite-wide Style Lab pane migration

## Goal

Every modern standalone Ck debugger visibly follows the shared `Pane Treatment` and `Corner Style` policy without changing feature data, interaction, splitter behavior, or bespoke visualization semantics.

## Entry criteria

- [x] Common `SCkDebug_Card` already resolves live `SurfaceElevation` geometry and `CornerStyle` brushes.
- [x] AI Overview and GOAP establish the accepted Cards versus Workbench comparison.
- [x] Pre-edit gate baseline captured in `BASELINE_20260823-031422_SUITE_STYLE_MIGRATION.md`.
- [x] Read-only inventory classifies passive pane shells separately from rows, controls, graphs, canvases, and 3D viewports.

## Decisions

### [G4-D1] Migrate semantic pane shells, never sweep raw borders

`SurfaceElevation` remains scoped to Common semantic surfaces as ruled by `CkDebuggerCommon/_Design/2026-08-09-style-liveness-and-completeness.md`. Convert or wrap passive pane boundaries with `SCkDebug_Card`, `SCkDebug_InspectorPanel`, or another existing Common surface. Do not mechanically replace row banding, keycaps, search inputs, graph-node bodies, canvas paint, or viewport internals.

### [G4-D2] Preserve information architecture exactly

Wrapper-only migration must retain every `SSplitter` orientation, `Value`, `MinSize`, handle size, fixed rail width, scroll/list/tree identity, and feature-owned body padding. A migration may add the Style Lab card halo in Cards mode; Workbench remains edge-to-edge through the Common card geometry resolver.

### [G4-D3] Bespoke visualizations receive a Common host, not a rewrite

Runtime graphs, AStar/Map canvases, Intent's octant dial, Crowd/Jolt 3D preview adapters, Audio radar/curves, and similar interactive renderers retain their custom paint and domain colours. Their containing pane may use Common chrome; their internals do not become cards.

### [G4-D4] Frozen and non-window modules stay out of scope

The legacy Gen-1 `CkGameplayDebugger` module remains maintenance-only. `CkSaveDebuggerEditor` is an editor companion rather than a standalone debugger window. Style Lab is the control/reference surface. AI Overview and GOAP are already migrated and serve as exemplars.

### [G4-D5] Splitter-adjacent passive hosts use zero-body-padding Common cards

When a splitter slot already owns its internal gutters, wrap that slot's existing child with `SCkDebug_Card.BodyPadding(FMargin{0.0f})`. This preserves the feature padding while selecting the same live geometry used by GOAP and AI. The Common one-pixel ring remains intentional pane separation; do not delete existing feature separators until live comparison proves they are redundant.

### [G4-D6] Zero-padding wrapper cards are not a valid rounded-pane composition

The live comparison disproved the wrapper-only assumption in D5. AI Overview authors transparent content inside a non-zero Common card body, so the shared rounded brush owns the visible edge. Migrated legacy panes instead place opaque square root borders directly inside zero-padding cards; Slate does not mask child paint to a rounded brush, so those child fills visibly square off or interrupt the card corners and seams. The repair must centralize a corner-safe pane-host contract and remove redundant legacy outer chrome, not tune colors or splitter geometry.

### [G4-D7] Remove Outlined as a selectable pane treatment

The CTO ruled Outlined useless. Remove it from Style Lab, profiles, option metadata, tests, and current documentation. Preserve load compatibility for existing per-user `SurfaceElevation=Outlined` values by retaining only a hidden legacy wire value that resolves/migrates to Flat; unrelated outline brushes and outline-capable widgets remain valid and must not be removed.

### [G4-D8] Dedicated pane host owns Cards and Workbench perimeter grammar

The owner matrix in `PANE_OWNER_MATRIX.md` rules out both another zero-padding card mode and a convention-only cleanup.
`SCkDebug_PaneHost` owns one rounded ring/surface and the style extent in Cards. In Workbench it owns a square,
ringless, zero-extent surface, leaving each splitter handle or one retained fixed-layout separator as the sole shared
boundary. Passive children are transparent at the outer edge. Opaque graphs, canvases, timelines, and 3D viewports use
an explicit Cards-only content frame that collapses in Workbench. `SCkDebug_Card` remains the generic card primitive and
AI Overview remains unchanged as the golden reference.

## Work items

1. Graph/decision family — SM, Scheduler, AStar, EQS, Intent, Dialog, Aggro.
   - Step: migrate passive major pane shells through Common surfaces.
   - Verify: source audit confirms no splitter/value/min-size changes; focused construction/model/graph tests pass.
2. Spatial/runtime family — Crowd, Jolt, Map, Object Pooling, Input, UI, Audio audit.
   - Step: migrate passive host panes; keep viewport/canvas/row semantics bespoke.
   - Verify: existing Crowd/Jolt adapter/window tests plus new safe construction coverage pass; source audit confirms viewport/canvas bodies unchanged.
3. Platform/tool family — ECS, Insights, Save, Optimization, Launcher.
   - Step: migrate major passive split panes and catalog/tool shells, using existing responsive cards where already present.
   - Verify: existing module suites and focused construction/style-policy tests pass; destructive Save/Optimization behavior is untouched.
4. Suite integration.
   - Step: build the final Editor artifact and run focused family gates plus the full `Debugger` gate.
   - Verify: failing set matches the named baseline exactly; `git diff --check` is clean.

## Exit criteria

- [x] All 18 remaining modern debugger windows either consume a Common style-responsive pane surface or have an evidence-backed already-migrated/bespoke classification.
- [x] No wrapper migration changes feature state, selection, refresh, teardown, graph topology, viewport data, or splitter geometry.
- [x] Focused style/construction and existing feature gates pass on the final binary.
- [x] Full `Debugger` gate has no failing-set delta from the 256-test baseline.
- [ ] `[EDITOR-VERIFY]` Open every debugger, switch Classic / Workbench and Cards / Workbench, verify immediate visible response, splitter dragging, and default/narrow-width usability.
- [x] No local cook, package, stage, archive, clean rebuild, engine switch, commit, or push occurred.

## Final coverage classification

| Classification | Debuggers | Evidence |
|---|---|---|
| Migrated in Gate 4 | SM, Scheduler, AStar, EQS, Intent, Dialog, Aggro, Crowd, Jolt, Map, Object Pooling, Input, UI, ECS, Insights, Optimization, Save | Major pane shells use `SCkDebug_PaneHost`; redundant outer chrome is removed, opaque renderers use the explicit interior frame, and splitter ratios/minimums/handles plus fixed rails are unchanged. |
| Already responsive | Audio | Existing metric and director surfaces already consume Common cards; its remaining pages are single bespoke audio visualizations rather than peer pane shells. |
| Existing exemplars | AI Overview, GOAP | Established the accepted Cards versus Workbench comparison before Gate 4. |
| Style authority | Style Lab | Owns profiles, axes, controls, and previews rather than consuming the suite pane layout. |
| Explicit non-window or frozen exclusions | Debugger Launcher, Save Debugger Editor, legacy Gen-1 Gameplay Debugger | The launcher is an adaptive tool rail, Save Debugger Editor has no standalone debugger window, and Gen-1 is maintenance-only. |

## Automated verification

- Fresh pane-remediation baseline: Development Editor succeeded against `D:\Repos\UnrealEngineAngelscript_Other`; `Debugger` was 256 total / 255 passed / one inherited failure / 0 skipped/contaminated (`Saved/Logs/DebuggerPaneParity-Baseline.log`).
- Final Development Editor build succeeded in 56/56 incremental actions against `_Other`.
- Focused Style gate: 25/25 passed, 0 failed/skipped/contaminated (`Saved/Logs/DebuggerPaneParity-Final-Style.log`). Representative Jolt construction passed 1/1 (`Saved/Logs/DebuggerPaneParity-Representative-Jolt.log`).
- Final `Debugger` gate: baseline 256 total / 255 passed / 1 inherited failure -> final 256 / 255 / the same inherited failure, 0 skipped/contaminated (`Saved/Logs/DebuggerPaneParity-Final-Debugger.log`).
- Sole failure: `Bb.Snapshot.SmallLoopGoapDebuggerLoadAcceptance`, still requiring the explicit `BB_RUN_SMALLLOOP_OLD_SAVE_ACCEPTANCE=1` opt-in.
- Full `git diff --check` passed; source audit found no production zero-padding pane wrapper and no changed splitter value/minimum/handle/fixed-width setting. Final logs contain zero ensures, AngelScript errors, fatals, and compiler/linker diagnostics.
