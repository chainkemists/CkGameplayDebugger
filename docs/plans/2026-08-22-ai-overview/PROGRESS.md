# CK AI Overview — living progress

## Current state

**As of 2026-08-23 (implementation based on CkGameplayDebugger `e8d7bf38412668b5eab3343c87e7f5b50a131c0d`):** Gate 4 automated pane-owner remediation is complete and the implementation is partitioned into reviewable AI/runtime-control, shared-style-contract, and suite-migration commits. Seventeen modern debugger windows now use the dedicated Common pane host: Cards has one rounded Common perimeter, Workbench is square/ringless and leaves boundaries to splitters or one fixed-layout separator, and opaque renderers receive a Cards-only inset frame. AI Overview is byte-identical to the pre-edit golden source; Audio remains the already-responsive/bespoke classification. Live painted/editor acceptance remains. Gate 3 remains a build-machine handoff.
**Baseline being diffed against:** fresh `Debugger` gate 256 total, 255 passed, 1 inherited failure `{Bb.Snapshot.SmallLoopGoapDebuggerLoadAcceptance}`, 0 skipped/contaminated. See [BASELINE_20260823-093005_PANE_PARITY.md](BASELINE_20260823-093005_PANE_PARITY.md).
**Next action:** open the modern debugger suite beside Style Lab and complete the ordered Cards / Workbench painted-layout and splitter acceptance under [Gate 4](Plan/Gate_04_SuiteStyleMigration.md).
**Engine preflight:** this checkout's generated `BusterBlock.vcxproj` is correctly rooted in `D:\Repos\UnrealEngineAngelscript_Other`. The `.uproject` GUID currently maps through HKCU to `D:\Repos\UnrealEngineAngelscript`, so `CkAuto\Get-ProjectEnginePath.ps1` is not authoritative for this checkout. Do not change either association implicitly; run builds only through UnrealToolbox and abort if its log does not name `_Other`. Gate 3 still cannot execute locally because all cooking and packaging are build-machine-only by CTO direction.

## Decision log

| Date | Decision | Why | Revisit when |
|---|---|---|---|
| 2026-08-22 | Commonize every touched reusable UI shape; bespoke exceptions require written justification. | CTO directive. | Never within this campaign. |
| 2026-08-22 | Reuse overlay AI providers for concise evidence and Crowd’s value-only viewport for spatial evidence. | Prevents duplicate debug-data ownership and copied rendering infrastructure. | A required datum is absent from both public surfaces. |
| 2026-08-22 | Use replicated `AWorldSettings::TimeDilation` behind an authority-only Common API. | Engine source confirms clamp and replication. | Runtime listen-server proof contradicts it. |
| 2026-08-22 | BusterBlock owns stuck-recovery suppression; Common owns only registry/presentation. | Keeps gameplay policy out of debugger UI modules. | Another game needs an identical policy contract. |
| 2026-08-22 | Instrument picker empty states before changing gather behavior. | The packaged symptom is not root-caused yet. | Gather evidence collapses to one mechanism. |
| 2026-08-22 | Promote the overview's stage strip and health roster into Common and export Crowd's existing value-only viewport/view model. | The approved screen introduced reusable debugger shapes; copying Crowd data/rendering would violate the framework boundary. | Never within this campaign. |
| 2026-08-22 | Run every cook/package/stage/archive gate only on the build machine. | CTO global environment boundary. | Only if the CTO explicitly revokes the global rule. |
| 2026-08-22 | Use Common card/selectable-label presentation and Common unload-safe tab release in the AI Overview. | Final delegated audit found feature-local panel chrome and a raw tab-close callback that could survive module unload. | Never within this campaign. |
| 2026-08-23 | Reopen Gate 2 and replace the aggregate evidence signature instead of wrapping or truncating it. | The live Slate screenshot showed provider tags and values flattened into one unreadable neutral row; the approved mockup requires distinct current facts and event deltas. | Never within this campaign. |
| 2026-08-23 | Follow Intent/Crowd nested `SSplitter` composition: 5px seams, no peer-pane padding, minimum readable sizes. | CTO approved the revised edge-to-edge resizable mockup. | A platform limitation prevents nested Slate splitters. |
| 2026-08-23 | Retain empty AI provider/source sections only for detailed AI model consumers. | Runtime GOAP/SM identity is diagnostic evidence even when the provider has no current row; ordinary focus cards must not regain blank chips. | A producer supplies explicit instance records outside overlay sections. |
| 2026-08-23 | **Superseded later this date:** give Common cards an explicit zero-halo mode for splitter panes. | This correctly identified the unconditional 6px halo as the gap mechanism, but hard-coding one treatment discarded a valid alternate style. | Superseded by the Style Lab policy in the next row; retained as root-cause history. |
| 2026-08-23 | Reuse Style Lab `SurfaceElevation` + `CornerStyle` for pane style instead of adding a competing Workbench/Cards setting. | These existing persisted axes already own depth and shape; the missing piece was live outer geometry and GOAP/AI adoption. | The axes can no longer express a requested pane treatment independently. |
| 2026-08-23 | **[G4-D1..D4]** Migrate only semantic passive pane shells through existing Common surfaces; preserve information architecture and host bespoke renderers without rewriting them. | The prior Style Lab design explicitly forbids a blanket `SBorder` sweep, while the suite inventory shows graphs, canvases, viewports, rows, and controls whose colours or interaction are domain semantics rather than pane chrome. | A debugger cannot expose any semantic Common host without changing its interaction contract; record that exact bespoke exception. |
| 2026-08-23 | **[G4-D5]** Use `SCkDebug_Card` with zero body padding for splitter-adjacent passive host panes. | It is the already-tested Common surface policy used by GOAP/AI; zero body padding preserves feature-owned gutters while Cards/Workbench change only outer geometry and surface treatment. | Live compact-width acceptance proves the Common ring plus a retained feature separator creates an unacceptable doubled seam. |
| 2026-08-23 | **[G4-D6]** A rounded Common card must own the only outer pane chrome; migrated children must not repaint an opaque square root inside it. | Stock Slate 5.7 has no rounded child clipping zone, so zero-padding an opaque legacy root into a rounded brush necessarily produces broken corners. | A future engine adds a native rounded descendant clip with acceptable cost. |
| 2026-08-23 | **[G4-D7]** Retire Outlined from the selectable pane treatments and map its hidden legacy config value to Flat. | CTO direction; the option adds no useful debugger grammar. | Never within this campaign. |
| 2026-08-23 | **[G4-D8]** Give splitter/fixed-rail shells a dedicated `SCkDebug_PaneHost`; Cards owns one rounded ring, Workbench is square/ringless, and opaque renderers opt into a Cards-only interior frame. | The source/paint owner matrix proved generic zero-padding cards cannot enforce one perimeter owner and double their ring with splitter boundaries. | A future Slate version provides a cheap rounded descendant clip and measured paint evidence justifies replacing the ownership contract. |

## Dated entries

### 2026-08-22 — baseline and architecture research

- Ran: Development Editor build plus full `Debugger` automation through UnrealToolbox.
- Result: build succeeded; 239/240 passed; inherited explicit local-save gate failed as designed without its environment opt-in.
- Confirmed: 20 WindowChrome constructions; 7 shared picker-control consumers; the textual Tools dropdown is central; `UtilityContent` is already the right-aligned lane.
- Confirmed: engine `AWorldSettings::SetTimeDilation` clamps the value and `TimeDilation` is replicated.
- Confirmed: overlay AI providers already emit GOAP, SM, Crowd, PathNetwork, Objective, and interaction facts; Crowd owns value-only 3D snapshots.
- Inferred, not yet confirmed: packaged missing entities are likely a target-gather visibility/representation gap rather than a Game-world guard. Gather statistics and packaged reproduction will decide.

### 2026-08-22 — Gates 0-2 implementation

- Replaced the textual Tools dropdown with a typed launcher icon in shared WindowChrome; moved all seven existing entity pickers into the right-aligned Common actions lane and retained the visible `SelectInViewport` cursor label.
- Added one authority-world speed control to WindowChrome, so all 21 debugger windows receive `0.1x`, `0.25x`, `0.5x`, and `1x` with client-only/ambiguous failure reasons.
- Added a generation-safe behavior-override registry plus common dynamic panel/row; BusterBlock contributes transient authority-owned stuck-recovery suppression and resets every detector/retry field while active.
- Added `CkAiDebugger` as a DeveloperTool using overlay AI providers, a Crowd-backed NPC health roster, Common stage strip, Common event/override/drill-down widgets, and Crowd's exported value-only 3D spatial viewport.
- Added picker availability taxonomy and single-pass cached predicate accounting: missing world, unsupported world, no matches, no transform representation, cull/filter, ignored local pawn, and viable candidates.
- Development Editor build succeeded. `Ck.DebuggerCommon` focused gate passed 59/59 with fresh AngelScript bytecode (`Saved/Logs/AiOverview-Integrated-Common-Retry3.log`).
- Focused gates passed: `Ck.AiDebugger` 1/1, `Ck.DebuggerLauncher` 3/3, and the real ticking-world `Bb_AutoTest_NpcAI_StuckRecoverySuppression` 1/1.
- Development Game target build succeeded through UnrealToolbox (`Saved/Logs/AiOverview-DevelopmentGame-Build.log`), compiling and linking `CkAiDebugger` plus the debugger suite outside the Editor target.
- Final `Debugger` regression gate: 245 total, 244 passed, 1 failed. The sole failure is the identical inherited explicit local-save gate `Bb.Snapshot.SmallLoopGoapDebuggerLoadAcceptance` requiring `BB_RUN_SMALLLOOP_OLD_SAVE_ACCEPTANCE=1`; no new failures or contaminated tests (`Saved/Logs/AiOverview-Final-Debugger.log`).

### 2026-08-22 — final non-cook acceptance and commonality audit

- Added a direct C++ multi-PIE test for the exact Common world-speed API. It started one listen server plus one remote client, applied `0.1x`, observed replicated client `AWorldSettings::TimeDilation`, reset to `1x`, and passed 1/1 (`Saved/Logs/AiOverview-ListenServerWorldSpeed.log`).
- Replaced every AI Overview feature-local group-border card with `SCkDebug_Card` and its copy-worthy model header with `SCkDebug_SelectableLabel`.
- Added Common `Release_DebuggerTab` teardown: clear raw close callbacks, optionally request close while Slate is live, detach module content, then release the local tab reference. AI close, pre-exit, and module shutdown all use it.
- Added Common detach-content coverage and a real AI tab open/close construction test.
- A local cook/package attempt was stopped before the cook commandlet ran. No cooked, staged, or archived build was produced; all Gate 3 execution is now explicitly a build-machine handoff.
- Root cause of the subsequent 4,179-action Editor rebuild: the package path selected `D:\Repos\UnrealEngineAngelscript` from the `.uproject`/registry helper, while local UnrealToolbox builds use the existing Visual Studio project association at `D:\Repos\UnrealEngineAngelscript_Other`. Both engines reused this checkout's `Intermediate` tree; switching there and back rewrote shared build definitions and invalidated the full local Editor action graph. Feature source changes did not cause that rebuild scale.
- Final `_Other` Development Editor rebuild succeeded, 4,179/4,179 actions (`Saved/Logs/AiOverview-FinalAuditFix-Build.log`). The post-failure lifecycle-test adjustment rebuilt incrementally in 5/5 actions (`Saved/Logs/AiOverview-LifecycleTestFix-Build.log`).
- The first native-window lifecycle test correctly exposed an invalid test topology: global nomad-tab invocation under `-nullrhi` fatals in engine `FGenericWindow::GetRestoredDimensions`. The test now executes the registered Common tab factory, constructs the same real AI Slate tree without a top-level OS window, and closes through the production module path.
- Final focused gates: `Ck.AiDebugger` 2/2 (`Saved/Logs/AiOverview-LifecycleTestFix-Ai.log`) and refreshed `Ck.DebuggerCommon` 60/60, including tab content detachment (`Saved/Logs/AiOverview-FinalAuditFix-Common-Refreshed.log`).
- Final full `Debugger` regression: 248 total, 247 passed, 1 failed. The sole failure remains the identical inherited opt-in `Bb.Snapshot.SmallLoopGoapDebuggerLoadAcceptance` gate requiring `BB_RUN_SMALLLOOP_OLD_SAVE_ACCEPTANCE=1`; no new failures (`Saved/Logs/AiOverview-FinalAuditFix-Debugger.log`).
- Independent final common-widget/lifetime re-audit passed after the fixes: no remaining unjustified AI-local widget, raw callback, or stored-pointer finding.

### 2026-08-23 — Gate 2 reopened after live UX review

- Confirmed root cause: `BuildEvidenceSignature` serializes every AI provider/field/value tuple and `Append_EvidenceIfChanged` appends the whole snapshot as one neutral `AI` row.
- User-approved acceptance target: separate Current Evidence and Recent Cross-System Events panes; concise source/tone/headline/meta rows; every nested GOAP/SM source remains distinct; exact-instance drill-through; edge-to-edge independently resizable splitter panes.
- Fresh pre-edit baseline: 248 total, 247 passed, one identical inherited opt-in legacy-save failure, zero skipped/contaminated (`Saved/Logs/AiOverview-Evidence-Baseline-Debugger.log`).
- Neighboring patterns: Common `SCkDebug_EventLog` for bounded stable event rows; Crowd event dedup/selection; Intent/Crowd nested no-padding `SSplitter` layouts.

### 2026-08-23 — evidence and splitter remediation implemented

- Added Common `SCkDebug_EvidenceList`: stable keyed reconciliation, readable source/headline/value/detail rows, hierarchy indentation, selection/copy payloads, atomic invalid-key rejection, and capped retention.
- Replaced aggregate evidence snapshots with normalized current facts and added/changed/resolved deltas. Initial selection seeds without flooding the event log; unchanged refreshes emit nothing.
- Preserved provider/source identity, parent source ID, depth, and breadth-first order. AI detailed models disable per-source condensation and retain empty valid sections, so every nested GOAP/SM instance remains visible.
- Rebuilt the real AI Overview as edge-to-edge nested 5px splitters: roster/detail, overview/diagnostics, GOAP/State Machine, current evidence/events, and diagnostics/spatial. Selected Entity, Behavior Controls, and Drill Into share an always-visible resizable row; Spatial Evidence owns the larger lower-workspace share.
- Enriched Common NPC health rows with stable pointer reconciliation plus a third context line for entity ID, role/tag, queue/rank, current/max speed, neighbors, and path-point count.
- Changed the BusterBlock behavior adapter to positive `Stuck respawn enabled` semantics. Checked means recovery enabled; unchecked maps to the existing authority-owned suppression policy.
- Final Development Editor build used `D:\Repos\UnrealEngineAngelscript_Other` and succeeded incrementally in 7/7 actions (`Saved/Logs/AiOverview-EvidenceRemediation-Final2-Build.log`). No cook, package, stage, archive, clean rebuild, or engine association change ran.
- Fresh focused AI gate passed 6/6 (`Saved/Logs/AiOverview-EvidenceRemediation-AiDebugger-Fresh.log`). Final full `Debugger` gate: 256 total, 255 passed, 1 failed, 0 skipped/contaminated (`Saved/Logs/AiOverview-EvidenceRemediation-Final2-Debugger.log`). The sole failure is the identical baseline opt-in `Bb.Snapshot.SmallLoopGoapDebuggerLoadAcceptance` requiring `BB_RUN_SMALLLOOP_OLD_SAVE_ACCEPTANCE=1`.
- Independent integration review found the empty-source loss; the detailed retention option and tests closed it. Final adversarial lifetime/order/empty-state audit found and closed the empty-AI-selection drop. No raw pointer or stale-session finding remains in the new paths.

### 2026-08-23 — live-review pane-gap polish

- Compared the live GOAP and AI Overview constructions. The splitters were already edge-to-edge and resizable; the visible AI gutters came from `SCkDebug_Card` reserving a 6px glow extent on every edge even when its glow was transparent.
- Added reusable `SCkDebug_Card::GlowExtent` configuration with the existing 6px default. Every AI Overview pane selects zero extent, eliminating the doubled outer halo while preserving card body padding, header spacing, and the existing 5px splitter handles.
- Incremental Development Editor build succeeded against `D:\Repos\UnrealEngineAngelscript_Other`; focused `Ck.AiDebugger` automation passed 6/6 with no failed, skipped, or contaminated tests (`Saved/Logs/AiOverview-FlushPanes-BuildTest.log`). No cook, package, stage, or archive step ran.
- Live editor acceptance remains pending because automated Slate construction cannot verify the final painted seam or drag affordance.

### 2026-08-23 — pane style promoted to Style Lab policy

- Live comparison established two coherent visual grammars: GOAP's contiguous square workbench and AI Overview's separated rounded card dashboard. Both are valid preferences; neither is now hard-coded as the only pane treatment.
- Reused the existing per-user Style Lab axes. `SurfaceElevation::Layered` retains 6px card breathing room and depth tinting; Flat switches already-built cards to zero outer extent. `CornerStyle` remains orthogonal. Added a curated `Workbench` profile that selects Flat + Sharp without silently changing row density.
- Changed `SCkDebug_GlowWrap::Extent` into a live attribute and made unset `SCkDebug_Card::GlowExtent` resolve through Style Lab. AI Overview removed every literal zero-extent override. The GOAP Inspector's Agent, Planner tree, center, World State, and Timeline panes now share the same Common card frame without changing splitter values/minimums.
- Style Lab's sample document now renders a real `SCkDebug_Card`, not only raw surface swatches. Existing `CommonWidgetsAreLive` coverage now proves one already-built card changes by exactly 12px total width/height between Layered and Flat. Profile coverage pins Workbench to Flat + Sharp.
- Baseline: 256 total, 255 passed, one inherited opt-in failure, 0 skipped/contaminated (`Saved/Logs/AiOverview-StyleLab-Baseline-Debugger.log`; snapshot `BASELINE_20260823-015931_STYLE_LAB.md`).
- Final Development Editor build succeeded against `D:\Repos\UnrealEngineAngelscript_Other` after 101 dependent actions. Focused Style gate passed 23/23 (`Saved/Logs/AiOverview-StyleLab-BuildTest.log`). Final full `Debugger` gate matched baseline exactly: 256 total, 255 passed, the same sole `Bb.Snapshot.SmallLoopGoapDebuggerLoadAcceptance` opt-in failure, 0 skipped/contaminated (`Saved/Logs/AiOverview-StyleLab-Final-Debugger.log`).
- No cook, package, stage, archive, clean rebuild, engine-association change, commit, or push ran. Live two-window Classic ↔ Workbench acceptance remains pending.

### 2026-08-23 — Style Lab information architecture remediation

- Replaced the detached sample canvas plus narrow control rail with one scrollable grouped document. Curated profile buttons are visible at the top; each style group places its relevant controls immediately above a focused live preview.
- Added seven ordered groups: layout/panes/density; tokens/chips/legend; entity/value reading; hierarchy/edit behavior; icons/glyph treatment; graph telemetry; and feature-local Input HUD. Input HUD is last and collapsed by default so its large tuner no longer hides generic debugger styles.
- Renamed the generic surface axis to `Pane Treatment` and exposed its options as `Cards` and `Workbench`. `Classic` and `Workbench` are also visible curated-profile buttons rather than choices hidden in a combo box.
- Kept reflection as the authoritative 24-axis inventory and added packaged-safe group metadata plus integrity coverage. Added a construction contract proving all 24 generic axes and seven colocated previews are present.
- Incremental Development Editor build succeeded against `D:\Repos\UnrealEngineAngelscript_Other`. A stale Toolbox discovery cache plus concurrently refreshed `BusterBlockEditor_NoTests` target metadata caused the first focused test attempt to idle-timeout before assertions; after a dedicated single-lane `--discover-fresh` run, all five `Ck.StyleLab` tests passed (`Saved/Logs/AiOverview-StyleLabGrouped-Fresh.log`).
- Final full `Debugger` regression matched the captured baseline: 256 total, 255 passed, one identical inherited `Bb.Snapshot.SmallLoopGoapDebuggerLoadAcceptance` opt-in failure, zero skipped/contaminated (`Saved/Logs/AiOverview-StyleLabGrouped-Final-Debugger.log`).
- No cook, package, stage, archive, clean rebuild, engine-association change, commit, or push ran. Live visual acceptance of grouping, narrow-width wrapping, and immediate preview changes remains pending.

### 2026-08-23 — suite-wide pane treatment migration

- Migrated passive major pane hosts in SM, Scheduler, AStar, EQS, Intent, Dialog, Aggro, Crowd, Jolt, Map, Object Pooling, Input, UI, ECS, Insights, Optimization, and Save to zero-body-padding Common cards. Existing splitter ratios, minimum sizes, handle sizes, fixed rails, inner gutters, list/tree identity, and feature behavior remain unchanged.
- Kept graph/canvas/viewport internals bespoke: SM/GOAP/Scheduler runtime graphs, AStar and Map canvases, Intent's dial, Crowd/Jolt 3D views, and Audio radar/curves are only Common-hosted where applicable. Audio already used Common cards for its semantic passive surfaces and has no multi-pane shell to migrate.
- Extended the existing Jolt window construction test to assert its four major responsive pane hosts. The final Development Editor retry compiled in 5/5 actions against `D:\Repos\UnrealEngineAngelscript_Other` after correcting test-helper constness and preserving UI history's typed `SExpandableArea` handle.
- Focused Style automation passed 25/25 with zero failed, skipped, or contaminated tests (`Saved/Logs/AiOverview-SuiteStyleMigration-BuildTest-Retry2.log`).
- Final full `Debugger` regression matched the captured baseline exactly: 256 total, 255 passed, one identical inherited `Bb.Snapshot.SmallLoopGoapDebuggerLoadAcceptance` opt-in failure, zero skipped/contaminated (`Saved/Logs/AiOverview-SuiteStyleMigration-Final-Debugger.log`).
- Scoped `git diff --check` passed. A source-level geometry audit found no changed splitter value, minimum, handle, or fixed-width setting. Independent adversarial review found no lifetime, state, or bespoke-renderer rewrite defect; painted comparison must still inspect ECS's retained inner separator and the single-surface Aggro/Dialog root cards.
- No cook, package, stage, archive, clean rebuild, engine-association change, commit, or push ran.

### 2026-08-23 — pane-owner matrix and dedicated host remediation

- Captured the fresh current-state baseline in `BASELINE_20260823-093005_PANE_PARITY.md`: `_Other` Development Editor build succeeded; `Debugger` was 256 total / 255 passed / the sole inherited opt-in failure / 0 skipped / 0 contaminated (`Saved/Logs/DebuggerPaneParity-Baseline.log`).
- Built `PANE_OWNER_MATRIX.md` across AI Overview, ECS, GOAP, SM, Scheduler, AStar, EQS, Intent, Dialog, Aggro, Crowd, Jolt, Map, Object Pooling, Input, UI, Insights, Optimization, Save, and Audio before changing Common. It separates redundant pane chrome from semantic graph/canvas/viewport/timeline fills and records frozen splitter/fixed-rail geometry.
- Added Common `SCkDebug_PaneHost`. Cards owns one rounded ring/surface and style extent. Workbench uses a square, ringless, zero-extent surface so the existing splitter handle or one retained fixed-layout separator owns the boundary. `OpaqueRenderer` adds a Cards-only `SpaceS` frame and collapses it in Workbench. Generic `SCkDebug_Card` and AI Overview were not changed by this remediation.
- Replaced every modern zero-padding pane wrapper with the dedicated host. Removed source-proven redundant ECS/GOAP/SM/Scheduler/EQS/Crowd/Jolt/Map/Object Pooling/Input/UI/Intent/Insights outer chrome while preserving content padding and semantic fills. Map's 250/280 rails and Object Pooling's 300px inspector remain fixed; their single separators are Workbench-only. Audio remains already-responsive/bespoke.
- The first representative Style run correctly failed one new assertion because the expected geometry omitted the ring that Workbench intentionally removes. The corrected exact contract (`12px` extent delta + two ring widths; opaque renderers additionally add two `SpaceS` insets) passed 25/25. Representative Jolt construction passed 1/1 with exactly four Common pane hosts.
- Final `_Other` Development Editor build succeeded in 56/56 incremental actions. Final focused Style passed 25/25 (`Saved/Logs/DebuggerPaneParity-Final-Style.log`). Final full `Debugger` matched baseline exactly: 256 / 255 / the same `Bb.Snapshot.SmallLoopGoapDebuggerLoadAcceptance` opt-in failure / 0 skipped / 0 contaminated (`Saved/Logs/DebuggerPaneParity-Final-Debugger.log`).
- Both final logs contain zero ensures, AngelScript errors, fatals, and compiler/linker diagnostics. Source census finds no production `BodyPadding(FMargin{0.0f})` pane wrapper; the only remaining occurrence is the generic-card liveness test. AI Overview remains SHA-256 `3BBEED8B5C309C0CC7F9080C6F1C3AD68B0C797AC2212B307205DD537975A5A9`, mtime 2026-08-23 02:02:43.
- Full `git diff --check` passed. No cook, package, stage, archive, clean rebuild, engine-association change, commit, or push ran. Painted Cards/Workbench corners, seams, narrow widths, and splitter dragging remain `[EDITOR-VERIFY]`.

## Common-widget audit

| Surface encountered or added | Ownership | Rationale |
|---|---|---|
| Window utility lane, launcher icon, world speed, picker controls | `CkDebuggerCommon` | Suite-wide controls shown by every WindowChrome consumer. |
| Behavior override registry, panel, and row | `CkDebuggerCommon` | Generic session policy presentation; game modules provide callbacks only. |
| Entity health roster/list row | `CkDebuggerCommon` | Reusable health/attention list independent of Crowd or AI data sources. |
| Current evidence / lightweight hierarchy list | `CkDebuggerCommon` | Keyed stable rows, tone/source semantics, wrapping, indentation, trailing state/value, selection, and copy are debugger-agnostic. |
| Decision-to-motion stage strip/card | `CkDebuggerCommon` | Reusable fixed-stage pipeline presentation. |
| Panel card chrome and copyable data labels | `CkDebuggerCommon` | AI Overview now consumes the existing Common card and selectable-label primitives; it owns no duplicate panel surface. |
| Suite-wide pane host | `CkDebuggerCommon` | Seventeen modern debugger windows use `SCkDebug_PaneHost`: Common owns Cards/Workbench outer grammar; feature rows remain domain-owned and opaque graphs/canvases/viewports retain semantic paint inside an explicit frame. |
| Dock-tab release/teardown | `CkDebuggerCommon` | Reusable unload-safe callback/content detachment for module-owned tabs. |
| AI Overview window | `CkAiDebugger` | Composition-only tab layout; it defines no reusable row/control widget. |
| Crowd 3D viewport facade | `CkCrowdDebugger`, exported and reused | Explicit bespoke exception: it translates Crowd, Queue, Recast, VoxelNav, and PathNetwork value snapshots into the already-common `SCkDebug_3dPreviewViewport`. Moving that domain adapter into Common would invert dependencies; all generic camera/scene/input mechanics already live in Common/CkDebugScene. |

## Open items

| Item | Status | Next step |
|---|---|---|
| Common chrome migration | Complete | Interactive narrow-width visual QA remains in final pass. |
| World speed | Verified (non-cook) | Pure matrix and direct listen-server/client replicated readback pass. |
| Stuck recovery suppression | Verified (non-cook) | Focused ticking-world test passed 1/1. |
| AI Overview and debugger suite pane treatment | Automated host remediation complete; visual acceptance pending | Open the modern suite beside Style Lab; switch Cards / Workbench and verify pane grammar plus splitter dragging at default/narrow sizes. |
| Packaged picker | Instrumented; build-machine handoff | Reproduce in a Development package on the build machine, read taxonomy, then repair only the proven mechanism. |
| Final commonality audit | Complete | Independent re-audit passed; Common tests pass 60/60. |
