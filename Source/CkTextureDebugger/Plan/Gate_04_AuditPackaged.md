# Gate 4 — Scene Audit and packaged acceptance

> **Status:** ⏳ Pending
> **Depends on:** Gate 3 ✅

## Goal

After this gate, the loaded-world audit is actionable, the complete tool works in Editor/PIE and packaged Development Game, all checker packages are cooked, and completion is proven against every mission criterion.

## Work items

1. Aggregate loaded-scope findings with stable rows, filter/highlight, severity, selection, copy, and focus actions.
2. Add scan progress/cancellation and loaded-cell caveat; never force-load World Partition.
3. Build Development Editor and the exact packaged Development Game target with developer tools enabled; verify the concrete target enables `bBuildDeveloperTools`, cook rules, staged assets, `CkTextureDebugger`/`CkDebuggerLauncher` binaries, and staged module manifest entries.
4. Run focused tests, the fresh full Debugger cohort, adversarial code review, and manual Editor/PIE/packaged workflows.
5. Update launcher/plugin/permanent docs and perform requirement-by-requirement completion audit.

## Expected observations

- Loaded-world audit counts match the same underlying collector used by pages and picker.
- The exact staged Development executable opens `ck.TextureDebugger 1`, its launcher catalog contains the texture tool, every checker/master asset loads, picker targets a component, streaming capability is reported, and checker apply/restore is visible.
- Final fresh `Debugger` cohort matches baseline: empty failing set.
- Game target links with no UnrealEd/AssetRegistry/TargetPlatform leakage.

## Exit criteria

- [ ] Every PROMPT success criterion has authoritative evidence.
- [ ] Editor and Development Game build exit codes are zero.
- [ ] Cook/stage log contains all six required asset packages and no missing-package warnings.
- [ ] Staged module manifest and binaries prove DeveloperTool inclusion for the exact executable that was launched.
- [ ] `[EDITOR-VERIFY]`, `[PIE-VERIFY]`, and `[PACKAGED-VERIFY]` are complete.
- [ ] Adversarial review blockers resolved and re-reviewed.
- [ ] Campaign docs are deleted or tombstoned after permanent `CLAUDE.md` absorbs the surviving contracts.
