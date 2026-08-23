# CkTextureDebugger

`CkTextureDebugger` is the packaged-capable Gen-2 Texture & Surface Debugger. It is a `DeveloperTool`: available in Editor and Development/DebugGame game targets, excluded from Test/Shipping. Open it with `ck.TextureDebugger 1` or through the debugger launcher.

## Product contract

- Operate directly on the selected active Editor, PIE, or Game world. Never create a preview or QA world and never load World Partition cells.
- Checker scope is either explicit slots on one selected component or every checker-capable component already loaded in the active world.
- ISM, HISM, and foliage overrides are component-wide. A confirmation bound to the exact world/scope/component/slot target set is mandatory.
- Apply is atomic. Snapshot the full override topology, strongly root original materials, and keep component/world references weak.
- Restore only values still owned by the checker; preserve later external material changes and report conflicts.
- Reject components with per-slot overlay materials because public engine APIs cannot reconstruct that overlay topology safely.
- In Editor worlds, remove checker values before save and reapply only after a successful save and fresh preflight. During world cleanup, release without mutating teardown state.

## Assets and cook

The five checker textures and `M_CkTextureChecker` live under `/CkDebugger/TextureDebugger`. `CkTextureDebugger.Build.cs` stages all six as UFS, and the module's editor-only `ModifyCook` callback adds the exact package catalog. The source PNGs are narrowly unignored under `Content/TextureDebugger/SourceArt` so reimport never depends on a developer's local drive.

The master material is unlit, two-sided, and authored for skeletal meshes, instanced static meshes, and Nanite. Runtime code creates a transient MID and sets only the `CheckerTexture` parameter; it never fabricates or compiles a material at runtime.

## Analysis rules

- Texture Health reports cooked dimensions, mip/streaming state, requested/resident gaps, and explicit unavailable states. Never call `GetPlatformData()` in the runtime scanner.
- Material Inputs resolves active-quality/platform parameter values and labels non-parameter used-texture references as potential rather than resolved.
- UV density emits a number only when world-triangle area, UV-triangle area, texture binding, texture transform, and cooked dimensions are authoritative. Otherwise report the missing capability.
- Surface & Lighting reports material/component facts available through public runtime APIs; do not infer unsupported shadow or lighting conclusions.

## Verification

Focused tests use `Ck.TextureDebugger` plus `Ck.DebuggerCommon.ViewportComponentPicker`. The final regression gate is the fresh, serial `Debugger` cohort recorded in `Plan/Baseline_20260822-025007.md`. Packaged acceptance must verify the six cooked packages, the `ck.TextureDebugger` command, window construction, runtime asset loads, picker behavior, and visible apply/restore in a Development package.
