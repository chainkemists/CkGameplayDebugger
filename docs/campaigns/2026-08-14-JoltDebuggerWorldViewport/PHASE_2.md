# Phase 2 — Preview-world viewport shell (CkGameplayDebugger/CkJoltDebugger)

> **Status:** ✅ Done 2026-08-15 (code uncommitted, pending user commit approval; `[EDITOR-VERIFY]` A–D pending human PIE pass)
> **Depends on:** Phase 1 ✅ (CkFoundation dev @ 5edf6c152, CkTests dev @ fbd99927)
> **Estimate:** 1 session

## Goal

After this phase: opening the Jolt debugger during PIE shows a live 3D viewport (debugger-owned
`FPreviewScene` world) rendering all Jolt bodies via a registered `FCk_Jolt_DebugDrawTarget`, with
Crowd-parity camera controls (Perspective + 6 orthos + Frame All + Frame Selection, orbit/pan/
zoom/WASD, F/Home), a wireframe/solid toggle, per-population visibility toggles, and a color
legend — surviving PIE end without crash.

## Entry criteria — VERIFIED 2026-08-15

- [x] Phase-1 exit re-verified on current HEAD: full suite 1147/1150 (fails ⊆ baseline named
      set), scoped Jolt 62/62 serial exit 0, commits landed (hashes above).
- [x] Phase-2 baseline = those numbers + the baseline snapshot's 4-name failing set (Homing flake
      annotated).

## Work items

1. **Facility extensions (CkJolt, small)**: per-color-class visibility on the target
   (`Set_ClassVisibility`; hidden class ⇒ instances hidden/skipped, cheap toggle — bucket-level
   `SetVisibility`, not re-capture) + JPH-free `Get_ContentBounds()` for camera framing. Specs
   for both. → verify: scoped Jolt suite green.
2. **Viewport widget**: `SCkJoltDebugger_3dViewport` mimicking
   `SCkCrowdDebugger_3dViewport.{h,cpp}` (FPreviewScene no-lighting/no-physics/non-editor +
   FSceneViewport + `FUMGViewportClient` subclass; 9 camera presets; orbit/pan/zoom/WASD;
   F/Home; per-frame invalidate). Drop Crowd's nav/voxel drawing; keep the shell + camera math.
   Target bound to the preview world, registered against the SELECTED game world's
   `UCk_Jolt_Subsystem` (shared `FCkDebuggerModel_WorldSelector` conventions), demand =
   tab visible && world valid.
3. **Window integration**: extend `SCkJoltDebuggerWindow` — viewport as the main pane (existing
   stat sections become a side rail), command groups per `SCkDebug_WindowChrome` lanes: camera
   group (9 icon buttons, `FCkDebuggerCommonStyle` icons as Crowd), render-mode toggle
   (`SCkDebug_IconToggle`), population toggles (JoltBody/BakedStatic/Sensors/Characters),
   color legend. Existing `ck.Jolt.DebugDraw.*` toggles stay.
4. **Lifecycle**: `ck::DebugSessionLifecycle` + `EndPIE` — unregister the target from the dying
   subsystem (facility already self-heals via weak refs + OnWorldCleanup; window drops its
   game-world references synchronously); window survives PIE end showing last state; teardown on
   `OnEnginePreExit` per plugin contract.
5. **Specs**: window-construct spec stays green; add viewport-construct spec (SlatePrepass,
   desired size) per `CkJoltDebuggerWindow.spec.cpp` pattern.
6. **Docs weld + `[EDITOR-VERIFY]` list** (same commit as last work item): PLAN row, this header,
   PROGRESS entry; exact manual PIE steps covering — viewport shows bodies matching in-world
   draw; camera presets/flight; wireframe toggle; population toggles; legend; PIE-end survival;
   the Phase-1 deferred items (legacy draw visual parity, OnWorldCleanup on real PIE end,
   asleep-spawn funnel live).

## Expected observations at the gate — branches

| I will run | I expect | If instead | Response |
|---|---|---|---|
| Scoped `Jolt` serial suite | all green incl. new specs | facility-extension regressions | STOP, restore, diagnose |
| Full serial suite | fails ⊆ baseline named set | new reds | STOP; bisect |
| `[EDITOR-VERIFY]` PIE pass (human) | bodies render in viewport, camera/toggles work | anything else | record verbatim; fix or fence |

## Fences

- No `SEditorViewport`; no `UnrealEd` outside `Target.bBuildEditor` (packaged DeveloperTool).
- Debugger module never includes `CkJolt_DebugDrawTarget_Impl.h` (module anti-pattern).
- Never rebuild Slate structure on Tick (scrunch-free contract, `SCkDebugger_WindowBase`).
- No `FCk_Handle` cached without the EndPIE/session-invalidated clear chain.
- Selection/outliner/picking = Phase 3, not here.

## `[EDITOR-VERIFY]` — human PIE pass (agents cannot launch PIE)

Prereq: PIE a map with baked static geometry, some dynamic JoltBodies (ideally a settled pile),
at least one Probe sensor and one JoltCharacter if available.

**A. Legacy in-world draw (Phase-1 parity)**
1. Console `ck.Jolt.DebugDraw.Enabled 1` → bodies draw in the GAME viewport as before (motion-type
   colors, translucent). `Opacity 0.15` → `0.9` → tint changes live, no hitch. `SleepColoring 1`
   → settled dynamics red, awake yellow. `Enabled 0` → all gone the same frame (nothing frozen).
2. Multi-world: PIE with 2 players → both server + client worlds draw (intended change P1-D17).

**B. Asleep-spawn + sleeping-at-open (Phase-1 deferred)**
3. Open debug draw AFTER a pile has settled → sleeping bodies draw immediately without waking.
4. Spawn a JoltBody with `InitialSleepState = Asleep` → draws immediately.

**C. PIE-end teardown (Phase-1 + Phase-2)**
5. With the Jolt debugger open and drawing, press Stop → no crash, no ensure; window survives,
   shows last state. Re-PIE → viewport repopulates against the new world.

**D. The viewport (Phase 2)**
6. Tools ▸ Debug ▸ CK Jolt Physics (or `ck.JoltDebugger 1`) during PIE → the 3D pane shows the
   same bodies as the in-world draw, same colors; stat rail still updates.
7. Camera: each of the 7 preset buttons (Perspective, Top, Bottom, Left, Right, Front, Back) +
   Frame All (button and `Home`); orbit RMB, pan MMB, wheel zoom, RMB+wheel speed, WASD/QE/arrows
   flight in perspective. **Ortho presets must NOT clip half the bodies** (eye backed off — P2-D20
   #8; Crowd still has the bug, expected).
8. Frame All frames the visible content; hide a distant population then Frame All → must not frame
   empty space (P2-D19).
9. Primary-lane wireframe toggle → wireframe ⇄ solid, instance count/framing unchanged.
10. Population toggles (JoltBody / BakedStatic / Sensors / Characters) → each hides/shows its class
    instantly, no rebuild hitch. Legend swatches match drawn colors.
11. "In-world draw" Context toggles affect the GAME viewport only, not this pane (by design; tooltip
    says so).
12. **Demand-off on tab switch:** with PIE running and the pane drawing, click a sibling tab in the
    same tab well; `stat CkJolt` → `Jolt_DebugDraw_Capture` should drop to ~0 (P2-D20 #1). Return
    to the tab → capture resumes, pane repopulates.

## Exit criteria — ALL in the same commit as the last work item

- [x] Scoped Jolt suite green (serial, orchestrator-run): 67/67 exit 0; full suite 1146/1150,
      failing set == baseline's 4 names (delta-zero)
- [x] Window + viewport specs green (4/4 by name in the serial run); launcher census 3/3
      (executor run; census spec unchanged)
- [x] `[EDITOR-VERIFY]` list delivered (section above, 12 steps A–D)
- [x] PLAN.md row + this header + PROGRESS.md updated; `CkJoltDebugger/CLAUDE.md` created,
      `CkJolt/CLAUDE.md` current from Phase 1
