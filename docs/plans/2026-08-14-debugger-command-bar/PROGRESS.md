# Debugger command bar campaign - PROGRESS.md

## Current state

**As of 2026-08-14:** the shared command bar and all 18 standalone-window migrations are implemented. Live editor feedback exposed nested icon/group wrapping, a redundant command-bar identity, and a latent Jolt `SScrollBox` slot-attribute ensure; all three causes are corrected. The Editor build and automated gates are green, with the visual recheck pending.

**Baseline:** Editor build succeeded; `Debugger` passed 108/108, failed 0, skipped 0, contaminated 0. See [baseline_20260814-030712.md](baseline_20260814-030712.md).

**Final automated gate:** after rebasing onto `origin/dev`, the generated Editor build and full `Debugger` cohort in `Saved/Logs/DebuggerCommandBar-PostRebase.log` succeeded with 114/114 passed, failed 0, skipped 0, contaminated 0. The 108-test baseline gained five command-bar/icon-toolbar contract tests plus one Jolt window-construction regression without adding a failure. The focused Jolt red/green evidence is `Saved/Logs/JoltSlotAttributeEnsure-Repro.log` (exact ensure reproduced) and `Saved/Logs/JoltSlotAttributeEnsure-Fixed.log` (1/1 passed with no failure pattern); the earlier focused common gate remains 40/40 in `Saved/Logs/DebuggerCommandBar-NoWrap-Focused-04.log`.

**Next action:** reopen the debuggers and recheck the corrected tab-only identity and single-line lanes at normal and narrow dock widths.

**Blocked on:** no external blocker. Live Slate review is deliberately deferred to `[EDITOR-VERIFY]` after compilation and automation.

## Decision log

| Date | Decision | Why | Revisit when |
|---|---|---|---|
| 2026-08-14 | Use arbitrary-widget semantic group descriptors rather than a universal command-action union. | Existing selectors, pickers, transport controls, and popovers have feature-owned state and mature Slate behavior; embedding them intact preserves ownership and avoids a second UI framework. | Several modules independently duplicate the same complete command type and behavior. |
| 2026-08-14 | Keep direct icon actions in `SCkDebug_IconToolbar`. | Its descriptors, live state attributes, tooltips, accessibility text, and atomic validation already solve that narrower problem. | Never as part of this campaign. |
| 2026-08-14 | Keep primary and context lanes on one horizontal line each, with horizontal scrolling under width pressure. | Live editor evidence showed both the outer group lane and inner icon toolbar wrapping; two nested wrap points created multi-row icon grids. | A dedicated, discoverable overflow affordance replaces scrolling. |
| 2026-08-14 | Preserve refresh opt-in per window. | Seven windows intentionally have no refresh widget; inventing refresh behavior would exceed a presentation migration. | A separate product decision standardizes refresh semantics. |
| 2026-08-14 | Make the dock tab the sole debugger identity surface. | Live editor feedback confirmed that repeating the debugger title and icon in Chrome adds no information. | Never for standalone dock tabs. |
| 2026-08-14 | Put status, ECS sync, refresh, and Tools in one trailing utility cluster. | This matches the approved hierarchy and removes the second chrome/status strip without changing the owned controls. | A measured narrow-dock review requires a different status width cap. |

## Census

- Registered standalone windows using `SCkDebug_WindowChrome`: 18/18.
- Direct semantic command-group windows: 18/18.
- Remaining legacy `MenuActionsContent` or `ToolbarContent` consumers: 0.
- Command-bar debugger title/identity inputs remaining: 0.
- Feature-local refresh widgets remaining in active standalone windows: 0.
- Windows with existing refresh controls: A*, ECS, EQS, Input, Intent, Jolt, Map, Object Pooling, Scheduler, State Machine, UI.
- Windows without refresh controls: Aggro, Crowd, Dialog, GOAP, Insights, Save, Style Lab.
- Removed duplicate feature identities: Crowd, EQS, Scheduler; the shared command-bar identity was also removed from all 18 windows.

## Open items

| Item | Status | Next step |
|---|---|---|
| Gate 00 | Complete | Editor build succeeded; `CommandBar` passed 3/3 with zero failures or contamination in `Saved/Logs/DebuggerCommandBar-Gate00.log`. |
| Gate 01 | Complete | All 14 standard-density windows use direct groups; module compilation is green. |
| Gate 02 | Complete | Crowd, State Machine, GOAP, and Insights use direct groups; module compilation is green. |
| Gate 03 | Automated complete / editor recheck pending | 18/18 census, diff hygiene, post-rebase generated Editor build, focused common 40/40, focused Jolt 1/1, and broad 114/114 are green. The corrected layout still requires live visual confirmation. |

## Editor acceptance matrix

| ID | Exact action | Expected observation | Status |
|---|---|---|---|
| `[EDITOR-VERIFY]` 1 | Open every launcher debugger and compare the top area at a normal dock width. | The dock tab is the sole debugger identity; controls are separated into visible semantic groups and the Tools switcher is stable at the right edge. | Recheck pending after live feedback fix |
| `[EDITOR-VERIFY]` 2 | Narrow-dock every debugger to approximately the approved mockup's 860 px desktop target. | Primary and context lanes remain single-line; individual icons never form a grid, and any overflow scrolls horizontally without obscuring trailing utilities. | Recheck pending after live feedback fix |
| `[EDITOR-VERIFY]` 3 | Exercise every migrated button, toggle, selector, popover, stepper, playback control, refresh selector, and the Tools switcher. | Behavior and live state match the pre-migration window; disabled states, tooltips, and accessible names remain truthful. | Pending |
| `[EDITOR-VERIFY]` 4 | Inspect Crowd, EQS, Scheduler, Map, Jolt, and GOAP specifically. | Redundant/static product labels are gone; Map's dynamic world status remains; alignment no longer depends on ad hoc fill spacers. | Pending |

**Rule:** do not convert pending editor rows into an automated completion claim.
