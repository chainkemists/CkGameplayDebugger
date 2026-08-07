# Debugger QoL campaign - mission brief

> **Written:** 2026-08-04. Stable content only; current state lives in [PROGRESS.md](PROGRESS.md).
> **This doc dies when:** the campaign ships and its durable contracts move into module `CLAUDE.md` files.

## Goal

Make entity debugging continuous across the CK debugger suite: the on-screen overlay stays readable and prioritizes GOAP/navigation failures, the ECS tree remains structurally truthful without manual refreshes, and every entity-aware debugger can adopt or receive an ECS selection through hierarchy-aware common infrastructure.

## Success criteria

1. The overlay focus card cannot be monopolized by high-volume providers. It retains prioritized AI/navigation sections and renders explicit overflow summaries instead of silently clipping omitted data.
2. GOAP and PathNetworkFollower overlay sections show real active-plan/action and route progress/failure context using runtime-safe CkFoundation APIs.
3. ECS lifetime-owner transfers regroup automatically within the existing refresh interval while preserving row identity, selection, and expansion. Destroyed entities disappear without a manual full refresh.
4. ECS tree selection, viewport picking, and overlay quick-select update every already-open entity debugger that can resolve the selected lineage, without echo loops.
5. A common `Sync from ECS` control targets the current debugger from the ECS primary selection and resolves nested/owned entities through the destination debugger's route.
6. The ECS inspector panel exposes common `Open In` actions for every registered entity-targetable dedicated debugger. Gate-0 destinations are GOAP, Crowd, State Machine, A*, and Aggro.
7. Every CkGameplayDebugger-owned standalone debugger window uses the shared top-chrome/content/status-frame contract while retaining its feature-specific controls and layout.
8. The final Development editor build and focused `Debugger` plus `DebugOverlay` suites have no failing-set regression from the recorded baseline; editor-only behavior has an exact manual verification checklist.
9. The Insights Analyzer editor tab is owned by CkGameplayDebugger, uses the shared debugger chrome and icon-toggle language, and retains its existing tab ID and commandlet compatibility while CkFoundation keeps only UI-free analysis/reporting code.
10. Every launcher debugger exposes at least one useful boolean action as a shared icon toggle in the common menu bar; feature modules construct no raw checkbox controls, including ECS contextual filters.

## Constraints and locked decisions

| Decision | Choice | Why |
|---|---|---|
| Cross-module ownership | Registries and widgets live in `CkDebuggerCommon`; feature modules register callbacks | Avoid sibling debugger dependencies and preserve module unload safety. |
| Entity matching | Reuse `ck::DebugSelectionSync::Is_SameLineage` | It already handles ancestor/descendant pairs, rejects cross-registry handles, and avoids shared-root false matches. |
| ECS current selection | One-way provider callback registered by `CkEcsDebugger` | Common never owns a PIE handle or depends on ECS debugger models. |
| Targeted debugger jumps | Generation-token route registry with `CanTarget` and `OpenAndTarget` callbacks | Supports closed-tab open/focus plus exact feature-owned entity resolution without reverse dependencies. |
| Inspector affordance | One common `Open In` strip at inspector-panel level | GOAP and Crowd currently have no native ECS inspector family; a generic strip still appears only for routes that accept the selected entity. |
| Overlay overflow | Budget the model before rendering and summarize omissions | The overlay is hit-test-invisible, so scrolling cannot be operated; raw Slate clipping hides the most useful state. |
| AI priority | GOAP, Crowd, A*, and PathNetworkFollower outrank generic attribute volume | AI/navigation triage is the overlay's primary operational job. |
| Window consistency | Shared structural chrome with named top, content, and status slots | Standardizes placement and styling without flattening GOAP/ECS/SM specialized controls. |
| Insights ownership (revised 2026-08-05) | CkFoundation keeps trace analysis, reports, and the commandlet; `CkInsightsDebugger` owns the Slate tab and launcher descriptor | Restores the debug-data/UI boundary without introducing a Foundation-to-debugger dependency. This supersedes the original external-proxy exception. |
| Toggle presentation (revised 2026-08-05) | Common chrome owns an inline menu-action slot; Common owns icon toggles and rich toggle surfaces; short exclusive choices use the engine segmented control | Makes every debugger's high-frequency options one-click and visually consistent without promoting contextual ECS filters into global state. |

## Non-goals

- Do not modify the legacy UE GameplayDebugger generation.
- Do not redesign feature data collection or interaction models beyond small presentation filters backed by already-collected debugger state.
- Do not add runtime gameplay state to CkGameplayDebugger or make CkFoundation depend on debugger modules.
- Do not change or suppress inherited SQLite, AngelScript-shadowing, cvar-discovery, or third-party build warnings as part of this campaign.
- Do not publish, commit, or stage unrelated CkFoundation campaign files.
- Do not rename the `CkInsightsAnalyzerTab` tab ID or the `CkInsightsAnalyzer` commandlet/module contract.

## Reading list

- [PLAN.md](PLAN.md) and the three gate contracts under `Plan/`.
- `Source/CkDebuggerCommon/CLAUDE.md` for shared widget, selection, list-row, and lifecycle contracts.
- `Source/CkEcsDebugger/CLAUDE.md` for tree/model/inspector ownership.
- `Source/CkGoapDebugger/CLAUDE.md` and `Source/CkSmDebugger/CLAUDE.md` for feature-specific selection and teardown.
- Neighboring patterns: `SCkGoapDebuggerWindow::OpenForEntity`, `CkDebug_Navigator`, `CkDebug_SelectionSync`, and ObjectPooling/Map window footers.

## Things ruled out - do not re-investigate

| Ruled out | Why | Evidence |
|---|---|---|
| Periodically force-full-refresh the ECS tree | It would discard stable row identity and hide the actual missing hierarchy relink. | Incremental refresh sees the revision but applies an empty membership diff after owner transfer. |
| Match debugger entities by top-level root | Unrelated gym NPCs can share a station root. | Existing selection-sync contract deliberately uses lineage containment. |
| Add a scrollbar to the overlay | The root is `HitTestInvisible`; users cannot interact with it. | Overlay root/focus-card construction and settings contract. |
| Put entity-target callbacks in the launcher descriptor | The launcher registry is intentionally plain tab metadata. | `CkDebuggerToolRegistry` permanent launcher contract. |
| Make ECS debugger depend on GOAP/Crowd/SM modules | This creates sibling coupling and unsafe module lifetime assumptions. | Existing feature-owned GOAP inspector gateway registration pattern. |
