# Phase 3 — Position planner

> **Status:** ⏳ Pending
> **Depends on:** Phase 2 ✅
> **Estimate:** 1 session
> **Change class:** 2

## Goal

After this phase: given a loaded world and a request, `CkPerfLab` deterministically produces the
ordered position+direction plan — navmesh-seeded where a navmesh exists, bounds-grid fallback where
not, multi-yaw per mode preset — and the pure parts are spec-tested with synthetic inputs.

## Entry criteria

- [ ] Phase 0 addendum's navmesh/bounds API confirmations at hand.
- [ ] Read `FCk_Eqs_Algorithm::DoGenerate_{SimpleGrid,Grid}` + `DoRunTests`
      (`Plugins/CkFoundation/Source/CkEqs/Public/CkEqs/Query/CkEqs_Algorithm.h`) — the shapes to
      mimic. **Entry decision (record in PROGRESS.md decision log):** depend on CkEqs vs mirror the
      grid math locally. Default: mirror locally (the planner needs no entity context and CkEqs's
      fragment plumbing is dead weight here); flip only if the mirrored code exceeds ~150 lines.
- [ ] Branch `perflab/phase-3`; baseline `Ck.PerfLab` counts from Phase 2 exit.

## Work items

### 3.1 Pure planning core (`Public/CkPerfLab/Planner/CkPerfLab_Planner.h/.cpp`)

Split world-reading from planning so the math is spec-testable without a world:

- `FCk_PerfLab_WorldSurvey` — plain-data input: navmesh point set (possibly empty), actor bounds
  list (location + box + coarse cost proxies), world bounds. Gathered by 3.2; synthesised by specs.
- `FCk_PerfLab_Planner::Generate(const FCk_PerfLab_WorldSurvey&, const FCk_PerfLab_Request&)
  -> FCk_PerfLab_Plan`:
  1. Seed set: navmesh points when present (grid-decimated to spacing), else bounds-grid cells that
     contain ≥1 actor within radius R (reject empty space).
  2. Content weighting: rank cells by summed cost proxies; keep top `positionBudget` with a minimum
     mutual spacing (greedy farthest-point style — deterministic given the seed).
  3. Eye offset + per-position yaw set from the mode preset (Quick 1 forward-facing-at-content,
     Standard 4, Deep 16 — evenly spaced, starting yaw derived from seed).
  4. Deterministic order: sort by cell key with final tie-break on position id. Same survey + same
     request ⇒ byte-identical plan (spec-pinned).
- Position ids: stable hash of quantised location (so re-runs of the same map match ids — the
  compare-view contract, D-compare in SCHEMA.md).

### 3.2 World survey gather (`Private/CkPerfLab/Planner/CkPerfLab_WorldSurvey_Builder.{h,cpp}`)

- Runs in the child (game world): `UNavigationSystemV1` sampling per Phase 0 addendum;
  `TActorIterator` bounds + cost proxies (component counts, tri counts where registry-cheap,
  light/Niagara/tick flags — read live components, this is the child, not the editor scan).
- No ensure-spam on maps without navmesh — absence is a legal branch, not an error
  (`TryGet` contract).

### 3.3 Specs

1. `Ck.PerfLab.Planner.Determinism` — synthetic survey → two `Generate` calls byte-equal; permuted
   input actor order → same plan (sort discipline pin).
2. `Ck.PerfLab.Planner.NavmeshSeeded` / `.GridFallback` — presence/absence of navmesh points selects
   the branch; empty-world survey → empty plan + typed reason (never a crash).
3. `Ck.PerfLab.Planner.Spacing` — no two positions closer than the preset spacing.
4. `Ck.PerfLab.Planner.StableIds` — same location quantises to the same id across runs; a moved
   actor changes nothing about ids of unmoved positions.
5. `Ck.PerfLab.Planner.ModePresets` — Quick/Standard/Deep produce 1/4/16 directions.

## Expected observations — branches

| I will run | I expect | If instead | Response |
|---|---|---|---|
| `--test-pattern Ck.PerfLab` | Phase-2 greens + new planner specs green | Determinism spec flaky | You have hidden iteration-order dependence (TMap walk?) — sort before emit; never accept "usually equal" |
| Manual read of `Generate` | No world/UObject access in the pure core | It crept in | Move it to the survey builder — the split IS the testability |

## Exit criteria — same commit

- [ ] Planner specs green, counts recorded vs baseline.
- [ ] `CkPerfLab/CLAUDE.md` gains §Planner (determinism contract, id stability contract, the
      CkEqs-mirror decision + why).
- [ ] PLAN.md row + Status header + PROGRESS.md entry.

## Fences

- No `FMath::RandRange`/`FRandomStream` outside the seeded stream carried in the request; no
  wall-clock anywhere in planning (determinism).
- World Partition cell APIs: out of scope v1 (PROMPT.md non-goal) — do not add "just a little" WP.
