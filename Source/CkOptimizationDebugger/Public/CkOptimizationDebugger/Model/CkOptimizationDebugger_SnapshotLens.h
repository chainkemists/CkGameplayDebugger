#pragma once

#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_Thresholds.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Snapshot.h"

#include "CkCore/Macros/CkMacros.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

/** Which measurement the picture is painted by.
 *
 *  `None` is the captured picture itself, which is why it leads: the selector's first entry is always what the
 *  camera saw, and every other entry is that same frame recoloured by one number. */
enum class ECkOptimizationDebugger_SnapshotLens : uint8
{
    None,

    TriangleDensity,
    SamplerCount,
    TextureMemory,
    MeshMemory,
    InstanceCount,
    Distance,

    NaniteMask,
    MaterialFlags,
    Unidentified,

    Budget,
};

// --------------------------------------------------------------------------------------------------------------------

/** Which of a snapshot's images is on screen. ONE selector drives it, so the viewer only ever caches one brush and
 *  the hover/selection overlay draws on top of whichever this names. */
enum class ECkOptimizationDebugger_SnapshotViewKind : uint8
{
    Capture,
    Lens,
    Aux,
};

// --------------------------------------------------------------------------------------------------------------------

struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_SnapshotView
{
    ECkOptimizationDebugger_SnapshotViewKind Kind = ECkOptimizationDebugger_SnapshotViewKind::Capture;

    ECkOptimizationDebugger_SnapshotLens Lens = ECkOptimizationDebugger_SnapshotLens::None;

    int32 AuxIndex = INDEX_NONE;

    auto operator==(const FCkOptimizationDebugger_SnapshotView& InOther) const -> bool
    {
        return Kind == InOther.Kind && Lens == InOther.Lens && AuxIndex == InOther.AuxIndex;
    }

    auto operator!=(const FCkOptimizationDebugger_SnapshotView& InOther) const -> bool
    {
        return NOT (*this == InOther);
    }
};

// --------------------------------------------------------------------------------------------------------------------

/** Per-pixel optimization views computed from the ID map.
 *
 *  The whole point: the ID map already says WHICH mesh is at every pixel and the prim table already says what that
 *  mesh costs, so any per-mesh number is a heatmap a PURE function can paint. Nothing here renders, captures or
 *  reads the live world — which is what makes these views work on a snapshot loaded from a file on a machine that
 *  never saw that level, and in a packaged Development build where the engine's own debug viewmodes are gone.
 *
 *  Thresholds arrive as the plain struct, never off the settings CDO: the same rule the checks follow, for the same
 *  two reasons — a spec can build the struct, and a budget edited half way through cannot make one picture disagree
 *  with itself. */
namespace ck_optimization_debugger_snapshot_lens
{
    /** Every lens in selector order, `None` first. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_AllLenses() -> TArray<ECkOptimizationDebugger_SnapshotLens>;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_LensLabel(
        ECkOptimizationDebugger_SnapshotLens InLens) -> FString;

    /** What the colours MEAN, in one sentence. A heatmap without a legend is a picture, not a measurement — the
     *  reader cannot tell a red that means "worst in this view" from a red that means "over budget", and those are
     *  different claims. Empty for `None`, which is not a measurement at all. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_LensLegendText(
        ECkOptimizationDebugger_SnapshotLens InLens) -> FString;

    // ----------------------------------------------------------------------------------------------------------------

    /** How many pixels each prim owns, indexed by prim. The sentinel is ignored, and so is any id the table cannot
     *  back — a stored ID map is only as trustworthy as the prim table it was captured beside.
     *
     *  This is the denominator of every "for how it is SEEN" question: a 200k-triangle mesh covering forty pixels is
     *  a different finding from the same mesh filling the screen, and only coverage tells them apart. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_ScreenCoverage(
        const TArray<uint32>& InDecodedIds,
        int32 InPrimCount) -> TArray<int32>;

    /** LOD0 triangles per pixel the mesh actually occupies. Zero coverage yields zero rather than infinity: a mesh
     *  that is in the table but behind something else has no density to report, and a sentinel value here would sort
     *  to the top of every list that ranks by it. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_TrianglesPerCoveredPixel(
        const FCkOptimizationDebugger_SnapshotPrim& InPrim,
        int32 InCoveredPixels) -> float;

    // ----------------------------------------------------------------------------------------------------------------

    /** One lens rasterized: row-major BGRA, exactly `Width * Height` entries, or EMPTY when the snapshot cannot be
     *  painted (no lens, no identification, or an ID map whose size disagrees with the picture).
     *
     *  Scalar lenses normalize over the values PRESENT IN THIS VIEW rather than an absolute scale, because the
     *  question a snapshot answers is "what is expensive HERE" — an absolute ramp calibrated for a whole project
     *  paints every indoor scene the same shade of green. `Budget` is the exception and says so in its legend: it
     *  paints the graded severity of a real budget breach, so its red means the same thing the findings page's red
     *  means about the same mesh. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_LensPixels(
        const FCkOptimizationDebugger_Snapshot& InSnapshot,
        const TArray<uint32>& InDecodedIds,
        ECkOptimizationDebugger_SnapshotLens InLens,
        const FCkOptimizationDebugger_Thresholds& InThresholds) -> TArray<FColor>;

    /** The worst budget this mesh breaches, unset when it breaches none. The `Budget` lens paints it and the mesh
     *  list badges it — one rule, so a row and a pixel cannot disagree about whether a mesh is over. */
    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_BudgetSeverity(
        const FCkOptimizationDebugger_SnapshotPrim& InPrim,
        const FCkOptimizationDebugger_Thresholds& InThresholds) -> TOptional<ECkOptimizationDebugger_Severity>;

    /** The largest sampler count any ONE of the mesh's materials uses. The budget is per material, so the maximum is
     *  the number that breaches it — a sum would flag a mesh with six modest materials and miss the one material
     *  that actually exceeds the platform's limit. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_MaxSamplerCount(
        const FCkOptimizationDebugger_SnapshotPrim& InPrim) -> int32;
}

// --------------------------------------------------------------------------------------------------------------------
