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

    // ----------------------------------------------------------------------------------------------------------------

    /** One row of the mesh list: everything the table shows about one captured primitive, already measured against
     *  this view and this project's budgets. Built once per refresh so the list, its sort and its badges all read
     *  the same numbers — a row that recomputed per column could sort by one value and print another. */
    struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_SnapshotMeshRow
    {
        int32 PrimIndex = INDEX_NONE;

        FString DisplayName;

        ECkOptimizationDebugger_SnapshotPrimKind Kind = ECkOptimizationDebugger_SnapshotPrimKind::StaticMesh;

        bool IsNanite = false;

        int32 Lod0Triangles = 0;

        /** `INDEX_NONE` when the snapshot carries no identification — which is not the same as zero pixels, and a
         *  zero printed there would read as "this mesh is not visible" rather than "this snapshot cannot tell". */
        int32 CoveredPixels = INDEX_NONE;

        float TrianglesPerPixel = 0.0f;

        int64 MeshResourceSizeBytes = 0;
        int64 TextureResidentBytes = 0;

        int32 InstanceCount = 0;
        int32 SlotCount = 0;

        TOptional<ECkOptimizationDebugger_Severity> BudgetSeverity;
    };

    // ----------------------------------------------------------------------------------------------------------------

    enum class ECkOptimizationDebugger_SnapshotMeshColumn : uint8
    {
        Mesh,
        Kind,
        Triangles,
        Coverage,
        Density,
        MeshMemory,
        Instances,
        Slots,
    };

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_AllMeshColumns() -> TArray<ECkOptimizationDebugger_SnapshotMeshColumn>;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_MeshColumnId(
        ECkOptimizationDebugger_SnapshotMeshColumn InColumn) -> FName;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_MeshColumnLabel(
        ECkOptimizationDebugger_SnapshotMeshColumn InColumn) -> FString;

    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_MeshColumnFromId(
        FName InColumnId) -> TOptional<ECkOptimizationDebugger_SnapshotMeshColumn>;

    /** One row per captured prim, in prim order. Coverage is handed in rather than recomputed so the list, the lens
     *  on screen and the picking all count the same pixels; pass an empty array for a snapshot without an ID map and
     *  the rows say so instead of claiming zero. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_SnapshotMeshRows(
        const FCkOptimizationDebugger_Snapshot& InSnapshot,
        const TArray<int32>& InCoverage,
        const FCkOptimizationDebugger_Thresholds& InThresholds) -> TArray<FCkOptimizationDebugger_SnapshotMeshRow>;

    /** Negative, zero or positive, exactly like a comparator: the sort and any future consumer share ONE ordering
     *  rule per column rather than two that can drift. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Compare_SnapshotMeshRows(
        const FCkOptimizationDebugger_SnapshotMeshRow& InLhs,
        const FCkOptimizationDebugger_SnapshotMeshRow& InRhs,
        ECkOptimizationDebugger_SnapshotMeshColumn InColumn) -> int32;

    /** Total by construction: the prim index breaks every tie, and it does so ASCENDING in both directions — the
     *  header toggle reverses the COLUMN, never the tie-break, so rows equal on the sorted column keep their relative
     *  order whichever way the arrow points. The memory page's rule, applied to a second table. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_SortedSnapshotMeshRows(
        const TArray<FCkOptimizationDebugger_SnapshotMeshRow>& InRows,
        ECkOptimizationDebugger_SnapshotMeshColumn InColumn,
        bool InAscending) -> TArray<FCkOptimizationDebugger_SnapshotMeshRow>;

    // ----------------------------------------------------------------------------------------------------------------

    enum class ECkOptimizationDebugger_SnapshotDeltaKind : uint8
    {
        Added,
        Removed,
        Changed,
    };

    /** One mesh's difference between two captures.
     *
     *  Keyed by mesh ASSET rather than by prim index, because prim indices are per capture: the same shelf is index 4
     *  in one snapshot and index 11 in the next, and a comparison that matched on index would report every mesh as
     *  both added and removed. Placements of one asset aggregate into a single row — what changed between two
     *  captures of a level is "there are three more of these", not three unrelated rows. */
    struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_SnapshotDeltaRow
    {
        ECkOptimizationDebugger_SnapshotDeltaKind Kind = ECkOptimizationDebugger_SnapshotDeltaKind::Changed;

        /** The asset path, or the display name when the capture recorded no path. Stable across captures either way,
         *  which is the only property the key needs. */
        FString Key;

        FString DisplayName;

        int32 PlacementDelta = 0;
        int64 Lod0TriangleDelta = 0;
        int32 InstanceDelta = 0;

        /** The ASSET's size, so it is a difference only when the asset itself changed between the two captures —
         *  summing it per placement would report a re-used mesh as new memory it does not cost. */
        int64 MeshMemoryDelta = 0;

        /** Unset when EITHER side lacks an ID map: coverage cannot be compared against a capture that never counted
         *  pixels, and a zero there would read as "this mesh takes up the same room" rather than "unknown". */
        TOptional<int32> CoverageDelta;
    };

    /** What changed between two captures, worst regression first.
     *
     *  Pure over the two snapshots: coverage is recomputed from each side's own stored RLE rather than handed in,
     *  because the baseline is not the snapshot on screen and nothing else has its pixels counted. Rows sort by
     *  triangle delta descending with the key as a total tie-break, so two runs over the same pair are byte-identical
     *  — a comparison table that reshuffled itself would make every re-read a fresh diff. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_SnapshotDelta(
        const FCkOptimizationDebugger_Snapshot& InBaseline,
        const FCkOptimizationDebugger_Snapshot& InCurrent) -> TArray<FCkOptimizationDebugger_SnapshotDeltaRow>;

    // ----------------------------------------------------------------------------------------------------------------

    /** The largest sampler count any ONE of the mesh's materials uses. The budget is per material, so the maximum is
     *  the number that breaches it — a sum would flag a mesh with six modest materials and miss the one material
     *  that actually exceeds the platform's limit. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_MaxSamplerCount(
        const FCkOptimizationDebugger_SnapshotPrim& InPrim) -> int32;
}

// --------------------------------------------------------------------------------------------------------------------
