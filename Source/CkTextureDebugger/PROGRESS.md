# Texture & Surface Debugger — PROGRESS.md

## Current state

**As of 2026-08-22 (CkGameplayDebugger `e8d7bf38412668b5eab3343c87e7f5b50a131c0d`):** Gates 0–3 are implemented; final Development Editor/Game compile, full regression tests, cook, staged package, and packaged command/window/asset boot are green.
**Baseline being diffed against:** fresh-discovery serial `Debugger` cohort, 239 passed / 0 failed / 0 skipped / 0 contaminated, captured 2026-08-22.
**Next action:** complete live Editor/PIE and visible packaged apply/restore acceptance.
**Blocked on:** nothing.

## Decision log

| Date | Decision | Why | Revisit when |
|---|---|---|---|
| 2026-08-22 | New `CkTextureDebugger` DeveloperTool | Matches standalone packaged debugger architecture. | Only if Shipping support is explicitly requested. |
| 2026-08-22 | Add a generic Common component picker | ECS picker cannot return arbitrary primitive/material context. | If engine hit-proxy APIs provide a simpler packaged-safe shared path. |
| 2026-08-22 | Full override topology ledger with atomic apply and conflict-preserving restore | Slot-only snapshots can corrupt pre-existing MID/override state; blind restore clobbers later changes; all-or-nothing refusal can leave checker materials live. | Never without equivalent correctness proof. |
| 2026-08-22 | Track narrowly unignored source PNGs | Team reimport must not point at local `F:` fixtures. | If repository-wide source-art policy changes. |
| 2026-08-22 | No per-instance foliage checker in v1 | Material override is component-wide for ISM/HISM/foliage. | If a proven per-instance custom-data material path is designed. |
| 2026-08-22 | Pre-save restore/post-save reapply in editor worlds | Direct `SetMaterial` changes serialized override state and an unrelated save could persist the checker. | If a nonserialized render-proxy overlay replaces direct editor-world mutation. |
| 2026-08-22 | Reject components with per-slot overlay materials | Public restore APIs cannot reconstruct those overlays after `EmptyOverrideMaterials`. | If Engine exposes a supported setter/reconstruction API. |

## Dated entries

### 2026-08-22 — research and baseline

- Ran incremental Development Editor build: succeeded.
- First parallel `Debugger` test run executed 239/239 successfully but exposed one stale cached test name; rejected as an incomplete baseline.
- Ran fresh-discovery serial `Debugger` cohort: 239 passed, 0 failed, 0 skipped, 0 contaminated in 37 seconds.
- Confirmed current engine source at `D:\Repos\UnrealEngineAngelscript`, UE 5.7.4.
- Confirmed CkGameplayDebugger was content-clean but detached at `e8d7bf3`; root gitlink drift and all other root dirt predate this campaign.
- Confirmed module exemplar: CkSmDebugger for lifecycle/picker, CkOptimizationDebugger for audit/streaming facts, CkDebugScene for cook rules.
- Confirmed exact fixture hashes and deterministic semantic names from the reviewed images.
- Inferred pending compile proof: `GetStreamableResourceState`, component material APIs, and cook delegate signatures match current engine source; final authority is the actual module build.

### 2026-08-22 — adversarial plan review

- Blocked direct implementation until editor-save serialization, slot-overlay destruction, shader permutations, asset materialization/cook, concrete packaged target inclusion, streaming provenance, active material variants, and UV capability criteria were made explicit.
- Corrections are being applied to the gate contracts before re-review; no source implementation began against the rejected plan.
- Re-review results: module architecture GREEN-LIGHT; runtime/lifetime GREEN-LIGHT; asset/cook GREEN-LIGHT after the literal source-art exception and byte-identical copies were verified.

### 2026-08-22 — implementation and packaged Development gate

- Added the standalone `CkTextureDebugger` DeveloperTool module and launcher entry, using common window chrome, world selector, tabs, toggles, refresh gating, and a reusable primitive-component viewport picker.
- Added five renamed checker textures, a generated unlit/two-sided master material with skeletal/instanced/Nanite usage, deterministic source-art validation, UFS staging, and an explicit ModifyCook catalog.
- Added transactional component/slot checker application, exact topology snapshots, overlay fail-closed admission, editor pre-save suspension, teardown release, and conflict-preserving restore.
- Added Texture Health, UV & Density, Material Inputs, Surface & Lighting, and Loaded-world Scene Audit pages with explicit unavailable/provenance states.
- Final Editor build succeeded. The exact fresh serial `Debugger` cohort grew from the 239/239 baseline to 252/252: the expected 13 new tests, 0 failed, 0 skipped, 0 contaminated (`Saved/Logs/TextureDebugger-Final-Debugger3-20260822.log`).
- Final Development Game build/cook/stage/package succeeded. The cook admitted all six packages through `ModifyCookDelegate: CkTextureDebugger`, built the checker SM5/SM6 shader maps, and staged both shared glow brushes.
- The exact final staged binary mounted its containers, listed and executed `ck.TextureDebugger 1`, stayed responsive, logged window/tab construction, and loaded 5/5 checker textures plus the master material with no TextureDebugger error (`Saved/Logs/TextureDebugger-PackagedBoot-Final2-20260822.log`).
- Packaged boot also exposed two unrelated existing issues: pre-command ZenLoader/CkEnsure output during AngelScript refresh and missing common glow PNG staging. The glow staging gap is corrected in this working set; the pre-command framework issue remains outside this feature.

## Open items

| Item | Status | Next step |
|---|---|---|
| Gates 0–3 implementation | Complete | Keep their focused tests green through the final rebuild. |
| Gate 4 automated package evidence | Complete | Final Development package, staged resources, command, window, and runtime asset loads are proven. |
| Final regression gate | Complete | Baseline 239/239 became 252/252 solely through the expected 13 new tests; the failure set remains empty. |
| `[EDITOR-VERIFY]` | Open | Open with `ck.TextureDebugger 1`; pick a mesh in the ordinary Level Editor; apply each checker; save while active; confirm no checker is serialized. |
| `[PIE-VERIFY]` | Open | Repeat picker/apply/restore possessed and ejected; confirm End PIE leaves no checker material or stale selection. |
| `[PACKAGED-VERIFY]` visual | Open | In the staged Development executable, open the tool, pick a mesh, inspect all six pages, apply/restore selected and loaded-world scopes. |

**Rule: no completion claim may be written anywhere in this file while any row here is unresolved.**
