# Gate 0 — Contracts, picker, assets, and transaction core

> **Status:** 🟡 In progress
> **Depends on:** baseline captured

## Goal

After this gate, the new module registers in the launcher, exact checker assets can be generated and resolved, any playable-world primitive can be picked as a component target, and checker material application/restoration is atomic, conflict-aware, GC-safe, and test-covered before the full UI depends on it.

## Entry criteria

- [x] Fresh baseline recorded at selected HEAD.
- [x] Named exemplars and exact engine APIs inspected.
- [x] Independent module/runtime/asset research reconciled.
- [x] Three independent adversarial re-reviews returned GREEN-LIGHT after all blockers were resolved.

## Work items

1. Add module descriptor, Build.cs, module lifecycle, launcher descriptor, console command, cook delegate, and registration tests.
2. Add Common component-picker data contract, input/lifecycle implementation, analytic-bounds fallback, and controls.
3. Add value snapshot types and collector for loaded primitive/material/texture state.
4. Add checker override session with weak component/world links, strong original material roots, full topology snapshots, atomic apply/verify/rollback, conflict-preserving restore, editor pre-save restore/post-save reapply, and conflict reporting.
5. Reject checker admission for any component with non-empty per-slot overlay materials; expose the unsupported reason in diagnostics.
6. Narrowly unignore/copy semantic source art; add an entirely `#if WITH_EDITOR` deterministic asset generator for five textures and one unlit/two-sided checker material with skeletal, instanced-static-mesh, and Nanite usage flags. Keep every import/AssetTools/MaterialEditing dependency editor-gated in Build.cs and out of public runtime headers.
7. Commit generated assets as the source of truth. Bootstrap without pre-existing generated assets, verify source hashes and exact package names, then add both per-file UFS `RuntimeDependencies` and `ModifyCook/AddToCook` rules for all six packages.
8. Add focused tests for transaction topology, GC, conflict preservation, pre-save behavior, failure rollback, component overlays, component destruction, picker mapping, material usage support, descriptor packaging, and asset resolution.

## Expected observations

| Run | Expected | If instead | Response |
|---|---|---|---|
| Module build + launcher tests | Descriptor/spawner present; Development/DebugGame included; Test/Shipping excluded | Editor-only dependency leaks or descriptor absent | Stop and repair module boundary before UI work. |
| Apply two components with existing overrides/MIDs | Both receive checker; originals remain alive through GC; unchanged slots restore identity-equivalent through public APIs | Partial apply, lost MID, or wrong override topology | Reject design; do not add UI. |
| Change one material after apply, then restore | Externally changed slot remains; checker-owned slots restore; conflict is reported | External material overwritten or checker remains | Treat as gate blocker. |
| Component has per-slot overlay material | Whole component is rejected before any mutation with explicit unsupported reason | Apply clears or changes overlay | Treat as gate blocker. |
| Save editor world while checker is active | Checker restores before serialization; save contains originals/external changes; session re-applies after save | Checker path appears in saved package or session re-applies after failed preflight | Treat as gate blocker. |
| Destroy component/end world/EndPIE | Picker/session deactivates and drops weak entries without AV | Stale reference or partial state survives | Teardown blocker; audit every retained reference. |
| Generate/load assets from a state with no pre-existing outputs | Five committed textures and master material resolve at `/CkDebugger/TextureDebugger/...`; source hashes match; skeletal/ISM/Nanite usage checks pass; six RuntimeDependencies and six AddToCook rules exist | Missing package, local-only reimport path, unsupported permutation, or cook rule gap | Fix generator/cook path before Gate 1. |

## Exit criteria

- [ ] All expected observations above have focused automated evidence.
- [ ] Incremental Editor build succeeds after final Gate 0 edit.
- [ ] Focused `Ck.TextureDebugger` plus Common/Launcher tests pass.
- [ ] Invalid-input tests assert rejection, zero downstream mutation, and no partial state.
- [ ] `git check-ignore` proves precisely the five tracked source PNGs are unignored while other SourceArt PNGs remain ignored.
- [ ] Gate index, this status, PROGRESS, and permanent `CLAUDE.md` contracts updated together.
