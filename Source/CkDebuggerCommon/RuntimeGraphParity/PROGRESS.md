# Runtime graph parity — progress

## Current state

**As of 2026-08-10:** the shared canvas and all four debugger adapters are implemented and partitioned into reviewable commits. Static boundary and whitespace checks are complete; the build-machine gate remains pending by CTO instruction.

**Baseline being diffed against:** ECS omitted its Graph page, Scheduler showed a packaged fallback message, GOAP omitted its Plan graph tab, and State Machine substituted a three-list packaged window. The new adapters use the same runtime Slate surfaces in Editor and packaged builds. The unrelated untracked `CONTINUATION_PROMPT_DebuggerUsability.md` remains excluded; no local build was run because the BM remains the packaged-build authority.

**Next action:** publish the debugger submodule and root gitlink, then use the BM Development package for compile and visual-interaction acceptance.

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

## Open items

| Item | Status | Next step |
|---|---|---|
| Shared graph viewport | Implemented, pending BM | Exercise stable slots, transforms, culling, edge geometry, selection, pan/zoom, and frame-all in the package. |
| ECS parity | Implemented, pending BM | Compare relationship topology, layout, card state, selection, drag, copy, and frame-all. |
| Scheduler parity | Implemented, pending BM | Compare the full/detail processor graphs, live timing state, selection, copy, and layout controls. |
| GOAP parity | Implemented, pending BM | Compare action/goal cards, plan/failure/tree edge styles, selection, name depth, and fit. |
| SM parity | Implemented, pending BM | Compare compound/state/transition rendering, scrub/live highlights, timeline, history, details, and breakpoint state. Editor-only preview/test authoring controls remain editor-only. |
| Visual acceptance | Pending | Capture matched Editor/package screenshots and run the interaction checklist on BM output. |
