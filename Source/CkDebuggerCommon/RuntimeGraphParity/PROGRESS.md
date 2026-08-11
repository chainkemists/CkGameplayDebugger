# Runtime graph parity — progress

## Current state

**As of 2026-08-10:** the shared canvas and all four debugger adapters are implemented. Development Editor and Win64 Development Game compile successfully through UnrealToolbox. Static boundary and whitespace checks are complete; manual side-by-side visual and interaction acceptance remains.

**Baseline being diffed against:** ECS omitted its Graph page, Scheduler showed a packaged fallback message, GOAP omitted its Plan graph tab, and State Machine substituted a three-list packaged window. The new adapters use the same runtime Slate surfaces in Editor and packaged builds. The State Machine Preview and Test controls now use the same value-owned runtime graph surface in both targets. The unrelated untracked `CONTINUATION_PROMPT_DebuggerUsability.md` remains excluded.

**Next action:** publish the debugger submodule and root gitlinks, then use the BM Development package for matched Editor/package visual-interaction acceptance.

**Blocked on:** nothing.

## Decision log

| Date | Decision | Why | Revisit when |
|---|---|---|---|
| 2026-08-10 | Use the shared runtime renderer in Editor and packaged builds. | Exact parity must be structural, not maintained by duplicated implementations. | Never, unless Unreal ships GraphEditor as a supported Game-target runtime module. |
| 2026-08-10 | Preserve debugger-specific layout algorithms behind a shared scene interface. | ECS, GOAP, and SM encode product behavior that a generic layout would lose. | If snapshot tests prove a shared algorithm produces identical positions. |
| 2026-08-10 | Deliver SM after the other three graph adapters. | It contains the superset of compound layout, transition badges, scrub coupling, breakpoints, and preview behavior. | If a shared-canvas requirement can only be proven through SM. |

## Dated entries

### 2026-08-10 — campaign started

- Confirmed: the four packaged fallbacks are narrower than the Editor graph surfaces by reading their window/page source.
- Confirmed: existing graph data collectors and most ViewModels are runtime-safe; GraphEditor is the rendering/authoring dependency boundary.
- Confirmed: `FCkDebugGraphLayout` is a runtime-safe shared layout primitive already used by Scheduler and SM.
- Inferred: a shared arbitrary-child Slate panel plus semantic edge painter can preserve all four card designs; Gate 0 tests and first adapter will confirm it.

### 2026-08-10 — compile gates complete

- Confirmed: Development Editor build succeeds through UnrealToolbox.
- Confirmed: Win64 Development Game build succeeds through UnrealToolbox with the DeveloperTool debugger modules and no GraphEditor or UnrealEd dependency closure.
- Confirmed: the packaged State Machine path compiles with runtime pause/resume, Preview class selection, transient topology walking, and Test data rendered through `SCkSmRuntimeGraph`.
- Authored deterministic canvas/model coverage compiled in both targets. Automation tests were not launched by explicit CTO instruction.

## Open items

| Item | Status | Next step |
|---|---|---|
| Shared graph viewport | Build verified | Exercise stable slots, transforms, culling, edge geometry, selection, pan/zoom, and frame-all in the package. |
| ECS parity | Build verified | Compare relationship topology, layout, card state, selection, drag, copy, and frame-all. |
| Scheduler parity | Build verified | Compare the detail processor graph, live timing state, selection, copy, and layout controls. |
| GOAP parity | Build verified | Compare action/goal cards, plan/failure/tree edge styles, selection, name depth, hide-dimmed, 1:1 reset, and fit. |
| SM parity | Build verified | Compare compound/state/transition rendering, scrub/live highlights, timeline, history, details, breakpoints, runtime pause, Preview, and Test mode. |
| Visual acceptance | Pending | Capture matched Editor/package screenshots and run the interaction checklist on BM output. |
