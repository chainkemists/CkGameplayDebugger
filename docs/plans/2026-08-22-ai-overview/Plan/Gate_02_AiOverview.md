# Gate 2 — Unified AI Overview debugger

> **Status:** Remediation implemented and automated gates complete; live Slate visual acceptance pending
> **Depends on:** Gate 1

## Goal

After this gate, one registered CK AI Overview window presents the approved low-noise NPC diagnosis surface and drills into specialist tools without duplicating their debug-data ownership.

## Work items

1. Add `CkAiDebugger` module, tab, console toggle, launcher descriptor, lifecycle reset, target route, picker, and packaged DeveloperTool registration.
2. Build a view model that merges AI roots, selection lineage, overlay AI-layout sections, Crowd spatial snapshots, and bounded event history using value copies.
3. Add/reuse Common widgets for entity health roster, stage strip, current-evidence list, status alert, drill-down row, and behavior overrides. Extract every reusable shape; justify any feature-local survivor.
4. Reuse/export `SCkCrowdDebugger_3dViewport` for spatial evidence and selection.
5. Implement targeted GOAP/SM/Crowd drill-down through `FCkDebug_EntityTargetRegistry`.
6. Normalize provider rows into stable current evidence and added/changed/resolved cross-system event deltas; never render the aggregate provider signature.
7. Use edge-to-edge nested splitters for roster/detail, overview/evidence, GOAP/SM, evidence/events, and diagnostics/spatial boundaries.
8. Add model, selection, lifecycle, catalog, construction, normalization, and stable-row tests.

## Expected observations

| Run | Expected | If instead | Response |
|---|---|---|---|
| 150-agent roster fixture | flat roster stays responsive; selection pointer identity remains stable | full deep collection per row or selection flicker | keep roster value-only and collect deep model only for selected root |
| PIE stop/start | all handles, histories, viewport snapshots, pending targets clear before registry death | stale selection or next-PIE crash | extend the feature-owned reset chain before proceeding |
| Drill-down buttons | open/focus exactly one specialist tab targeted to closest lineage entity | opens tab without targeting | fix route predicate/open contract; do not special-case modules |

## Exit criteria

- [x] New module builds in Editor and Development Game.
- [x] Launcher catalog contains AI Overview and packaging census includes it.
- [x] Focused tests pass; no stale-handle lifecycle gap remains.
- [x] Current Evidence is a keyed Common list with one readable row per provider/source/field fact; live visual sizing remains the user acceptance step.
- [x] Recent Cross-System Events emits one concise source-toned row per added/changed/resolved fact and no row for unchanged refreshes.
- [x] Multiple GOAP and State Machine source instances, including empty valid sources, remain distinct throughout collection and normalization; the visible Common drill buttons target the selected NPC through each specialist's existing resolver.
- [x] Approved edge-to-edge splitter layout is implemented by the real Slate composition and its real factory/close lifecycle constructs in automation.
- [x] Commonality audit lists no unjustified one-off UI.
- [x] PLAN, status, PROGRESS, and module docs updated together.
- [ ] Live editor review accepts default and resized pane proportions, readability, and immediately visible behavior/drill controls.
