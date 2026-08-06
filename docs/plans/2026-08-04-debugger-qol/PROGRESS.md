# Debugger QoL campaign - PROGRESS.md

## Current state

**As of 2026-08-05 (root `7850d85`, debugger `f28fd88`):** all three implementation gates and the follow-up automation boundaries are verified and committed; live editor acceptance remains pending.
**Baseline being diffed against:** Development editor build succeeded; `Debugger` 12/12 and `DebugOverlay` 7/7, both with an empty failing set. Snapshot: `D:/Repos/CkPlugins/_scratch/baseline_debugger_qol_20260804-1622.md`.
**Next action:** run the `[EDITOR-VERIFY]` matrix below in a representative NPC gym/PIE session, including the pinned-card, nested-hover, and legend rows, then record the observed pass/fail result for each row.
**Blocked on:** nothing.

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

## Dated entries

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
| `[EDITOR-VERIFY]` 6 | Open ECS, State Machine, UI, Scheduler, A*, GOAP, Crowd, EQS, Input, Object Pooling, Jolt, Map, Dialog, and Aggro. Exercise the `Debuggers` menu and narrow-dock each tab. | All 14 tabs share the title/menu/content/status frame, retain feature controls, and stay reachable when narrow. Only registered entity-target tools show the ECS adoption action. | Pending |
| `[EDITOR-VERIFY]` 7 | With ECS, Crowd, GOAP, A*, and State Machine open, select and pin an entity, use Back/Forward at least once, then move gyms or stop/restart PIE and repeat one ECS adoption action. | ECS selection/history/pins clear before the old registry dies; Crowd/A* release their selected/cache state; no invalid-registry `FFragment_LifetimeOwner` ensure storm occurs; the next action uses only the new world's selection. | Pending |
| `[EDITOR-VERIFY]` 8 | In the ECS toolbar's On-Screen Overlay popover, enable `Sync hovered entity`, then aim the overlay at top-level and nested NPC entities with ECS, GOAP, Crowd, A*, and State Machine already open. | Every compatible open debugger adopts the closest lineage match once per focus change; closed tabs stay closed. | Pending |
| `[EDITOR-VERIFY]` 9 | Leave Max Depth at its default 0, enable `Full depth for focus`, then hover, lock, and pin an entity with nested descendants. | Only top-level markers are shown globally; the hovered/locked/pinned entity's complete subtree is shown, while unrelated deep branches remain hidden. | Pending |
| `[EDITOR-VERIFY]` 10 | In possessed and F8-ejected PIE, exercise double-Shift pin, double-Ctrl ECS quick-select, double-V co-located cycle, and double-Backspace unpin-all with rapid and slow tap pairs. | Rapid pairs trigger exactly once even if both presses land in one overlay tick; slow pairs do not trigger; input remains available to Slate/PIE. | Pending |
| `[EDITOR-VERIFY]` 11 | Pin entity A, aim at B, then aim back at A; repeat while A is focus-locked, then unpin A while it remains focused. | A has one cyan-outlined card in both primary and secondary positions, never duplicates; cyan wins over amber while pinned, and unpin restores amber or no ring as appropriate. | Pending |
| `[EDITOR-VERIFY]` 12 | With Max Depth 0 and `Full depth for focus` enabled, pin a root whose parent/child markers share a transform, then aim at the expanded hierarchy and exercise the co-located cycle. | The visible deeper candidate can become focus rather than the parent winning permanently by gather order; unrelated hidden descendants remain ineligible. | Pending |
| `[EDITOR-VERIFY]` 13 | Compare a top-left focus-card legend with its normal provider chips and with the same provider's in-world near plate. | Only the legend pill backgrounds are visibly muted; legend text is unchanged, and normal card chips plus in-world pills/plates retain full saturation. | Pending |

## Open items

| Item | Status | Next step |
|---|---|---|
| Gate 00 | Implementation verified; editor acceptance pending | Run matrix rows 1-4 and 7. |
| Gate 01 | Implementation verified; editor acceptance pending | Run matrix rows 5, 9, and 11-13. |
| Gate 02 | Implementation verified; editor acceptance pending | Run matrix rows 6-7. |

**Rule:** do not convert the editor-acceptance boundary into a completion claim while any matrix row remains pending.
