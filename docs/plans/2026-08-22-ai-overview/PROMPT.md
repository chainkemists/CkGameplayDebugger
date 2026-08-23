# CK AI Overview — mission brief

> **Written:** 2026-08-22. Stable content only; current state lives in [PROGRESS.md](PROGRESS.md).
> **This doc dies when:** the campaign ships and its permanent contracts have moved into module `CLAUDE.md` files.

## Goal

Ship one low-noise CK AI Overview debugger that explains the selected NPC’s decision-to-motion chain, exposes the most actionable cross-system evidence, and drills into the existing specialist debuggers. At the same time, make the debugger suite’s common right-side controls consistent, add host-authoritative multiplayer slow motion, support session-only gameplay recovery suppression, and make packaged picker failures diagnosable and fixable.

## Success criteria

1. Every standalone CK debugger uses one right-justified Common actions lane. The old textual Debuggers/Tools dropdown is absent; a compact diagnostics icon opens the CK Debugger Launcher.
2. Every entity-capable debugger places the shared Pick Entity control in that lane. The control uses the typed `SelectInViewport` cursor icon and preserves its settings popover.
3. Every debugger window shows the same world-speed control. `0.1x`, `0.25x`, `0.5x`, and `1x` mutate only an authority world; client-only windows explain why the control is unavailable. Listen-server changes replicate through `AWorldSettings::TimeDilation`.
4. A generic, session-only behavior-override registry and Common UI exist. BusterBlock contributes `Suppress NPC stuck recovery`; while enabled, the stuck processor emits no stuck write, stop/replan escalation, or terminal depart despawn, and disabling it starts a fresh timing episode.
5. `CK AI Overview` is a registered DeveloperTool debugger in Editor and packaged Development/DebugGame. It provides an NPC roster, selected-entity identity, concise AI/navigation stages, current evidence, spatial evidence, recent cross-system events, and targeted drill-down to GOAP, State Machine, and Crowd.
6. The overview reuses the On-Screen Overlay AI provider model and Crowd’s value-only viewport rather than duplicating their feature reads or retaining live world objects in presentation state.
7. Packaged picker activation either presents selectable entities or presents a precise empty-state reason. The reported missing-entity mechanism is reproduced in Development packaged behavior and removed without making all debugger modules Runtime.
8. Every new or encountered reusable UI element is implemented in `CkDebuggerCommon`. Any retained feature-local widget carries a written justification for why reuse is implausible.
9. Final Development Editor build and the same `Debugger` automation gate reproduce only the named inherited baseline failure. Focused Common, Launcher, picker, AI Overview, and NPC stuck-policy tests are green, and fresh logs contain no touched-file AngelScript warnings or new ensure/errors.

## Constraints and locked decisions

| Decision | Choice | Why |
|---|---|---|
| New integrated tool | `CkAiDebugger`, DeveloperTool | It needs its own roster, multi-panel layout, history, and packaged Development support. |
| Concise AI data | Reuse `CkEntityDebugOverlay` provider registry and AI layout | The overlay already owns the selected cross-system facts and priority ordering. |
| Spatial evidence | Reuse/export the existing Crowd value-only 3D viewport | It already owns copied navmesh, paths, agents, queues, selection, and teardown. |
| Common controls | `SCkDebug_WindowChrome` utility lane | One migration controls every debugger and keeps feature lanes separate. |
| Launcher access | Common tab-id/invoke path; no Common-to-Launcher dependency | Avoids a module cycle while keeping one typed route. |
| Slow motion | Authority-world `AWorldSettings::SetTimeDilation` | Engine source confirms clamping and replication of `TimeDilation`. |
| Behavior suppression | Generic Common registry; policy state owned by BusterBlock | Debugger owns presentation; gameplay owns gameplay behavior. |
| Picker diagnosis | Add explicit gather/empty statistics before mechanism changes | The current symptom has multiple distinguishable causes; no guessed fix. |
| Widget policy | Common by default; bespoke requires written proof | CTO directive for this campaign. |
| Build configuration | Development/Auto only | Shipping requires separate explicit approval. |

## Non-goals

- Replace the full GOAP, State Machine, Crowd, AStar, or ECS debuggers. The overview links to them.
- Move DeveloperTool debuggers to Runtime or support Test/Shipping debugger windows.
- Create an external debugger process or IPC transport.
- Generalize `CrowdAgent.DebugOverride` into a global behavior kill switch.
- Persist debug suppression across process restarts or saved games.
- Start a Shipping build.

## Reading list

- [PLAN.md](PLAN.md)
- `Source/CkDebuggerCommon/CLAUDE.md`
- `Source/CkCrowdDebugger/CLAUDE.md`
- `Source/CkGoapDebugger/CLAUDE.md`
- `Source/CkSmDebugger/CLAUDE.md`
- `Plugins/CkFoundation/CLAUDE.md`
- `Plugins/CkFoundation/Source/CLAUDE.md`
- `Plugins/CkFoundation/Script/CLAUDE.md`

## Things ruled out — do not re-investigate

| Ruled out | Why | Evidence |
|---|---|---|
| Per-window `slomo` console text | It can act in the wrong local world and has no explicit client rejection/readback contract. | No current CK API; `AWorldSettings` is the replicated authority-owned seam. |
| Client-local time dilation | It intentionally diverges simulation and will be server-corrected. | Common authority request-gate contract plus replicated WorldSettings source. |
| Moving ECS/GOAP/SM/Crowd debuggers to Runtime | Development packaged builds already include DeveloperTool modules; Test/Shipping exclusion is intentional. | `CkDebugger.uplugin` module types and packaging tests. |
| Reusing `CrowdAgent.DebugOverride` for global recovery suppression | It is a per-agent manual-control flag with different semantics and no global authority contract. | CkCrowd utils and BusterBlock stuck processor. |
| Reimplementing GOAP/SM/Crowd summary reads in the overview | The overlay providers already expose the desired concise facts. | `CkEntityDebugOverlay` provider registry and AI layout. |
