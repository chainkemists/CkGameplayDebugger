# Texture & Surface Debugger — mission brief

> **Written:** 2026-08-22. STABLE content only — current state lives in [PROGRESS.md](PROGRESS.md).
> **This doc dies when:** the campaign ships and permanent contracts have moved to `CLAUDE.md`.

## Goal

Ship a CkGameplayDebugger standalone Texture & Surface Debugger that lets artists select one of five supplied checker textures, apply it non-destructively to real mesh components in the active Editor, PIE, or packaged game world, pick and inspect components/material slots, diagnose missing or blurry textures, measure UV density where data permits, inspect material/surface/lighting context, audit the loaded scene, and always restore original materials without dirtying a level or overwriting unrelated runtime changes.

## Success criteria

1. `[CK] Texture & Surface Debugger` opens from the CK Debugger Launcher and `ck.TextureDebugger [0/1]` in Editor and packaged Development/DebugGame.
2. The tool operates on the active Editor/PIE/game world; it never creates a preview or QA world.
3. The five supplied source images are renamed, copied under plugin content, imported as exact checker `UTexture2D` assets, shown in a gallery, and explicitly cooked with one authored checker master material.
4. Selected/focused, nearby, and loaded-world scopes are explicit. Loaded World never force-loads World Partition cells.
5. Static, skeletal, ISM, HISM, and foliage mesh components are scanned. Foliage/ISM replacement is clearly component-wide and reports the affected instance count.
6. Checker application is atomic. Any invalid target or failed verification rolls back the whole attempted batch.
7. Restore is conflict-preserving: slots still holding the checker return to their originals, while any material changed by gameplay/editor after apply is preserved and reported rather than clobbered. No checker override may be serialized by an editor world save.
8. Closing the tab, changing world/session, ending PIE, or shutting down releases/restores state safely; destroyed components are never dereferenced.
9. A runtime-safe component picker works in PIE and packaged game views, including collisionless bounds fallback; it selects a component/hit context and lets the user choose the material slot explicitly.
10. Ordered resolution-detection rules and project-owned per-material overrides explain which texture defines a material's resolution. Absence or ambiguity is reported as Unknown, never fabricated.
11. Texture Health distinguishes authored/cooked dimensions, requested/resident/wanted mips where available, streaming-disabled/unavailable states, fallback/missing textures, memory, format, LOD group, and editor-only authoring checks.
12. UV & Density reports true texels-per-world-unit only when authoritative mesh/UV data is available; unsupported cases say Unavailable rather than estimating.
13. Material Inputs exposes the textures that the current material instance actually resolves; Surface & Lighting exposes texture-related normal, opacity, shadow, lightmap, and shading context without claiming to be a complete lighting debugger.
14. Scene Audit aggregates actionable texture/surface findings for the loaded scope and routes every row through the shared selection/picker context.
15. The final Editor build, focused tests, full debugger cohort, Development Game build, cook/package asset presence, and manual Editor/PIE/packaged workflows are evidenced separately. Shipping is not built without explicit approval.

## Constraints and locked decisions

| Decision | Choice | Why |
|---|---|---|
| Module | New `CkTextureDebugger` `DeveloperTool` module | Matches Gen-2 Ck tools and packages in Development/DebugGame while remaining excluded from Test/Shipping. |
| World ownership | Direct active-world components only | A second world is expensive and contradicts the required workflow. |
| UI | `SCkDebug_WindowChrome`, common command bar/tabs/status/search/rows | Shared CK appearance and behavior are requirements. |
| Picker | New reusable component picker in `CkDebuggerCommon` | Existing picker is ECS-handle-specific; material inspection needs primitive/component hit context. |
| Stored UObject ownership | Weak world/component links; strong original material/MID roots only | No raw pointers; originals must survive GC while temporarily removed. |
| Checker rendering | One cooked unlit/two-sided master material with skeletal, instanced-mesh, and Nanite usage permutations plus tool-owned MIDs | A texture cannot be passed to `SetMaterial`; runtime material compilation is not packaged-safe. |
| Apply | Full override-topology snapshot, whole-batch preflight, mutation verification, rollback on failure | Admission is atomic; no successfully accepted subset may remain applied. |
| Restore | Conflict-preserving reconciliation through public APIs | Remove checker values that are still ours while preserving and reporting newer external material changes. |
| Editor saves | Restore before world save; reapply only after save succeeds and only if the session still preflights | `OverrideMaterials` is serialized even when the checker operation itself did not dirty the package. |
| Component overlays | Fail closed when `GetComponentMaterialSlotsOverlayMaterial()` is non-empty | `EmptyOverrideMaterials()` also clears per-slot overlays and no public setter can reconstruct them safely. |
| Source art | Track five narrowly unignored renamed PNGs beside imported assets | Reimport must not depend on one developer's `F:` drive. |
| Configuration | Project config owns rules/overrides; per-user runtime config owns UI preferences | Team detection policy should be shared; personal layout and checker choice should not dirty project config. |
| Packaged target | Development/DebugGame only under current debugger contract | Existing DeveloperTool policy excludes Test/Shipping; a Shipping build also requires explicit approval. |
| Editor-only data | Clearly labeled unavailable outside Editor | Source build settings, AssetRegistry navigation, and static authoring data must not leak into Game dependencies. |
| Health provenance | Authored=editor-only; Cooked=runtime asset; Requested/Resident=valid active streamer state; Unavailable states are explicit | Volatile streamer zeroes are not evidence. “Wanted” is not a separate promise unless an exact supported API proves it. |
| Material input provenance | Active quality/platform parameter values are `Parameter`; active-variant used-texture rows are `UsedTexture`; unprovable sampler/slot mappings are `Unavailable` | Default union enumeration can include inactive quality branches and must not be presented as rendered truth. |

## Fixture mapping

| Source file | Tracked source-art name | Imported asset |
|---|---|---|
| `CustomUVChecker_byValle_2K.png` | `CustomUVChecker_ColorGrid_2K.png` | `T_CkTextureChecker_ColorGrid_2K` |
| `CustomUVChecker_byValle_4K.png` | `CustomUVChecker_ColorGrid_4K.png` | `T_CkTextureChecker_ColorGrid_4K` |
| `CustomUVChecker_byValle_4K (1).png` | `CustomUVChecker_GoldGray_4K.png` | `T_CkTextureChecker_GoldGray_4K` |
| `CustomUVChecker_byValle_4K (2).png` | `CustomUVChecker_RoundedSpectrum_4K.png` | `T_CkTextureChecker_RoundedSpectrum_4K` |
| `CustomUVChecker_byValle_4K (3).png` | `CustomUVChecker_DirectionalMono_4K.png` | `T_CkTextureChecker_DirectionalMono_4K` |

## Non-goals

- Loading or scanning unloaded World Partition cells.
- Silent asset auto-fixes or automatic mutation of texture/material import settings.
- Per-instance material replacement for a shared ISM/HISM/foliage component.
- A complete Lumen, VSM, GPU, or lighting debugger; only texture/surface-facing context belongs here.
- Shipping inclusion or a Shipping build without an explicit new decision and approval.

## Reading list

- [PLAN.md](PLAN.md) and `Plan/Gate_*.md`.
- `../CkDebuggerCommon/CLAUDE.md`.
- `../CkSmDebugger/CkSmDebugger_Module.cpp` and its window picker lifecycle.
- `../CkOptimizationDebugger/Public/CkOptimizationDebugger/Analysis/` for audit/streaming precedents.
- `CkDebugScene` module cook rules in CkFoundation.

## Things ruled out — do not re-investigate

| Ruled out | Why | Evidence |
|---|---|---|
| Use the ECS picker unchanged | It returns `FCk_Handle` and only resolves ECS-ready actors; texture work needs primitive/component and hit metadata. | `CkDebug_ViewportPicker.h/.cpp` inspected 2026-08-22. |
| Pass checker textures to `SetMaterial` | `SetMaterial` requires `UMaterialInterface`. | Engine `PrimitiveComponent.h`. |
| Compile/generate a generic material in packaged runtime | Material graph authoring/compiler APIs are editor-only. | CkParticles editor generator and MaterialEditingLibrary boundary. |
| Store raw world/component/material pointers | Violates repository lifetime policy and PIE teardown safety. | Root doctrine and Ck Slate lifecycle incidents. |
| Restore blindly | Would overwrite gameplay/editor changes made after checker application. | Conflict-preserving ledger review, 2026-08-22. |
| Create a separate preview world for the main workflow | Too expensive and does not affect the actual scene the artist is diagnosing. | User decision, 2026-08-22. |
