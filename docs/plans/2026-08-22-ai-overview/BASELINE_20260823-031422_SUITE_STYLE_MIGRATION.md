# Baseline — debugger suite Style Lab migration

Captured: 2026-08-23 03:14:22 America/Los_Angeles

## Gate state

- Reused the immediately preceding final `Debugger` gate because no source changed after it; only the campaign `PROGRESS.md` was updated.
- Log: `E:\Repos\BusterBlock_Other\Saved\Logs\AiOverview-StyleLabGrouped-Final-Debugger.log`
- Log mtime: 2026-08-23 03:01:58
- Total: 256
- Passed: 255
- Failed: 1
- Skipped: 0
- Contaminated: 0
- Inherited failing set:
  - `Bb.Snapshot.SmallLoopGoapDebuggerLoadAcceptance`
  - Exact reason: `BB_RUN_SMALLLOOP_OLD_SAVE_ACCEPTANCE=1 is required; this explicit local-save gate must never false-green.`

`No regressions` for this phase means the final artifact reruns the same `Debugger` gate with no failing-set delta from the named failure above. Style-specific coverage is additive and must also pass its focused gates.

## Base commits

- BusterBlock root: `9b4094e20c394999ee27fcca7c065c004ebbac11`
- `Plugins/CkGameplayDebugger`: `e8d7bf38412668b5eab3343c87e7f5b50a131c0d`

## Dirty ownership

The checkout is intentionally dirty from the active AI Overview/debugger campaign and other user work. Preserve all of it; stage or edit only explicit paths for this phase.

- Active debugger campaign includes the modified Common/style/window files, GOAP/SM/AStar/Crowd/Intent/Jolt window integrations, new `CkAiDebugger`, tests, and the untracked campaign docs.
- Existing root/game work includes `Config/DefaultGameplayTags.ini`, NPC stuck-recovery code/tests, BusterBlock module edits, generated AutoTest actors, AI debug sources, and external-actor files.
- No path is authorized for cleanup, reset, restoration, deletion, or broad staging.

## Fixture ground

- Primary policy sources are the live Common `SCkDebug_Card` implementation and `CkDebuggerAxes` resolvers in this checkout.
- Visual truth remains live editor comparison across Classic, Workbench, and Outlined at default and narrow widths; automated construction tests cannot prove painted seams or drag affordances.
