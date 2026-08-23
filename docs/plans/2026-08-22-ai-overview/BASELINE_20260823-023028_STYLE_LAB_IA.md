# Style Lab grouped-controls baseline — 2026-08-23 02:30:28 PDT

## Gate state

- Reused the immediately preceding full `Debugger` run because no source changed after it and before this baseline.
- Log: `E:\Repos\BusterBlock_Other\Saved\Logs\AiOverview-StyleLab-Final-Debugger.log`
- Result: 256 total, 255 passed, 1 failed, 0 skipped, 0 contaminated, 4m09s.
- Failing set: `{Bb.Snapshot.SmallLoopGoapDebuggerLoadAcceptance}`.
- Exact inherited failure: `BB_RUN_SMALLLOOP_OLD_SAVE_ACCEPTANCE=1 is required; this explicit local-save gate must never false-green.`

For the grouped Style Lab pass, “no regressions” means rerunning this same `Debugger` gate on the final built artifact and observing the same named failing set with no new failures.

## Repository identity and dirty ownership

- BusterBlock root HEAD: `9b4094e20c394999ee27fcca7c065c004ebbac11`.
- CkGameplayDebugger HEAD: `e8d7bf38412668b5eab3343c87e7f5b50a131c0d`.
- The full pre-existing campaign dirty inventory remains the one enumerated in `BASELINE_20260823-015931_STYLE_LAB.md`; every listed path is still preserved.
- Additional already-built pane-style campaign modifications since that snapshot are limited to Common style axes/card/glow files, Style Lab axis metadata/sample, GOAP pane framing/docs, and the campaign progress/baseline documents.
- `Config/DefaultGameplayTags.ini` remains unrelated/not mine. No stash, reset, clean, worktree, clone, commit, or push is authorized.

## Fixture ground

- Live UX screenshot: `C:\Users\sulfu\AppData\Local\Temp\codex-clipboard-90878a1a-f46d-4887-9c67-af23be7c6e6c.png`, 188,358 bytes, modified 2026-08-23 02:26:33 PDT.
- Screenshot observation: detached full-document preview on the left; Profile, a very tall feature-local Input HUD tuner, then a monolithic 24-axis list on the right. Workbench is hidden in the Profile dropdown and Outlined is below the visible fold under Surface Elevation.
