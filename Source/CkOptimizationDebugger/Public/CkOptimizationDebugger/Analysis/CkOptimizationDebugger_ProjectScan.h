#pragma once

#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_Thresholds.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/SoftObjectPath.h"

// --------------------------------------------------------------------------------------------------------------------

struct FAssetData;
class UObject;

// --------------------------------------------------------------------------------------------------------------------

/** What the asset REGISTRY says about one asset, with nothing loaded.
 *
 *  Every numeric field is `-1` when the registry did not carry it, and `-1` is never treated as a value. A registry
 *  built by an older editor, an asset saved before a tag existed, or a class that writes no tags at all are all
 *  ordinary — and a check that read a missing triangle count as zero would report every one of them as clean.
 *
 *  Plain data with no `FAssetData` inside, so the rules over it are pure and a spec can drive them without a
 *  project on disk. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_AssetFacts
{
    FSoftObjectPath Path;

    FString DisplayName;

    bool IsStaticMesh = false;
    bool IsTexture = false;
    bool IsSound = false;

    // ---- Static mesh ----
    int32 TriangleCount = -1;
    int32 LodCount = -1;
    int32 CollisionPrimitiveCount = -1;
    int32 MaterialSlotCount = -1;

    /** -1 unknown, 0 off, 1 on. Three states rather than a bool for the same reason the numbers are -1. */
    int8 NaniteEnabled = -1;

    /** The engine's own `CollisionComplexity` tag, verbatim. */
    FString CollisionComplexity;

    // ---- Texture ----
    int32 Width = -1;
    int32 Height = -1;

    /** The engine's own `MipGenSettings` tag, verbatim (e.g. `TMGS_NoMipmaps`). */
    FString MipGenSettings;

    // ---- Sound ----
    //
    // Nothing here comes from the registry: `USoundWave` writes no cost-bearing tags at all, so every audio answer
    // is a deep-pass answer. Said out loud because it is the one family where the registry pass contributes
    // nothing, and a reader watching the fast half finish should not conclude their audio is clean.

    /** Whether a question about this asset can only be answered by LOADING it — the mip check's texture-group half,
     *  the sRGB check's data-texture predicate, the Nanite material-usage flag. The registry pass sets it; the deep
     *  pass consumes it, and nothing else decides which assets get opened. */
    bool NeedsDeepPass = false;
};

// --------------------------------------------------------------------------------------------------------------------

/** Where a project scan has got to. Plain data so the incremental driver can be stopped, resumed and reported on
 *  without the scan owning a timer or a window. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_ProjectScanState
{
    TArray<FCkOptimizationDebugger_AssetFacts> Assets;

    /** How far the registry pass has walked. */
    int32 NextRegistryIndex = 0;

    /** Assets the registry pass flagged as needing a load, and how far the deep pass has walked them. */
    TArray<int32> DeepQueue;
    int32 NextDeepIndex = 0;

    TArray<FCkOptimizationDebugger_FindingRow> Findings;

    int32 DeepLoadedCount = 0;

    bool IsRunning = false;
    bool WasCancelled = false;

    FCkOptimizationDebugger_Thresholds Thresholds;

    auto
    Get_TotalSteps() const -> int32;

    auto
    Get_CompletedSteps() const -> int32;

    /** 0..1, and exactly 1 only when both passes are done — a bar that reads full while work remains is a bar the
     *  reader stops believing. */
    auto
    Get_Progress() const -> float;
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_project_scan
{
    /** The registry-answerable checks over ONE asset's facts. Pure, and the reason the project pass can run over a
     *  whole project without opening anything.
     *
     *  It runs the SAME rules the level scan's checks do, phrased against the registry's tags rather than against a
     *  loaded object. That is a real duplication risk, and it is bounded deliberately: only checks whose WHOLE
     *  condition is a number or a flag the registry carries appear here. Anything needing a predicate — is this a
     *  data texture, is this material Nanite-incompatible — is flagged `NeedsDeepPass` and answered by the check's
     *  own exported predicate instead. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Run_RegistryChecks(
        const FCkOptimizationDebugger_AssetFacts& InFacts,
        const FCkOptimizationDebugger_Thresholds& InThresholds,
        TArray<FCkOptimizationDebugger_FindingRow>& OutFindings) -> void;

    /** Whether a number the registry did not carry should stop a check from firing. Exists as a named function
     *  because "-1 means unknown" has to be one rule rather than a convention every check re-implements. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_IsKnown(
        int32 InValue) -> bool;

    /** The heaviest meshes the project scan saw, densest first, at most `InTopCount` of them.
     *
     *  A RANKING rather than a budget: it fires no findings and flags nothing as wrong. "What are the biggest
     *  things in this project" is the question a reader asks before they know what their budget should be, and it
     *  is answerable from the registry pass alone.
     *
     *  Assets whose triangle count the registry did not carry are EXCLUDED rather than ranked as zero — a table
     *  claiming a mesh has no triangles because nobody wrote the tag would be worse than a shorter table. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_HeaviestMeshes(
        const TArray<FCkOptimizationDebugger_AssetFacts>& InAssets,
        int32 InTopCount) -> TArray<FCkOptimizationDebugger_AssetFacts>;

    // ----------------------------------------------------------------------------------------------------------------

#if WITH_EDITOR

    /** Every static mesh and texture under `/Game`, read from the registry with NOTHING loaded and NOTHING
     *  resolved: `GetAssetsByPath` with `bOnDiskOnly`, and `FAssetData::IsInstanceOf` with the default
     *  `EResolveClass::No`, exactly as the disk breakdown does. An audit tool that loaded a class to find out what
     *  it was would be the defect it exists to report. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Collect_ProjectAssets() -> TArray<FCkOptimizationDebugger_AssetFacts>;

    /** Parses one registry record. Exposed so a spec can hand it a hand-built `FAssetData` if one is ever cheap to
     *  make; the project walk is what normally calls it. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Parse_AssetFacts(
        const FAssetData& InAssetData) -> FCkOptimizationDebugger_AssetFacts;

    /** The checks that need the OBJECT, run over one loaded asset. Each one asks the level scan's own exported
     *  predicate rather than a second copy of the rule. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Run_DeepChecks(
        const FCkOptimizationDebugger_AssetFacts& InFacts,
        const FCkOptimizationDebugger_Thresholds& InThresholds,
        TArray<FCkOptimizationDebugger_FindingRow>& OutFindings) -> void;

    /** Advances the scan by at most `InBudget` assets and returns whether anything is left to do. Incremental on
     *  purpose: a project pass cannot sit inside a modal `FScopedSlowTask` and still cancel in about a second, and
     *  the registry pass is fast enough that the budget mostly matters for the deep one, which LOADS. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Advance(
        FCkOptimizationDebugger_ProjectScanState& InOutState,
        int32 InBudget) -> bool;

#endif
}

// --------------------------------------------------------------------------------------------------------------------
