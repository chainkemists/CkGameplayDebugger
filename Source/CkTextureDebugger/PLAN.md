# Texture & Surface Debugger — campaign plan

> **Written:** 2026-08-22. Update this index and each gate status together.

## Status

| Gate | State | Observable exit |
|---|---|---|
| 0 — Contracts, picker, assets, transaction core | ✅ Complete | Module/launcher registers; assets generate/resolve; component picker and atomic apply/restore tests pass. |
| 1 — Checker artist workflow | ✅ Complete | Gallery, scope, explicit slots, component picker, atomic apply, save suspension, and conflict-preserving restore are implemented. |
| 2 — Rules, overrides, and texture health | ✅ Complete | Active rendered-material inputs and packaged-safe streaming facts report explicit provenance/unavailable states. |
| 3 — UV, material inputs, and surface context | ✅ Complete | UV capability fails closed without authoritative evidence; material and surface pages expose the supported runtime facts. |
| 4 — Scene audit and packaged acceptance | 🟡 In progress | Development package/cook/boot passed; final full Debugger cohort and live Editor/PIE visual acceptance remain. |

## Locked architecture

1. `CkTextureDebugger` owns product-specific data, configuration, override session, view-model, pages, and window.
2. `CkDebuggerCommon` owns a reusable primitive-component picker modeled on the existing ECS picker.
3. Data collection publishes copied value rows plus weak navigation targets; UI never retains raw UObject pointers.
4. Original materials and checker MIDs are strongly rooted only for the lifetime of an active override session.
5. Apply is a whole-batch transaction. Restore reconciles each live component against the expected checker topology, removes values still owned by the tool, preserves external changes, and reports conflicts.
6. Components with material-slot overlay state fail checker admission until a supported public reconstruction API exists.
7. Active editor-world sessions restore before every world save and reapply after save only after a fresh preflight; a crash cannot persist a checker that was never saved.
8. `CkOptimizationDebugger` remains the offline project-audit owner. This tool may share algorithms or facts but does not depend on it or copy editor-only claims into packaged builds.

## Verification spine

- Baseline: [Plan/Baseline_20260822-025007.md](Plan/Baseline_20260822-025007.md).
- After each gate: incremental Editor build plus `Ck.TextureDebugger` and relevant Common/Launcher tests.
- Before final: fresh-discovery serial `Debugger` cohort compared with the 239/239 baseline.
- Game target: incremental Development Game compile; no Shipping build.
- Visual/runtime behavior: exact `[EDITOR-VERIFY]`, `[PIE-VERIFY]`, and `[PACKAGED-VERIFY]` steps recorded in [PROGRESS.md](PROGRESS.md).
