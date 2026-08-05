# Debugger QoL campaign - PROGRESS.md

## Current state

**As of 2026-08-04 (root `7850d857`, debugger `e31bc646`):** all three implementation gates are build- and automation-verified; live editor acceptance remains pending.
**Baseline being diffed against:** Development editor build succeeded; `Debugger` 12/12 and `DebugOverlay` 7/7, both with an empty failing set. Snapshot: `D:/Repos/CkPlugins/_scratch/baseline_debugger_qol_20260804-1622.md`.
**Next action:** run the `[EDITOR-VERIFY]` matrix below in a representative NPC gym/PIE session, then record the observed pass/fail result for each row.
**Blocked on:** nothing.

## Decision log

| Date | Decision | Why | Revisit when |
|---|---|---|---|
| 2026-08-04 | Preserve `CkDebug_SelectionSync` as the broadcast/lineage spine. | Its receiver guards and two-way owner-lineage match already solve nested entity mapping safely. | A debugger cannot express its entity mapping through lifetime ownership. |
| 2026-08-04 | Add a separate entity-target route registry rather than callbacks on launcher descriptors. | Keeps the launcher catalog plain and makes unload-safe entity targeting independently testable. | Never; this preserves the documented launcher boundary. |
| 2026-08-04 | Relink all existing ECS tree nodes on observed structural revision without recreating nodes. | Owner transfer changes hierarchy but not membership; O(n) relink occurs only on churn and preserves Slate identity. | Profiling shows churn relinking is material at target scale. |
| 2026-08-04 | Budget overlay rows/sections before Slate render. | Raw clipping is unrecoverable and scrolling is unavailable. | Overlay becomes interactive. |
| 2026-08-04 | Standardize a structural window frame, not feature action semantics. | GOAP, ECS, SM, and Scheduler have legitimately different control surfaces. | Multiple debuggers converge on an identical action set. |

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

## Editor acceptance matrix

| ID | Exact action | Expected observation | Status |
|---|---|---|---|
| `[EDITOR-VERIFY]` 1 | Start a representative NPC gym in PIE with GOAP, Crowd, State Machine, and A* entities. Open ECS plus those four dedicated debuggers. Double-Shift quick-select the NPC from the overlay. | ECS scrolls to the entity; every already-open compatible debugger adopts and reveals the closest lineage match without an echo loop. | Pending |
| `[EDITOR-VERIFY]` 2 | In the ECS inspector, select a create-style GOAP owner, then each Crowd/A*/State-Machine-capable owner or nested entity. Click each visible `Open In` button. | Only capable buttons appear. The existing tab opens/focuses and immediately selects its exact, ancestor, or descendant match. | Pending |
| `[EDITOR-VERIFY]` 3 | Select an ECS entity, then click the common `Use ECS <id>` status action in GOAP, Crowd, A*, and State Machine. Repeat with a nested feature entity and an unsupported sibling. | Exact matches win, otherwise the nearest ancestor/descendant wins deterministically; unrelated siblings are never selected; unsupported routes remain disabled/absent. | Pending |
| `[EDITOR-VERIFY]` 4 | Keep ECS open while spawning, destroying, and transferring a child from owner A to owner B. Do not press Refresh. | Spawned entities appear, destroyed entities disappear, and the same child row regroups under B at the next bounded refresh while surviving selection/expansion remains stable. | Pending |
| `[EDITOR-VERIFY]` 5 | Use the AI overlay preset on an attribute-heavy NPC and inspect GOAP/navigation failure and active-plan cases. | The card renders no more than 18 total rows and four per section; GOAP/Crowd/PathNetwork/A* sections outrank generic attributes; omission summaries are exact; GOAP and path rows describe actionable status/active/plan/cost/failure/corridor/goal data. | Pending |
| `[EDITOR-VERIFY]` 6 | Open ECS, State Machine, UI, Scheduler, A*, GOAP, Crowd, EQS, Input, Object Pooling, Jolt, Map, Dialog, and Aggro. Exercise the `Debuggers` menu and narrow-dock each tab. | All 14 tabs share the title/menu/content/status frame, retain feature controls, and stay reachable when narrow. Only registered entity-target tools show the ECS adoption action. | Pending |
| `[EDITOR-VERIFY]` 7 | Stop and restart PIE with the debugger tabs still open, then repeat one ECS adoption action. | No stale-handle teardown ensure/crash occurs, and the action uses the new world's current ECS selection. | Pending |

## Open items

| Item | Status | Next step |
|---|---|---|
| Gate 00 | Implementation verified; editor acceptance pending | Run matrix rows 1-4 and 7. |
| Gate 01 | Implementation verified; editor acceptance pending | Run matrix row 5. |
| Gate 02 | Implementation verified; editor acceptance pending | Run matrix rows 6-7. |

**Rule:** do not convert the editor-acceptance boundary into a completion claim while any matrix row remains pending.
