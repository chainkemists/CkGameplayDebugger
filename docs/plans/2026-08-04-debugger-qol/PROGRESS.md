# Debugger QoL campaign - PROGRESS.md

## Current state

**As of 2026-08-07:** independent review remediation is implemented for Gates 03 and 04; post-rebase automation and renewed editor acceptance remain pending.
**Baseline being diffed against:** today's `Ck.Debugger` build stopped before tests because the checkout paired a 25-commit-behind CkFoundation tip with a CkTests dependency on `CkEntityVisualizer`. Exact inherited evidence is in `Saved/Logs/Baseline-DebuggerQoL-20260807-053052.md`; the older 18/18 run remains historical evidence only.
**Next action:** commit the reviewed ownership boundaries, rebase affected plugins onto refreshed `origin/dev`, and run the broad `Ck.Debugger` gate on coherent tips.
**Blocked on:** no external blocker; editor-only visual and interaction verification remains after automation.

## Decision log

| Date | Decision | Why | Revisit when |
|---|---|---|---|
| 2026-08-04 | Preserve `CkDebug_SelectionSync` as the broadcast/lineage spine. | Its receiver guards and two-way owner-lineage match already solve nested entity mapping safely. | A debugger cannot express its entity mapping through lifetime ownership. |
| 2026-08-04 | Add a separate entity-target route registry rather than callbacks on launcher descriptors. | Keeps the launcher catalog plain and makes unload-safe entity targeting independently testable. | Never; this preserves the documented launcher boundary. |
| 2026-08-04 | Relink all existing ECS tree nodes on observed structural revision without recreating nodes. | Owner transfer changes hierarchy but not membership; O(n) relink occurs only on churn and preserves Slate identity. | Profiling shows churn relinking is material at target scale. |
| 2026-08-04 | Budget overlay rows/sections before Slate render. | Raw clipping is unrecoverable and scrolling is unavailable. | Overlay becomes interactive. |
| 2026-08-04 | Standardize a structural window frame, not feature action semantics. | GOAP, ECS, SM, and Scheduler have legitimately different control surfaces. | Multiple debuggers converge on an identical action set. |
| 2026-08-04 | Keep continuous overlay sync opt-in and adoption-only. | Debugging focus should update compatible tabs already in use without opening or foregrounding tools unexpectedly. | A deliberate workflow needs auto-opening tabs. |
| 2026-08-04 | Expand focused marker subtrees through call-scoped roots. | The shared marker gather can reveal hovered, locked, and pinned descendants without retaining PIE handles or making unrelated branches unlimited. | A non-lifetime hierarchy must participate. |
| 2026-08-05 | Prefer the deeper candidate only when hierarchy gaze scores tie. | Full-depth parents and descendants commonly share a transform; preserving the existing heuristic for non-ties avoids broad focus changes while making the visible nested entity selectable. | Live PIE shows near-co-located, non-tied descendants still cannot be aimed reliably. |
| 2026-08-05 | Invalidate handle-owning debugger state at a common session boundary, while keeping reset ownership feature-local. | Tabs outlive PIE registries; a handle-free Common broadcast gives every debugger the same teardown moment without centralizing feature state or masking stale reads with guards. | A debugger owns no registry-backed state across worlds. |
| 2026-08-05 | Split Insights Analyzer at the UI/core boundary. | CkFoundation owns trace analysis/reporting and the commandlet; CkGameplayDebugger owns editor debugger presentation and shared chrome. | Insights must remain available in projects that deliberately omit CkGameplayDebugger. |
| 2026-08-05 | Give every launcher debugger an inline common-menu action surface. | A shared frame without accessible feature actions satisfies visual framing but not the requested one-click workflow. | A debugger genuinely has no meaningful boolean inspection lens after its feature surface changes. |
| 2026-08-05 | Keep ECS contextual filters contextual while replacing their raw checkbox presentation. | Inspector tokens, cards, and All/Any or density choices are not debugger-wide state; promoting them would overload the global bar. | A contextual control becomes globally meaningful across every ECS page. |
| 2026-08-05 | Keep the title and direct action row auto-width and adjacent; flexible width belongs after the actions. | Editor acceptance showed that action-first ordering shifted titles, while a flexible title slot pushed the corrected action row to the far right. | Never for debugger menu actions; a separate non-menu component may adopt another policy with explicit evidence. |
| 2026-08-07 | Give the direct action surface the remaining header width and wrap at narrow widths. | An auto-width nine-action EQS row has no clipping or overflow policy; wrapping preserves one-click visibility while keeping the title anchored and Debuggers at the right edge. | A measured minimum tab width proves every supported action census always fits one row. |

## Dated entries

### 2026-08-05 - Complete toggle coverage baseline and design

- Confirmed: all 15 launcher windows use `SCkDebug_WindowChrome`, but only Insights currently passes the chrome's toolbar slot; existing feature icon toolbars remain inside bespoke body toolbars.
- Confirmed: ECS contains nine direct `SCheckBox` construction sites: one boolean mode, two inspector-token controls, two All/Any radio controls, two archetype cards, one feature-rail chip, and one grid-density radio family.
- Locked: add an inline common menu-action slot, give every launcher debugger at least one state-backed icon action, replace ECS rich toggles through a common content-bearing toggle surface, and use `SSegmentedControl` for short exclusive choices.
- Selected low-risk actions backed by existing state: ECS overlay/picker actions; SM display actions; Map enabled POIs; Dialog active cooldowns; A* capture pause; GOAP pause-on events; Crowd world diagnostics; EQS overlay lenses; Aggro engaged owners; Scheduler freeze capture; Object Pooling pools in use; Jolt debug draw; UI active layer; Input active actions; Insights show all.
- Baseline snapshot: `D:/Repos/CkPlugins/_scratch/baseline_debugger_toggle_coverage_20260805-183231.md`. A fresh broad run was blocked because Toolbox PID 44476 owns the editor log; no competing editor was launched.

### 2026-08-05 - Complete toggle coverage implementation and verification

- Added `MenuActionsContent` to the common window chrome and constrained it to the remaining top-bar width so `SCkDebug_IconToolbar` can switch to its compact overflow layout. Every one of the 15 launcher windows supplies this slot.
- Migrated existing ECS, State Machine, A*, GOAP, Crowd, EQS, Scheduler, and Insights booleans into the common bar. Added default-off presentation lenses to Map, Dialog, Aggro, Object Pooling, UI, and Input, plus CVar-backed Jolt draw/velocity actions.
- Replaced ECS's nine raw checkbox construction sites with shared icon toggles, `SCkDebug_ToggleSurface` for rich contextual cards/chips, and engine segmented controls for All/Any plus 2-6 columns. Final feature-local raw-checkbox file count is zero.
- An independent adversarial review found Scheduler's combined frozen/history state could remain on after one off click. The final handler clears both state dimensions; no other concrete integration finding remained.
- Final UnrealToolbox Development Editor build succeeded; all touched module binaries are newer than their latest source. The full `Debugger` pattern passed 18/18, failed 0, skipped 0, contaminated 0 in `Saved/Logs/DebuggerToggleCoverage-Final-20260805.log`.
- `git diff --check` and the 15/15 action/static census passed. Visual placement, overflow, live toggle state, and contextual interaction remain editor-only acceptance row 15.

### 2026-08-05 - Common action-bar visual correction

- Editor acceptance failed: `MenuActionsContent` was right-aligned inside the flexible header slot, and `SCkDebug_IconToolbar` interpreted that constrained width as a compact state. ECS visibly showed three actions plus a `•••` menu containing the remaining actions.
- Corrected the shared contract rather than individual windows: `MenuActionsContent` is now the first auto-width header slot, the title consumes the remaining width, and the Debuggers switcher stays at the right edge.
- Removed responsive partitions and the overflow menu from the canonical icon toolbar. All action descriptors now validate atomically into one direct row, including ECS, Crowd, and EQS, which previously supplied per-window direct-count limits.
- The final Development Editor rebuild succeeded and all six touched module binaries are newer than their latest source. A clean post-build `Debugger` run passed 18/18, failed 0, skipped 0, contaminated 0 in `Saved/Logs/DebuggerDirectMenuActions-TestOnly-20260805.log`.
- The first combined build/test invocation discovered the pre-build toolbar test name and retried once after the stable test path was restored. Its final rollup was green, but the clean post-build test-only log above is the authoritative automation evidence.
- The first correction's screenshot remains red visual baseline evidence: action-first ordering made the title shift between debuggers because each action row has a different width.
- Corrected the shared slot order again to title, direct actions, then Debuggers. The title remains the flexible ellipsizing slot, so the action row stays fully visible without moving the title's left edge.
- The final Development Editor build succeeded; the full `Debugger` pattern passed 18/18, failed 0, skipped 0, contaminated 0 in 39s in `Saved/Logs/DebuggerTitleThenActions-Final-20260805.log`. The fresh log contains no ensure, script-error, automation-error, no-match, compiler, linker, fatal, or failed-result markers.
- Renewed editor acceptance exposed a second placement defect: the title's `FillWidth` slot consumed the header and pushed the action row against the right-edge Debuggers menu.
- Corrected the shared geometry to auto-width title, auto-width direct actions, flexible spacer, then Debuggers. The title and actions now stay adjacent while only the spacer expands.
- The final Development Editor build succeeded; the full `Debugger` pattern passed 18/18, failed 0, skipped 0, contaminated 0 in 40s in `Saved/Logs/DebuggerAdjacentMenuActions-Final-20260805.log`. `CkDebuggerCommon` is newer than the corrected source, and every fresh diagnostic scan count is zero.
- Renewed live placement and interaction acceptance is still pending.

### 2026-08-05 - Insights Analyzer migration baseline

- Ran: fresh-boot UnrealToolbox `Ck.DebuggerLauncher` baseline -> 2/2 passed, 0 failed in 26s (`Saved/Logs/InsightsAnalyzerMigration-Baseline-20260805.log`).
- Confirmed: the current catalog contains one `CkInsightsAnalyzerTab` descriptor and a registered spawner before the ownership move.
- Confirmed: Foundation's only current dirt is unrelated untracked documentation; there is no overlap under `Source/CkInsightsAnalyzer` or `CkFoundation.uplugin`.
- Locked: retain `CkInsightsAnalyzerTab`, retain `-run=CkInsightsAnalyzer`, and make the new `CkInsightsDebugger` module the sole tab-spawner and descriptor owner.
- Confirmed by the final build: Foundation can drop every Slate/editor UI dependency after moving the tab and chart sources.

### 2026-08-07 - Independent review remediation

- Captured a fresh baseline before edits. Project generation succeeded, then the build stopped at the inherited checkout mismatch `Could not find definition for module 'CkEntityVisualizer'`; tests did not run.
- Review found implicit close-during-load cleanup in Insights. `SCkInsightsAnalyzerTab` now cancels its ticker and pending trace session in its destructor, `CkInsightsDebugger` drops an unused `CkEcs` dependency, and the launcher catalog asserts the descriptor's exact public contract.
- Review found that the all-direct action row could clip nine EQS actions at narrow widths. The title remains left-anchored, the action surface fills the remaining width and wraps, and the Debuggers control remains right-anchored.
- Four Crowd navigation controls added after the original Gate 04 census now use `SCkDebug_ToggleSurface`; the feature-local raw-checkbox invariant is restored. The unrelated Crowd path-network `OnPaint` trace remains excluded for its owning investigation.
- Post-rebase build/test evidence and editor matrix rows 14-15 remain pending; do not treat this entry as an acceptance claim.

### 2026-08-05 - Insights Analyzer ownership migration

- Moved the analyzer tab and frame chart into the new UncookedOnly `CkInsightsDebugger` module. The user-facing tab ID remains `CkInsightsAnalyzerTab`, preserving saved layouts and launcher discovery.
- `CkInsightsDebugger` now owns the Nomad spawner and launcher descriptor, wraps the analyzer in `SCkDebug_WindowChrome`, and presents `Show all` through `SCkDebug_IconToggle`. The launcher's temporary proxy registration was removed.
- CkFoundation's `CkInsightsAnalyzer` now contains only trace parsing, reports/JSON, logging, and the existing `-run=CkInsightsAnalyzer` commandlet contract; its Build.cs has no Slate, input, editor-style, tab, or workspace-menu dependencies.
- Final Development Editor build succeeded and the baseline-comparable `Ck.DebuggerLauncher` suite passed 2/2 with zero failures in 32s: `Saved/Logs/InsightsAnalyzerMigration-BuildTest-20260805.log`.
- Fresh diagnostics contained no fatal errors, ensures, AngelScript errors, linker/compiler failures, or errors naming the moved modules. The existing `CkAutoTest_Base.as` shadow warning and unrelated startup/cache warnings remain inherited.
- `git diff --check` passed for both changed plugins. An independent read-only adversarial review found no migration defects across dependency direction, registration/teardown ordering, saved tab compatibility, weak-ticker lifetime, shared chrome/toggle integration, or commandlet compatibility.
- Live Analyzer chrome, icon state, file-dialog cancellation, and close-during-load behavior remain editor-only verification.

### 2026-08-04 - Research and baseline

- Ran: UnrealToolbox Development editor build plus `Debugger` tests -> build succeeded, 12/12 passed.
- Ran: UnrealToolbox `DebugOverlay` tests -> 7/7 passed.
- Confirmed: overlay focus-card height is hard-clipped; every non-empty provider section renders unbounded rows; default All enables all fields for 24 providers.
- Confirmed: GOAP overlay Goal and Action values are proxies rather than the goal/action names; PathNetworkFollower overloads one field tag with status, corridor, and goal rows.
- Confirmed: overlay quick-select calls the ECS-only navigator but does not broadcast selection; GOAP and Crowd already receive hierarchy-aware broadcasts.
- Confirmed: ECS owner transfer advances the debug revision but produces an empty membership diff, so existing parent/child links remain stale until ForceFullRefresh.
- Confirmed: no common debugger window frame exists; 14 plugin-owned standalone windows compose bespoke top/status surfaces.
- Inferred pending editor proof: destroyed ECS entities follow the removal diff correctly; this requires the final PIE acceptance run.
- Follow-ups recorded, not chased: inherited SQLite search-cache error, `CkAutoTest_Base.as` shadow warning, and overlay cvar discovery performance warnings.

### 2026-08-04 - Implementation checkpoint

- Added a reload-safe entity-target registry, on-demand ECS primary-selection provider, closest-lineage resolver, common `Open In` links, and a route-aware common ECS-selection action.
- ECS structural refresh now relinks existing nodes after every observed cache revision, preserving node identity while correcting owner transfers and destroyed-entity membership.
- GOAP, Crowd, A*, and State Machine routes are registered with closest-lineage resolution. Aggro remains intentionally unregistered until it owns a real selected-entity model.
- Focus-card rendering now applies configurable total/per-section budgets before Slate, prioritizes GOAP/Crowd/PathNetwork/A* data, and reports omitted rows. GOAP and PathNetwork fields are truthful and legacy serialized field tags remain compatible.
- Added `SCkDebug_WindowChrome`; all 14 plugin-owned standalone debugger windows use it. Its common Debuggers menu and status strip preserve each tool's existing specialized root, and entity sync appears only for registered target routes.
- The first integration build exposed four compile-time seams in new code: Slate slot visibility, Crowd fragment include ownership, GOAP planner/action includes, and an automation assertion helper. All four were corrected before the final build.

### 2026-08-04 - Verification checkpoint

- Development Editor Win64 build passed through UnrealToolbox: `Saved/Logs/DebuggerQol-Final-Build2-20260804.log`.
- Baseline-comparable `Debugger` suite passed 12/12 with an empty failing set: `Saved/Logs/DebuggerQol-Final-Debugger-20260804.log`.
- Fresh-discovery `Ck.DebuggerCommon` tests passed 2/2, covering invalid rejection/callback suppression plus register/replace/stale-unregister behavior: `Saved/Logs/DebuggerQol-Final-DebuggerCommon-20260804.log`.
- Fresh-discovery `DebugOverlay` suite passed 9/9, up from the baseline 7/7 due to the new focus-budget and legacy-provider compatibility tests: `Saved/Logs/DebuggerQol-Final-DebugOverlay-20260804.log`.
- Fresh diagnostic scans contain no test failures, fatal errors, ensures, AngelScript errors, or errors naming touched debugger files. Baseline SQLite/FileInfo, AngelScript shadow, deferred-regeneration, and overlay-cvar warnings remain inherited. The fresh-discovery run also restarted local Zen/DDC during discovery and recovered; the focused tests subsequently passed.
- `git diff --check` passes. A read-only adversarial review reported no concrete correctness findings. No commit or push was requested.

### 2026-08-04 - Follow-up input, focus sync, and depth checkpoint

- Committed the previously verified campaign as debugger commit `6d4148c` (`feat: improve debugger entity workflows`) before starting the follow-up; no push was requested.
- Renamed the one-shot common status action from `Use ECS <id>` to `Sync from ECS <id>`. It remains the reverse-direction pull from the ECS primary selection into one debugger.
- Added opt-in `Sync hovered entity` and `Full depth for focus` toggles to the ECS toolbar's On-Screen Overlay popover. Continuous focus sync adopts into already-open ECS, GOAP, Crowd, A*, and State Machine tabs without opening them; lineage matching handles nested entities.
- Changed shared entity-marker Max Depth default from unlimited to 0. When full-depth focus is enabled, hovered, locked, and pinned roots plus their lifetime descendants bypass Max Depth while unrelated branches remain gated.
- Replaced the overlay input processor's per-tick key set with an ordered, timestamped per-key press buffer. Identical rapid presses can no longer collapse before double-tap evaluation; the processor remains passive and ignores repeats/invalid keys.
- An adversarial review found number-only sync dedupe and unintended ECS picker-hover broadcasts. The final implementation dedupes by full handle generation and scopes continuous sync to the on-screen overlay; ECS picker selection remains explicit click behavior.
- Final Development Editor rebuild passed: `Saved/Logs/DebuggerQol-Followup-Build2-20260804.log`.
- Fresh-discovery `DebugOverlay` passed 10/10, including the new press-buffer regression: `Saved/Logs/DebuggerQol-Followup-DebugOverlay-Fresh-20260804.log`.
- Baseline-comparable `Debugger` passed 14/14: `Saved/Logs/DebuggerQol-Followup-Debugger-20260804.log`. Fresh diagnostic scans found no fatal errors, ensures, test failures, AngelScript errors, or errors naming touched debugger files.
- Follow-up changes were committed after review as `b5d8faf`; live editor rows 8-10 remain pending.

### 2026-08-05 - Overlay visual and nested-selection follow-up

- The focused pinned entity now carries its cyan ring on the primary card while the duplicate pinned card remains suppressed. Previously the dedupe removed the pinned card and the primary-card path hardcoded `bIsPinned=false`.
- Full-depth candidates already shared the marker/candidate snapshot. Co-located parent/child entities could still tie in gaze score, leaving the parent selected by gather order; equal-score selection now prefers the deeper hierarchy candidate.
- Only the focus-card legend pill background uses a muted provider color (`0.55` alpha). Legend text, normal provider/field chips, and in-world plates keep their existing colors.
- Pre-change `DebugOverlay` baseline passed 10/10: `Saved/Logs/DebuggerQol-VisualFollowup-Baseline-20260805.log`.
- The nested-selection regression reproduced red before the fix (0/1, expected child index 1 but selected parent index 0): `Saved/Logs/DebuggerQol-VisualFollowup-SelectionRed-20260805.log`.
- Final UnrealToolbox Editor build succeeded and `DebugOverlay` passed 10/10: `Saved/Logs/DebuggerQol-VisualFollowup-Final-BuildDebugOverlay-20260805.log`. The broader `Debugger` suite passed 14/14: `Saved/Logs/DebuggerQol-VisualFollowup-Final-Debugger-20260805.log`.
- `git diff --check` passed and an independent read-only review found no concrete issues. Visual rows 11-13 remain editor-only verification.

### 2026-08-05 - Stale world-selection lifecycle follow-up

- Root cause: ECS world identity changes were detected, but selection cleanup lived in the Entity List widget and called `Clear_Selection`, which pushed the old registry-backed handle into Back history. Programmatic world changes could bypass that widget callback entirely.
- Added a handle-free debugger session-invalidation signal in Common. ECS, Crowd, and A* keep ownership of their own reset logic and clear handle-bearing models, collectors, and UI caches on both session and selected-world changes; GOAP and State Machine retain their already-existing feature-owned resets.
- The common world-selector model now invalidates its selected world at `OnWorldCleanup` and broadcasts the normal world-change event, so every debugger using the shared selector gets the same pre-teardown boundary without reacting to unrelated world cleanup.
- ECS now clears selection and Back/Forward history atomically, deactivates the picker, empties world caches, tree nodes, pins, recents, activity/inspector consumers, and only then permits a new-world refresh.
- Stateful inspector targets, tree expansion references, and overview-graph keys are explicitly released on deactivation so view-local caches cannot retain a previous registry.
- Added `Ck.EcsDebugger.Selection.WorldChangeResetDropsHistory`, a pure registry-backed regression proving that a world reset broadcasts empty state and cannot restore the old handle through Back/Forward.
- The final Development Editor build and focused Debugger gate passed 17/17 in `Saved/Logs/DebuggerIconToolbar-FinalVerified-20260805.log`; the lifecycle regression is included in that gate.

### 2026-08-05 - ECS Overview tab re-entry fix

- Root cause: tab selection rebuilt fresh family/card/singleton containers while retaining the previous tree's presented-key arrays and widget caches. With unchanged world data, refresh treated the new containers as already populated and left them empty.
- Dashboard content rebuild now invalidates container-bound presentation state before constructing the new Slate tree.
- Added `Ck.EcsDebugger.Dashboard.ContentRebuildInvalidatesPresentation`; the final Development Editor build succeeded and the focused ECS debugger gate passed 7/7 in `Saved/Logs/EcsOverviewTabReentry-FinalRerun-20260805.log`.
- The fix was committed as `f28fd88`; the live Overview -> Graph -> Overview visual round-trip remains editor-only verification.

## Editor acceptance matrix

| ID | Exact action | Expected observation | Status |
|---|---|---|---|
| `[EDITOR-VERIFY]` 1 | Start a representative NPC gym in PIE with GOAP, Crowd, State Machine, and A* entities. Open ECS plus those four dedicated debuggers. Double-Ctrl quick-select the NPC from the overlay. | ECS scrolls to the entity; every already-open compatible debugger adopts and reveals the closest lineage match without an echo loop. | Pending |
| `[EDITOR-VERIFY]` 2 | In the ECS inspector, select a create-style GOAP owner, then each Crowd/A*/State-Machine-capable owner or nested entity. Click each visible `Open In` button. | Only capable buttons appear. The existing tab opens/focuses and immediately selects its exact, ancestor, or descendant match. | Pending |
| `[EDITOR-VERIFY]` 3 | Select an ECS entity, then click the common `Sync from ECS <id>` status action in GOAP, Crowd, A*, and State Machine. Repeat with a nested feature entity and an unsupported sibling. | Exact matches win, otherwise the nearest ancestor/descendant wins deterministically; unrelated siblings are never selected; unsupported routes remain disabled/absent. | Pending |
| `[EDITOR-VERIFY]` 4 | Keep ECS open while spawning, destroying, and transferring a child from owner A to owner B. Do not press Refresh. | Spawned entities appear, destroyed entities disappear, and the same child row regroups under B at the next bounded refresh while surviving selection/expansion remains stable. | Pending |
| `[EDITOR-VERIFY]` 5 | Use the AI overlay preset on an attribute-heavy NPC and inspect GOAP/navigation failure and active-plan cases. | The card renders no more than 18 total rows and four per section; GOAP/Crowd/PathNetwork/A* sections outrank generic attributes; omission summaries are exact; GOAP and path rows describe actionable status/active/plan/cost/failure/corridor/goal data. | Pending |
| `[EDITOR-VERIFY]` 6 | Open ECS, State Machine, UI, Scheduler, A*, GOAP, Crowd, EQS, Input, Object Pooling, Jolt, Map, Dialog, Aggro, and Insights Analyzer. Exercise the `Debuggers` menu and narrow-dock each tab. | All 15 tabs share the title/menu/content/status frame, retain feature controls, and stay reachable when narrow. Only registered entity-target tools show the ECS adoption action. | Pending |
| `[EDITOR-VERIFY]` 7 | With ECS, Crowd, GOAP, A*, and State Machine open, select and pin an entity, use Back/Forward at least once, then move gyms or stop/restart PIE and repeat one ECS adoption action. | ECS selection/history/pins clear before the old registry dies; Crowd/A* release their selected/cache state; no invalid-registry `FFragment_LifetimeOwner` ensure storm occurs; the next action uses only the new world's selection. | Pending |
| `[EDITOR-VERIFY]` 8 | In the ECS toolbar's On-Screen Overlay popover, enable `Sync hovered entity`, then aim the overlay at top-level and nested NPC entities with ECS, GOAP, Crowd, A*, and State Machine already open. | Every compatible open debugger adopts the closest lineage match once per focus change; closed tabs stay closed. | Pending |
| `[EDITOR-VERIFY]` 9 | Leave Max Depth at its default 0, enable `Full depth for focus`, then hover, lock, and pin an entity with nested descendants. | Only top-level markers are shown globally; the hovered/locked/pinned entity's complete subtree is shown, while unrelated deep branches remain hidden. | Pending |
| `[EDITOR-VERIFY]` 10 | In possessed and F8-ejected PIE, exercise double-Shift pin, double-Ctrl ECS quick-select, double-V co-located cycle, and double-Backspace unpin-all with rapid and slow tap pairs. | Rapid pairs trigger exactly once even if both presses land in one overlay tick; slow pairs do not trigger; input remains available to Slate/PIE. | Pending |
| `[EDITOR-VERIFY]` 11 | Pin entity A, aim at B, then aim back at A; repeat while A is focus-locked, then unpin A while it remains focused. | A has one cyan-outlined card in both primary and secondary positions, never duplicates; cyan wins over amber while pinned, and unpin restores amber or no ring as appropriate. | Pending |
| `[EDITOR-VERIFY]` 12 | With Max Depth 0 and `Full depth for focus` enabled, pin a root whose parent/child markers share a transform, then aim at the expanded hierarchy and exercise the co-located cycle. | The visible deeper candidate can become focus rather than the parent winning permanently by gather order; unrelated hidden descendants remain ineligible. | Pending |
| `[EDITOR-VERIFY]` 13 | Compare a top-left focus-card legend with its normal provider chips and with the same provider's in-world near plate. | Only the legend pill backgrounds are visibly muted; legend text is unchanged, and normal card chips plus in-world pills/plates retain full saturation. | Pending |
| `[EDITOR-VERIFY]` 14 | Open the Launcher and click Insights Analyzer twice. Toggle `Show all`, open and cancel the `.utrace` picker, then close the tab during and after loading a trace. | One saved-layout-compatible `CkInsightsAnalyzerTab` opens and then focuses; common Debuggers/status chrome is present; the Tree icon visibly toggles; cancellation and teardown are safe. | Pending |
| `[EDITOR-VERIFY]` 15 | Open every launcher debugger and exercise its common-bar actions: ECS overlays/picker; State Machine Tasks/Compact/Frames; Map enabled POIs; Dialog active cooldowns; A* pause; GOAP pause-on-replan/failure; Crowd diagnostics; EQS pause/overlays; Aggro engaged owners; Scheduler freeze; Object Pooling in-use pools; Jolt draw/velocity; UI active layer; Input active actions; Insights Show all. Then exercise ECS inspector selection/hide, All/Any, attribute exclusion, feature chips, archetype cards, and 2-6 columns. | Every title starts at the same left edge and is followed immediately by its directly visible menu actions; no `•••` or action dropdown appears at normal and narrow widths. Each action is one-click, visibly reflects live state, and has no duplicate feature-toolbar control. Flexible space separates the action row from the right-edge Debuggers menu. ECS contextual controls retain their original state owner and behavior; no raw checkbox presentation remains. | Pending |

## Open items

| Item | Status | Next step |
|---|---|---|
| Gate 00 | Implementation verified; editor acceptance pending | Run matrix rows 1-4 and 7. |
| Gate 01 | Implementation verified; editor acceptance pending | Run matrix rows 5, 9, and 11-13. |
| Gate 02 | Implementation verified; editor acceptance pending | Run matrix rows 6-7. |
| Gate 03 | Implementation verified; editor acceptance pending | Run matrix rows 6 and 14. |
| Gate 04 | Implementation verified; renewed editor acceptance pending | Rerun matrix row 15. |

**Rule:** do not convert the editor-acceptance boundary into a completion claim while any matrix row remains pending.
