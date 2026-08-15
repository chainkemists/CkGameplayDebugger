# Phase 7 — Unreal-scheme camera, Draw lane, sim controls, detail, multi-select, drag UI

> **Status:** ⏳ Pending
> **Depends on:** Phase 6 ✅ (the whole facility surface Phases 5–6 add is a hard prerequisite —
> nothing in this phase compiles without it)
> **Scope (repo):** `Plugins/CkGameplayDebugger/Source/CkJoltDebugger/` ONLY. Zero CkFoundation
> edits — if this phase wants a facility change, that is a STOP, not a patch.
> **Estimate:** 1–2 sessions (3 implementation units + review)

## Goal

After this phase the window is a physics debugger, not a body viewer: the viewport camera obeys the
Unreal editor scheme every user already knows; a "Draw" command lane turns every Phase-5 draw flag
and colour mode on and off and persists the choice; the simulation can be paused and single-stepped
from the toolbar; the detail panel shows the whole Phase-6 body/character sample plus a clickable
contacts list; the outliner multi-selects, the viewport Ctrl+click adds to the selection, Isolate
hides everything else, Follow keeps the camera on the primary; and Ctrl+LMB drags a dynamic body
around by a spring.

## Entry criteria (verify BEFORE writing code — do not assume)

- [ ] Phase 6 exit green and committed; `CkJolt/Claude.md` documents the facility surface below.
- [ ] These facility symbols exist and are JPH-free (read `CkJolt_DebugDrawTarget.h` and
      `CkJolt_Subsystem.h`, do not trust this list): `FCk_Jolt_DebugDrawFlags`,
      `Set_DrawFlags`/`Get_DrawFlags`, `ECk_Jolt_DebugDrawColorMode`, `Set_ColorMode`/`Get_ColorMode`,
      `Get_LegendEntries`, `Set_HighlightedBodies`, `Set_IsolatedBodies`/`Clear_Isolation`,
      `Get_BodySample`, `Get_CharacterSample`, `Set_ContactsRequested`/`Get_SelectionContacts`,
      `Get_Labels`, `Draw_ExternalLine/Box/Sphere/Arrow`, `UCk_Jolt_Subsystem::Request_SetPaused`/
      `Request_StepOnce`/`Get_IsPaused`/`Get_WorldStats`/`Request_BeginDrag`/`Request_UpdateDrag`/
      `Request_EndDrag`.
- [ ] Baseline re-captured: full serial suite + scoped `Jolt`, `JoltDebugger`, `DebuggerLauncher`.

## Design rulings (orchestrator, binding — verbatim)

| ID | Ruling |
|---|---|
| P7-D49 | **Camera = Unreal editor scheme.** RMB-drag = look in place (yaw/pitch, eye fixed); WASD/QE/arrows fly ONLY while RMB held; RMB+wheel = speed; MMB-drag = pan; wheel = dolly along view; LMB-drag = forward/back + yaw (UE style); Alt+LMB = orbit around the current look-at; Alt+RMB = dolly; F = Frame Selection, Home = Frame All (unchanged); LMB click without drag = pick (unchanged 4-px threshold). Ortho: RMB/MMB drag = pan, wheel = zoom, no rotation. Rewrite `InputKey`/`InputAxis`/`Tick_Navigation` only; the look-at is maintained as eye + forward × distance so Frame/orbit still work. Crowd viewport NOT touched. |
| P7-D50 | **"Draw" command lane** — one checkbox per D39 flag (grouped: Bodies / Constraints / Contacts / Labels), colour-mode combo (D42), all persisted in `UCkJoltDebuggerSettings`; legend follows the mode. "In-world draw" lane exposes all six CVars. |
| P7-D51 | **Sim controls** — toolbar Pause/Resume + Step buttons (D43) + Space (toggle) / Enter (step) when the viewport is focused; Pause state shown in the stats header. Stats panel gains D48 fields. |
| P7-D52 | **Detail panel** shows every D44 field (two-column, grouped: Motion / Material / Layers / Shape / Misc; character group when applicable) and a "Contacts" list (D45) whose rows select the other body on click. |
| P7-D53 | **Selection**: outliner multi-select (Ctrl/Shift), primary = last clicked; viewport Ctrl+click adds; all selected highlighted (D46); Isolate/Un-isolate toolbar toggle + `I` hotkey; Follow-selection toggle (camera keeps its offset to the primary's bounds centre every tick). |
| P5-D61 | **STOP-list rulings — the parts that bind this phase.** **S10:** `GetBodyStats()` and the constraint count are sampled every 30 captures — the stat rail **must label those rows "(sampled)"** rather than presenting them as live. **S3:** External draws are **retained NAMED sub-channels** — the drag line pushes to a named channel (e.g. `"JoltDebugger.Drag"`) and is removed with `Clear_External(Name)` on drag end; it does **not** need re-pushing every tick. **S12:** the Phase-5/6 collateral allowance ran the other way — by this phase the facility API is final, so **zero CkFoundation edits** stands. |
| P7-D54 | **Mouse-drag UI** — Ctrl+LMB press on a dynamic body begins drag (D47), drag moves the anchor on the camera-parallel plane through the grab point (Ctrl+wheel moves the plane along the view), release ends; a drag line (external channel) from grab point to anchor. Only when the selected world is the authority (server/standalone) — disabled with a tooltip otherwise. |

## Research facts the executor must not re-derive

- **All key handling in this module lives in ONE place:** `FCkJoltDebugger_3dViewportClient::InputKey`
  (`Viewport/SCkJoltDebugger_3dViewport.cpp:118-221`). There is **no** `OnKeyDown`, no
  `SupportsKeyboardFocus`, no `FUICommandList` anywhere in `CkJoltDebugger`, and **no shared
  `FUICommandList`/`TCommands` infrastructure exists in `CkDebuggerCommon`** either. Every new
  hotkey in this phase (Space, Enter, `I`, Ctrl+0..9) therefore goes in `InputKey` — inventing a
  window-level command list is out of scope.
- **The Jolt viewport client was copied from Crowd's.** `InputAxis` (Jolt `:232-267` ≡ Crowd
  `:457-488`), `Tick_Navigation` (Jolt `:269-295` ≡ Crowd `:490-516`), `Zoom` (Jolt `:378-389` ≡
  Crowd `:649-661`) and `GetCursorWorldRay` (Jolt `:298-332` ≡ Crowd `:550-578`) are line-for-line
  identical. **P7-D49 deliberately breaks that parity on the Jolt side only** — the Crowd file is
  not to be edited, and the divergence is a documented follow-up for the Crowd campaign.
- Current camera scheme being replaced: RMB-drag orbits (`:242-253`), MMB-drag pans (`:255-263`),
  wheel zooms (`:214-218`), WASD/QE/arrows fly unconditionally in perspective (`:276-287`),
  RMB+wheel changes `_CameraSpeed` (`:200-212`), bare `F` frames selection (`:183-190`), `Home`
  frames all (`:192-198`), LMB press records `_PendingPickPress` (`:127-142`) and release picks
  within 4 px² (`:157-172`), `LostFocus` clears the pending press (`:226-230`).
- `SCkJoltDebuggerWindow::BuildCommandGroups` (`Window/SCkJoltDebuggerWindow.cpp:815-849`) builds
  6 lanes today: Primary `JoltRender` (`:982-1011`), Context `JoltTarget` (`:891-907`),
  `JoltCamera` (`:909-980`), `JoltInWorldDraw` (`:851-889`, TWO cvars only —
  `ck.Jolt.DebugDraw.Enabled`, `ck.Jolt.DebugDraw.Velocity`), `JoltPopulations` (`:1013-1027`),
  `JoltLegend` (`:1099-1138`).
- Lane doctrine (`CkDebuggerCommon/CLAUDE.md:551-564`): Primary and Context lanes each stay **one
  physical line and scroll horizontally when narrow** — controls never wrap. **Never construct
  `SCheckBox`**; use `SCkDebug_IconToggle` / `SCkDebug_IconToolbar` for icon booleans,
  `SCkDebug_ToggleSurface` for a rich chip, engine `SSegmentedControl` for a short exclusive choice.
- Outliner is **`ESelectionMode::Single`** (`Window/SCkJoltDebugger_OutlinerPanel.cpp:211`).
  `OnSelectionChanged` ignores `ESelectInfo::Direct` (`:452-453`); `DoSelectItem` (`:498-511`) uses
  `SetSelection(..., ESelectInfo::Direct)`; filter pipeline + pinned selected row `:246-354`
  (pin at `:286-292`, dim at `:513-523`), stable sort `:316-325`. **`CkDebuggerCommon/CLAUDE.md`
  has NO multi-select contract** — only copy-menu guidance at `:144-146` and `:533-534`
  ("multi-select aware — join with `\n`"). This phase writes that behaviour fresh; keep the
  pointer-identity contract (`CLAUDE.md:189-226`) intact.
- Window selection state is a single `TOptional<FCkJoltDebugger_BodySnapshot> _Selection`
  (`SCkJoltDebuggerWindow.h:174`), written only by `DoApplySelection` (`cpp:664-707`), which sets
  the highlight (`:680`), selection bounds (`:687`), outliner selection (`:694-700`) and broadcasts
  only for Outliner/Viewport sources (`:702-706`).
- Detail panel builds every row ONCE in `Construct` (`SCkJoltDebugger_DetailPanel.cpp:63-227`) with
  `MakeRow` (`:243-257`) and a `_RowValues` read-back map (`:250`, `Get_RowValueText` `:259-267`);
  rows are `SCkDebug_KeyValueRow`, entity row is `SCkDebug_EntityRef` (`:269-294`).
- Stats: `FCkJoltDebugger_Stats` (`SCkJoltDebuggerWindow.h:31-52`), filled by `DoRefreshStats`
  (`cpp:508-571`), rendered by `BuildStatRail` (`cpp:1162-1254`) with `MakeSectionHeader`
  (`:1258-1266`) and `MakeStatRow` (`:1270-1298`).
- Settings: `UCkJoltDebuggerSettings` (`Settings/CkJoltDebuggerSettings.h:45`,
  `UCLASS(Config = GameUserSettings)` `:44`, container `Editor` `:50-54`); 6 properties today
  (`RenderMode:59`, `CameraPreset:63`, `ShowJoltBodies:67`, `ShowBakedStaticWorld:71`,
  `ShowSensors:75`, `ShowCharacters:79`). **No `.cpp` exists** — keep it header-only.
  Write sites: `SCkJoltDebuggerWindow.cpp:933-935`, `:1005-1009`, `:1064-1066`; restore
  `DoApplySavedPreferences` (`:1070-1097`).
- Useful shared widgets (do not reinvent): `SCkDebug_IconToggle` / `SCkDebug_IconToolbar` +
  `FCkDebug_IconToggleAction` (`Widgets/SCkDebug_IconToggle.h:52`, `:84`), `SCkDebug_Chip:39`,
  `SCkDebug_CountBadge:15`, `SCkDebug_AlertRow:20`, `SCkDebug_StatusPill:18`,
  `SCkDebug_KeyValueRow:29`, `SCkDebug_SectionHeader:15`, `SCkDebug_InspectorPanel:16`,
  `SCkDebug_EntityRef:43`, `SCkDebug_ValuePill:22`.
- Specs are `IMPLEMENT_SIMPLE_AUTOMATION_TEST` (**never `DEFINE_SPEC`** — `.spec.cpp` is a naming
  convention only), whole file inside `#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS`, flags
  `EditorContext | EngineFilter`. Fixture precedent: a standalone `ck::FEcsWorld{}` +
  `UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry())`
  (`Private/Tests/CkJoltDebuggerOutliner.spec.cpp:61-135`).

## Work items

### Unit A — camera rewrite (P7-D49)

1. `Viewport/SCkJoltDebugger_3dViewport.cpp` — introduce an explicit camera state on the client
   (`_LookAt` already implied by the orbit math at `:242-253`; make it a real member alongside
   `_OrbitDistance`) so that **eye + forward × distance == look-at** is an invariant every path
   maintains. → verify: `Ck.JoltDebugger.Viewport.CameraPresets` still passes unchanged.
2. Rewrite `InputKey` (`:118-221`) mouse handling: RMB press/release = capture for look; MMB =
   pan; Alt+LMB = orbit; Alt+RMB = dolly; wheel = dolly (perspective) / ortho-width zoom (ortho);
   RMB+wheel = `_CameraSpeed`. Keep the LMB pick path (`:127-142`, `:157-172`), the bare-`F` /
   `Home` handlers (`:183-198`), the mid-gesture pick cancel (`:177-180`) and `LostFocus`
   (`:226-230`) **behaviourally unchanged**. → verify: build; `JoltDebugger` scoped suite.
3. Rewrite `InputAxis` (`:232-267`): RMB-drag rotates the camera **in place** (eye fixed, look-at
   recomputed from the new forward — the opposite of today's orbit); Alt+LMB-drag orbits about
   `_LookAt`; MMB-drag pans both eye and look-at; LMB-drag (no Alt, no RMB) = UE-style
   forward/back + yaw. Ortho: RMB and MMB both pan, rotation is rejected. → verify: build.
4. Rewrite `Tick_Navigation` (`:269-295`): flight keys apply **only while RMB is held** and only in
   perspective. → verify: build.
5. New spec `Ck.JoltDebugger.Viewport.CameraSchemeIsUnrealStyle` in
   `Private/Tests/CkJoltDebuggerViewport.spec.cpp` — drive the client's input entry points directly
   (as `CameraPresets` does for `ApplyPreset`) and assert the *discriminating* facts, not that code
   ran: RMB-drag leaves `Get_ViewLocation()` unchanged while `Get_ViewRotation()` changes; Alt+LMB
   drag CHANGES the location while the look-at point stays put; MMB-drag moves the location with
   the rotation unchanged; in ortho a rotate gesture leaves the rotation untouched. → verify:
   `--test-pattern JoltDebugger`.
6. `[EDITOR-VERIFY]` items 1–4 below. Record in `CkJoltDebugger/CLAUDE.md` that the camera table
   (`:69-74`) is rewritten and that Crowd intentionally diverges.

### Unit B — Draw lane, sim controls, stats (P7-D50, P7-D51)

7. `Settings/CkJoltDebuggerSettings.h` — add `DrawFlags` (int32/uint32 mirror of
   `FCk_Jolt_DebugDrawFlags`; store the raw bits, not the struct, so the config text is stable),
   `ColorMode` (`ECkJoltDebugger_ColorModePref` mirroring `ECk_Jolt_DebugDrawColorMode`),
   `bIsolateActive`, `bFollowSelection`, `bShowGrid` (consumed in Phase 8), `RunawayVelocityCmS`
   (Phase 8 default 5000), `CameraBookmarks` (Phase 8). All `Config, EditAnywhere`, header-only.
   → verify: build; `Ck.JoltDebugger.Settings.ConstructRestoresPreferences` extended to round-trip
   the new fields with NON-DEFAULT values (the existing RAII `FScopedPreferences` guard at
   `CkJoltDebuggerSettings.spec.cpp:12-45` already protects the real per-user ini — reuse it).
8. `SCkJoltDebuggerWindow.cpp` — new `BuildDrawGroup()` beside `BuildRenderGroup` (`:982-1011`),
   registered in `BuildCommandGroups` (`:815-849`) as a Context lane `JoltDraw` labelled
   "Jolt debug draw". Contents: one `SCkDebug_IconToolbar` per D39 group (Bodies / Constraints /
   Contacts / Labels) built from a static table beside `Get_PopulationGroups`
   (`:154-188`), plus an `SSegmentedControl` (or `SComboBox` if the mode list outgrows one line)
   for the colour mode. Every toggle writes the settings object + `SaveConfig()` and calls
   `_DebugDrawTarget->Set_DrawFlags(...)` / `Set_ColorMode(...)`. → verify: build; window spec.
9. `BuildLegendGroup` (`:1099-1138`) — source entries from `Get_LegendEntries(Get_ColorMode())`
   instead of `Get_AllColorClasses()`, and rebuild the lane when the mode changes. Population
   toggles keep mapping to BodyClass classes; when the mode is NOT BodyClass, disable the
   population toggles with a tooltip explaining why (they are a BodyClass-visibility mask).
   **If Phase 5 made visibility mode-independent, say so and skip the disable — STOP and report
   rather than guessing.** → verify: build.
10. `BuildInWorldDrawToggles` (`:851-889`) — extend from 2 cvars to all six (names come from the
    Phase-5 research; do NOT invent them — read the CVar registrations in `CkJolt_Subsystem.cpp`).
    Keep the enable-gate pattern already used for `ck.Jolt.DebugDraw.Velocity` (`:871-887`) and the
    "game-viewport only" tooltips. → verify: build.
11. New `BuildSimGroup()` — Primary-lane Pause/Resume + Step buttons calling
    `UCk_Jolt_Subsystem::Request_SetPaused` / `Request_StepOnce`, both disabled with a tooltip when
    there is no world; Space and Enter added to `InputKey`. Pause state shown as a
    `SCkDebug_StatusPill` in the stat rail's summary line (`:1167-1176`). → verify: build.
12. `FCkJoltDebugger_Stats` (`SCkJoltDebuggerWindow.h:31-52`) + `DoRefreshStats` (`cpp:508-571`) +
    `BuildStatRail` (`cpp:1162-1254`) — add a "Simulation" section: paused, last step ms, active
    rigid/soft bodies, body-stat breakdown, constraint count, and **contact pairs (Phase-6 research
    confirmed a `CkContactListener` exists at `CkJolt_Subsystem.cpp:70`, so this row is IN)**.
    **Per P5-D61/S10 the body-stat breakdown and the constraint count are sampled every 30 captures
    — label those rows "(sampled)"** so a lagging number reads as designed rather than as a bug.
    → verify: build; `JoltDebugger` scoped suite.

### Unit C — detail panel, multi-select, isolate, follow, drag (P7-D52, P7-D53, P7-D54)

13. `Data/CkJoltDebugger_Types.h` — the snapshot stays the outliner's flat row; the **sample** is
    NOT copied into it. Add a separate `FCkJoltDebugger_SelectionFacts` holding the facility's
    `FCk_Jolt_DebugDraw_BodySample` / `FCk_Jolt_DebugDraw_CharacterSample` + the contacts array,
    refreshed in `DoRefreshSelectionFacts` (`SCkJoltDebuggerWindow.cpp:623-662`). → verify: build.
14. `Window/SCkJoltDebugger_DetailPanel.{h,cpp}` — extend `Construct` (`cpp:63-227`) with the D52
    groups using `SCkDebug_SectionHeader` + `SCkDebug_KeyValueRow`, all values attribute-bound
    through the existing `MakeRow`/`_RowValues` mechanism (`:243-257`) so `Get_RowValueText` keeps
    working; every row degrades to `--` on an unset sample. Character group rows are collapsed
    (`EVisibility::Collapsed`) unless the primary selection is a Character. Add a "Contacts"
    `SListView` whose rows are `STextBlock`s only (**click-trap rule**,
    `CkDebuggerCommon/CLAUDE.md:156-188`) and whose `OnSelectionChanged` selects the other body
    through the window's selection path. → verify: build; extend
    `Ck.JoltDebugger.Detail.RowsReflectTheSelection` to cover ≥ 6 new rows including the `--`
    degradation and the character-group visibility flip.
15. `Window/SCkJoltDebugger_OutlinerPanel.{h,cpp}` — `ESelectionMode::Multi` (`cpp:211`), a
    `TArray<FRowIdentity> _SelectedIdentities` + primary; `Get_Selection` keeps returning the
    PRIMARY (so the detail panel is untouched) and a new `Get_SelectedAll()` returns the set;
    pinning (`:286-292`) and dimming (`:513-523`) extend to every selected row. Preserve the
    `ESelectInfo::Direct` ignore (`:452-453`) and the set-or-order-change refresh gate (`:329-342`).
    → verify: build; new spec `Ck.JoltDebugger.Outliner.MultiSelectKeepsPrimaryAndSurvivesRefresh`
    (three rows: ctrl-add makes 2 selected with the LAST as primary; a refresh over the same
    entities keeps both selected and the same primary; a filter hides neither selected row).
16. `SCkJoltDebuggerWindow` — replace `_Selection` with `_Selection` (primary, unchanged type) +
    `_SelectionAll` (`TArray<FCkJoltDebugger_BodySnapshot>`); `DoApplySelection` (`cpp:664-707`)
    pushes `Set_HighlightedBodies` for the whole set, `Set_SelectionBounds` for the primary, and
    broadcasts only the primary (echo-suppression contract unchanged). Viewport Ctrl+click adds
    (`HandleViewportBodyPicked`, `cpp:720-757` — the modifier is read in `InputKey` and carried on
    the delegate, NOT read from Slate state in the handler). → verify: build.
17. Isolate: toolbar `SCkDebug_IconToggle` + `I` in `InputKey`; ON pushes
    `Set_IsolatedBodies(<selected keys>)`, OFF calls `Clear_Isolation()`; state persisted;
    re-applied whenever the selection changes while active. Follow-selection: toggle + per-Tick
    camera offset maintenance against `Get_HighlightedBodyBounds()` (already fed into
    `Set_SelectionBounds` at `SCkJoltDebuggerWindow.cpp:363-368`). → verify: build.
18. Drag UI (P7-D54): in `InputKey`, Ctrl+LMB press → `TryPick_Body` at the press position, and if
    the hit body is dynamic call `Request_BeginDrag`; `InputAxis` while dragging → project the
    cursor ray onto the camera-parallel plane through the grab point and call
    `Request_UpdateDrag`; Ctrl+wheel moves the plane along the view; release → `Request_EndDrag`.
    The drag line goes to a **named retained External sub-channel** (P5-D61/S3), e.g.
    `Draw_ExternalLine("JoltDebugger.Drag", GrabPoint, Anchor, …)`, re-pushed only when the anchor
    moves and removed with `Clear_External("JoltDebugger.Drag")` on drag end — **not** re-pushed
    every Slate tick.
    The whole path is gated on the selected world being the authority — compute it once in the
    window (`World->GetNetMode() != NM_Client`) and pass it to the viewport; when false the gesture
    is inert and the toolbar shows a disabled state with a tooltip. → verify: build;
    `[EDITOR-VERIFY]` 9–10.
19. Docs weld: `CkJoltDebugger/CLAUDE.md` — camera table (`:69-74`), command-lane table
    (`:271-278`), selection source/sink tables (`:157-170`), detail row list (`:195-210`),
    prefs table (`:314-318`), spec census (`:341-350`), `[EDITOR-VERIFY]` list (`:367-386`),
    anti-patterns (`:406-424`). Note the `CkJolt/CLAUDE.md` vs `CkJolt/Claude.md` filename
    mismatch in the cross-references at `:386` and `:430` and normalize it.

## Fences

- **Zero CkFoundation edits.** A missing facility API is a STOP, not a local workaround, and it is
  never a reason to read `JPH::PhysicsSystem` from Slate (`CkJoltDebugger/CLAUDE.md:409-412` — this
  exact rule was written because a "harmless" velocity read got in once).
- No `SCheckBox`; no Slate structure rebuilt in `Tick`; no click-consuming widget inside an
  `STableRow`; row `Text` stays attribute-bound.
- No new window-level `FUICommandList` — keys go in the viewport client's `InputKey`.
- The Crowd viewport is **not** touched, even though the camera code is currently identical.
- Never key a row or lookup on a raw body key where a handle is available (0 is a valid key).
- Drag is the ONLY sim-mutating action this module may perform, and only on an authority world.

## `[EDITOR-VERIFY]`

1. RMB-drag looks around without moving the eye; WASD/QE fly only while RMB is held; RMB+wheel
   changes fly speed.
2. MMB-drag pans; wheel dollies; Alt+LMB orbits the current look-at; Alt+RMB dollies.
3. In an ortho preset, RMB and MMB pan and the view never rotates; wheel zooms.
4. LMB click still picks; LMB drag-then-release still does not pick; bare `F` frames the selection,
   `Home` frames all, `Ctrl+F`/`Alt+F` still do nothing.
5. Every Draw-lane toggle visibly changes the viewport (velocity arrows, AABBs, COM axes,
   constraints, contacts, labels) and survives an editor restart.
6. Switching colour mode recolours the bodies and the legend follows; Island mode shows bodies in a
   pile sharing a colour.
7. Pause freezes the sim (in-world too), Step advances exactly one step, Space/Enter work with the
   viewport focused, and the stat rail shows the paused state + step ms.
8. Ctrl+click in the outliner and in the viewport builds a multi-selection, all of it highlighted;
   Isolate hides everything else; `I` toggles it; Follow keeps the camera on a moving body.
9. Ctrl+LMB drag on a dynamic body pulls it around with a visible drag line; release drops it;
   dragging a static or kinematic body does nothing.
10. On a PIE **client** world the drag toolbar is disabled with an explanatory tooltip and Ctrl+LMB
    does nothing.

## Exit criteria — ALL in the same commit as the last work item

- [ ] Scoped serial `--test-pattern JoltDebugger` green; `--test-pattern Jolt` green (unchanged —
      this phase must not move the facility numbers); `--test-pattern DebuggerLauncher` census 3/3
- [ ] Full serial suite == baseline set (no new failures)
- [ ] ≥ 3 new debugger specs land (camera scheme, outliner multi-select, extended detail rows) and
      the settings round-trip spec covers every new preference
- [ ] Adversarial review (fresh Opus drafts the triage; orchestrator ratifies) → fix-up → gate of
      record re-run on the FINAL artifact
- [ ] `CkJoltDebugger/CLAUDE.md`, PLAN, PROGRESS updated; commit LOCAL only (ship withheld, P8-D60)
