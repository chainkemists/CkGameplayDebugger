# Gate 01 - Overlay AI triage

> **Status:** Implementation verified; editor acceptance pending
> **Depends on:** Gate 00 done
> **Estimate:** 1-2 implementation days; re-date at entry.

## Goal

After this gate, the overlay remains readable under attribute-heavy entities and gives an actionable first diagnosis for GOAP planning and PathNetwork navigation state.

## Entry criteria

- [x] Gate 00 exit evidence re-run on current HEAD.
- [x] Current All/AI layouts and provider priority/field tags re-verified.
- [x] Available runtime-safe GOAP and PathNetworkFollower Utils re-verified; no editor debugger dependency introduced.

## Work items

1. Add a pure focus-card budgeting model with configurable total-row and per-section limits, stable priority ordering, and explicit provider/row overflow summaries.
   -> verify: low-priority attributes cannot evict higher-priority AI sections; equal-priority source order is stable.
2. Tune All/AI layout defaults and provider priorities so GOAP, Crowd, A*, and PathNetworkFollower remain visible before generic attribute families.
   -> verify: default-enabled fields remain compatible with existing config overrides and unknown provider tags are reported by layout validation.
3. Replace GOAP status/depth proxies with runtime goal/active-action/plan/cost state obtainable from `CkGoap` Utils; add bounded history only where the runtime API already supports it.
   -> verify: no-plan, planning, active, failed/fallback, and nested-action cases render distinct actionable rows.
4. Split PathNetworkFollower status, failure, progress/corridor, next target, and goal into independently selectable fields.
   -> verify: None/Pending/Ready/Failed cases emit exact field shapes and severity.
5. Add an A* overlay provider only if current runtime debug data exposes a cheap, safe provider gate and useful status without editor-module coupling.
   -> verify: provider registration/layout validation and invalid handles are safe; otherwise record the omission explicitly.

## Expected observations and branches

| Run | Expected | If instead | Response |
|---|---|---|---|
| Budget model specs | AI sections survive attribute flood; overflow counts exact | important section omitted | correct priority/budget model, not Slate clip height |
| Provider shape specs | each GOAP/nav state has unique fields/severity | Utils cannot name active item | keep verified runtime data and record a Foundation API follow-up; do not depend on editor debugger |
| `[EDITOR-VERIFY]` attribute-heavy NPC | full plate stays within budget and shows `+N` summaries | content still hard-clips without summary | capture actual desired sizes and fix budget accounting |
| `[EDITOR-VERIFY]` GOAP/nav failure | first card view names failure/active action/route context | diagnosis still requires blind tab hunting | promote missing field or add direct Open In affordance through Gate 00 route |

## Exit criteria

- [x] `DebugOverlay` focused suite passes with new budget/provider cases.
- [x] `Debugger` gate remains at baseline failing set.
- [x] All editor-only checks are exact and recorded.
- [x] Overlay docs/settings comments describe budget and priority semantics.
- [x] `PLAN.md`, this status header, and `PROGRESS.md` update together.
