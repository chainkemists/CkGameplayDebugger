# Phase 3 — dispatch plan (orchestrator working doc)

## Unit VIII — facility (CkJolt) — DISPATCHED (Opus, fresh)
`Set_HighlightedBody` / `Get_HighlightedBody` / `Get_HighlightedBodyBounds` / `TryPick_Body` /
character-key helper; `Highlight` colour class (8th, always visible); specs
`HighlightAddsOverlayInstance`, `HighlightedBodyBounds`, `PickNearestBody`. Gate: scoped Jolt.

## Unit IX — debugger (CkJoltDebugger) — dispatch AFTER VIII lands
Prereq reading: PHASE_3.md P3-D23…D27; CkDebuggerCommon/CLAUDE.md (list-row pointer identity,
click traps, dual search, entity targeting, picker), Crowd's AgentListPanel + window selection
plumbing (the five-path selection sync at SCkCrowdDebuggerWindow.cpp:99-135, 244-249;
AgentListPanel.cpp:222-224, 296-311, 456-476, 541-566), CkJoltDebugger/** (current), the VIII
public API (CkJolt_DebugDrawTarget.h ONLY).
Work items PHASE_3.md 2–7. Build.cs + CkSpatialQuery (Probe fragment). Route registered in the
module after spawner / unregistered before. Picker in `JoltTarget` group with the shared predicate.
Selection sources → sinks per P3-D25. Frame Selection + `F` return in the viewport
(`_SelectionBounds` fed from `Get_HighlightedBodyBounds`). Viewport click → `TryPick_Body` via
the restored `GetCursorWorldRay`. Specs: outliner-construct, detail-construct.
Gates: `--test-pattern JoltDebugger`, `--test-pattern DebuggerLauncher`, `--test-pattern Jolt`.

## Unit X — adversarial review (fresh Opus) → fix-up → orchestrator gate of record → docs weld
Same shape as Phases 1–2.
