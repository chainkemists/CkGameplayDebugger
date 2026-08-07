# Gate 04 - Complete toggle coverage

> **Status:** Review remediation implemented; post-rebase automation and renewed editor acceptance pending
> **Depends on:** Gate 03 implementation verified; editor acceptance remains pending
> **Estimate:** 2 days, entered 2026-08-05

## Goal

After this gate, every debugger in the launcher exposes at least one useful one-click icon action in the common menu bar, and no feature debugger directly constructs a user-facing checkbox. ECS contextual filters retain their local semantics through shared rich-toggle or segmented controls.

## Entry criteria

- [x] Current tips and dirty ownership recorded in `_scratch/baseline_debugger_toggle_coverage_20260805-183231.md`.
- [x] All 15 window roots and all feature-local `SCheckBox` sites inventoried with `rg --no-ignore`.
- [x] Neighboring patterns re-read: `SCkDebug_WindowChrome`, `SCkDebug_IconToggle`, `SCkDebug_IconToolbar`, and ECS's existing `SSegmentedControl`.
- [x] Fresh full baseline attempted and correctly withheld because another Toolbox/editor owns the active log; last broad 17/17 and current-tree launcher 2/2 evidence recorded without calling them a same-tree A/B run.

## Work items

1. Add a named inline menu-action slot to `SCkDebug_WindowChrome` and a shared content-bearing toggle surface for rich contextual controls.
2. Move existing ECS, SM, GOAP, Crowd, EQS, Scheduler, and Insights boolean actions into the common menu-action slot without moving state ownership.
3. Replace all ECS feature-local raw checkboxes: icon toggles for booleans, common rich toggle surfaces for tokens/cards/chips, and `SSegmentedControl` for All/Any plus 2-6 columns.
4. Add small state-backed presentation actions to Map, Dialog, A*, Aggro, Object Pooling, Jolt, UI, and Input. Default every new filter off so existing views remain unchanged.
5. Run the existing common pure icon-action validation and update permanent Common and module docs; the new named slot and rich surface add no new pure policy seam.

## Expected observations and branches

| Run | Expected observation | If instead | Response |
|---|---|---|---|
| Static source census | 15 chrome action-slot users; zero `SNew(SCheckBox)` outside Common widget implementations | A feature checkbox or actionless window remains | Keep the gate open and classify the missing control; do not hide it through grep exclusions. |
| Development Editor build | Every debugger module compiles against the shared controls with no new reverse dependency | Slate template, include, or delegate failure | Correct the common API or feature binding; do not restore bespoke checkbox wrappers. |
| Full `Debugger` suite | Empty failing set on the final binary | New failure | Isolate the exact module/test and compare against recorded evidence before changing behavior. |
| `[EDITOR-VERIFY]` all 15 windows | Every top bar keeps its title at the left and all menu actions visible immediately to the title's right; the action surface uses remaining width and wraps rather than clips when narrow; tooltips/state are legible; contextual filters retain their semantics | Shifted title, clipped/hidden action, wrong state, or filter mismatch | Keep the gate open and correct the common chrome/toolbar contract before feature-local mappings. |

## Exit criteria

- [x] Static census proves action coverage and feature-local raw-checkbox removal.
- [x] Final build and full `Debugger` suite evidence recorded with the baseline limitation stated accurately.
- [x] `ck-change-control` Class 3 checklist satisfied; final binary is newer than every source edit.
- [x] Editor-only checks list exact action/state expectations for all 15 tools and remain pending until observed.
- [x] `PLAN.md`, this status header, permanent docs, and `PROGRESS.md` updated together.
- [x] Final diff passes style/comment audit and independent adversarial review.

## Verification evidence

- Static census: 15 `SCkDebug_WindowChrome` roots, 15 `MenuActionsContent` users, and zero feature-local `SCheckBox` construction/include files. The only raw checkbox construction is internal to Common's `SCkDebug_IconToggle` and `SCkDebug_ToggleSurface`.
- Final UnrealToolbox Development Editor rebuild succeeded and produced binaries newer than the latest source in every touched debugger module: `Saved/Logs/DebuggerToggleCoverage-Final-20260805.log`.
- The full `Debugger` pattern passed 18/18, failed 0, skipped 0, contaminated 0 in the same final log. This is a green final gate, not a strict same-tree A/B comparison because the fresh pre-edit broad baseline was blocked as recorded at entry.
- `git diff --check` passed. Independent adversarial review found one Scheduler off-state mismatch; the handler now clears both historical-frame selection and frozen capture, and the final rebuild includes that correction.
- Initial live placement failed: the flexible right-aligned action slot triggered a three-action compact row plus `•••` overflow. The canonical toolbar now has one all-direct layout.
- The next live placement exposed that action-first ordering shifted each debugger title by its action count. The shared chrome now anchors the title first, places the complete direct action row to its right, and retains the right-edge Debuggers control.
- Replacement Development Editor build succeeded; `CkDebuggerCommon`, ECS, SM, Crowd, GOAP, and EQS target binaries are newer than their latest changed source.
- Clean post-build `Debugger` automation passed 18/18, failed 0, skipped 0, contaminated 0 in `Saved/Logs/DebuggerDirectMenuActions-TestOnly-20260805.log`; the log contains no ensure, script-error, automation-error, no-match, fatal, or failed-result markers.
- Final title-then-actions Development Editor build succeeded; `CkDebuggerCommon` is newer than the corrected chrome source. The full `Debugger` pattern passed 18/18, failed 0, skipped 0, contaminated 0 in 39s in `Saved/Logs/DebuggerTitleThenActions-Final-20260805.log`, with all fresh diagnostic scan counts at zero.
- Live verification then showed that the fill-width title still pushed actions to the far right. The final shared order is auto-width title, auto-width direct actions, fill-width spacer, and right-edge Debuggers.
- Final adjacent-actions Development Editor build succeeded; the full `Debugger` pattern passed 18/18, failed 0, skipped 0, contaminated 0 in 40s in `Saved/Logs/DebuggerAdjacentMenuActions-Final-20260805.log`. The rebuilt Common binary is newer than source and every fresh diagnostic scan count is zero.
- 2026-08-07 review found that an auto-width nine-action EQS row could exceed a narrow header and clip. The action surface now fills the width between title and Debuggers and uses a width-aware wrap box, retaining direct access without overflow.
- Four newer Crowd navigation controls introduced after the original census now use `SCkDebug_ToggleSurface`, restoring the zero-feature-local-`SCheckBox` invariant. Final post-rebase automation and renewed editor row 15 remain pending.
- Renewed live visual state and ECS contextual interaction remain `[EDITOR-VERIFY]` row 15.
