#pragma once

#include "CoreMinimal.h"

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "UObject/SoftObjectPath.h"

// --------------------------------------------------------------------------------------------------------------------

/** Which kind of primitive a captured record came from. Landscape, BSP, Niagara and everything else are EXCLUDED at
 *  capture rather than represented here: their pixels read as unidentified and the snapshot's notes line says how
 *  many were left out, which is a smaller lie than a row that cannot describe what it names. */
enum class ECkOptimizationDebugger_SnapshotPrimKind : uint8
{
    StaticMesh,
    InstancedStaticMesh,
    SkeletalMesh,
};

// --------------------------------------------------------------------------------------------------------------------

/** One material slot as captured — labels, not live objects, so the row can still be printed after the world that
 *  produced it is gone. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_SnapshotMaterialSlot
{
    FString SlotName;
    FString MaterialName;
    FSoftObjectPath MaterialPath;

    FString BlendMode;
    FString ShadingModel;

    bool IsTwoSided = false;
    int32 UsedTextureCount = 0;

    // Names, not paths: the row and the report print them, and nothing navigates to a texture from here.
    TArray<FString> UsedTextureNames;
};

// --------------------------------------------------------------------------------------------------------------------

struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_SnapshotLod
{
    int32 Triangles = 0;
    int32 Sections = 0;
    int32 Vertices = 0;

    // The screen-size threshold this LOD activates at, as authored — what tech art tunes when a mesh
    // holds its detail too long. Zero when the mesh does not carry one.
    float ScreenSize = 0.0f;
};

// --------------------------------------------------------------------------------------------------------------------

/** One captured primitive. Soft paths and captured numbers only: a snapshot outlives the world it pictured — which
 *  is the entire point of storing one — so holding a live object here would be the module's no-live-handle rule
 *  broken by the one feature most able to break it. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_SnapshotPrim
{
    FString DisplayName;
    FString MeshDisplayName;
    FSoftObjectPath MeshAssetPath;

    ECkOptimizationDebugger_SnapshotPrimKind Kind = ECkOptimizationDebugger_SnapshotPrimKind::StaticMesh;

    bool IsNanite = false;
    int32 InstanceCount = 1;
    float DistanceFromCamera = 0.0f;

    // The mesh ASSET's exclusive resource size — the same API and mode the memory analyzer reports, so the two
    // numbers cannot disagree about one mesh.
    int64 MeshResourceSizeBytes = 0;

    // Resident bytes of the textures THIS mesh's materials sample, deduplicated within the mesh. Two meshes sharing
    // a texture each count it, which is the honest answer to "what does this mesh cost to draw" and deliberately
    // not the same question the snapshot-wide total answers.
    int64 TextureResidentBytes = 0;

    TArray<FCkOptimizationDebugger_SnapshotLod> Lods;
    TArray<FCkOptimizationDebugger_SnapshotMaterialSlot> MaterialSlots;
};

// --------------------------------------------------------------------------------------------------------------------

/** One auxiliary view captured beside the picture — depth, world normals, base colour, shader complexity.
 *
 *  Captured rather than computed, and STORED rather than recaptured on demand: producing one needs the live world
 *  (and, for the debug viewmodes, an editor), while LOOKING at one has to work from a shared file on a machine that
 *  never saw that world. The name is what the view selector prints, so it is the identity — a snapshot carrying two
 *  images called the same thing is a capture bug, not a display one. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_SnapshotAuxImage
{
    FString Name;
    TArray64<uint8> Png;
};

// --------------------------------------------------------------------------------------------------------------------

/** One capture: the picture, the per-pixel identity of what is in it, and the table those identities index. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_Snapshot
{
    FGuid Id;
    FString Label;

    // Handed IN by whoever captured it. Nothing in the model reads the clock, which is what lets a spec pin a label.
    FDateTime CapturedAt;

    FString WorldName;

    int32 Width = 0;
    int32 Height = 0;

    // Where the picture was taken from. It travels with the snapshot so the same framing can be captured again after
    // the world changed — which is the only way two captures of "the same view" are comparable pixel for pixel — and
    // so a report says where it was standing rather than leaving the reader to recognise the geometry.
    FVector CameraLocation = FVector::ZeroVector;
    FRotator CameraRotation = FRotator::ZeroRotator;

    // Doubles as the "does this snapshot know its own POV" flag: a capture always writes a positive FOV, and a
    // snapshot loaded from a bare image file has none to write.
    float CameraFov = 0.0f;

    // What the picture was rendered AT. QA compares captures from different machines, and a scalability preset or a
    // screen percentage changes what the same scene costs — a report without them is evidence nobody can weigh.
    FString ScalabilityPreset;
    float ScreenPercentage = 0.0f;
    FString BuildVersion;

    TArray64<uint8> ColorPng;

    // A colour-only snapshot is still a snapshot: it can be viewed and cycled, it just cannot be clicked into.
    bool HasIdMap = false;
    TArray<uint8> IdMapRle;
    int32 UnidentifiedPixelCount = 0;

    FString CaptureNotes;

    // Deduplicated across every visible material at capture time. Stored rather than recomputed because the prim
    // table keeps texture NAMES only — the dedup needed the paths, which existed only during the capture.
    int32 UniqueMaterialCount = 0;
    int32 UniqueTextureCount = 0;
    int64 TextureResidentBytes = 0;

    TArray<FCkOptimizationDebugger_SnapshotPrim> Prims;

    // Empty on every snapshot whose capture could not produce them, which is not a failure — the picture and the
    // identification are the feature, and an auxiliary view is an extra the view selector simply does not offer.
    TArray<FCkOptimizationDebugger_SnapshotAuxImage> AuxImages;

    // Per snapshot, so cycling away and back keeps what the reader marked on each one.
    TSet<int32> SelectedPrims;
};

// --------------------------------------------------------------------------------------------------------------------

enum class ECkOptimizationDebugger_SnapshotClickModifier : uint8
{
    None,
    Shift,
    Ctrl,
};

// --------------------------------------------------------------------------------------------------------------------

/** The snapshot rules that are worth testing, which is all of them that are not a draw call: the ID-map codec, the
 *  two coordinate mappings, the click semantics and the printed aggregates. Every one is pure over plain data. */
namespace ck_optimization_debugger_snapshot
{
    /** No primitive at this pixel — sky, an excluded primitive type, or one the stencil pass could not identify. */
    inline constexpr uint32 k_NoPrim = MAX_uint32;

    /** Runs of (count, value), both `uint32` little-endian, over row-major pixels. The value is `uint32` INCLUDING
     *  the sentinel: how many prims a capture admits is Phase-4 policy and must not be baked into the codec. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Encode_IdMapRle(
        const TArray<uint32>& InIds) -> TArray<uint8>;

    /** Empty for a truncated or malformed buffer, never a partial decode: half an ID map would silently misname
     *  every pixel after the damage. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Decode_IdMapRle(
        const TArray<uint8>& InRle) -> TArray<uint32>;

    /** Bounds-checked read over a DECODED map. Unset outside the image and unset on the sentinel — a caller cannot
     *  tell those apart, and does not need to: both mean "you clicked nothing". */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_IdAt(
        const TArray<uint32>& InDecodedIds,
        int32 InWidth,
        int32 InHeight,
        FIntPoint InPixel) -> TOptional<int32>;

    /** Stencil 0 is reserved for "not in this pass", so one pass distinguishes at most 255 primitives. */
    inline constexpr int32 k_StencilBatchSize = 255;

    struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_StencilSlot
    {
        int32 PassIndex = 0;
        uint8 StencilValue = 0;
    };

    /** Which pass a primitive is identified in, and the stencil value that identifies it there. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_StencilSlot(
        int32 InPrimIndex) -> FCkOptimizationDebugger_StencilSlot;

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_StencilPassCount(
        int32 InPrimCount) -> int32;

    /** One pixel's per-pass stencil readings collapsed to the primitive visible there.
     *
     *  Exactly one pass should read non-zero — the one holding the primitive that won the depth test. All zero
     *  means nothing identifiable is there (sky, an excluded type, a material that does not write custom depth).
     *  Two non-zero readings mean the scene moved between passes, which cannot be true within one game-thread
     *  scope: it resolves to the FIRST and counts the disagreement, because a capture that reported a picture it
     *  could not explain would be worse than one that says how often it was confused. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Resolve_PrimFromPassValues(
        const TArray<uint8>& InPerPassStencil,
        int32 InPrimCount,
        int32& OutConflictCount) -> TOptional<int32>;

    /** Where the image lands inside the widget under aspect-fit drawing. */
    struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_LetterboxGeometry
    {
        FVector2D Offset = FVector2D::ZeroVector;
        FVector2D DrawnSize = FVector2D::ZeroVector;
        double Scale = 0.0;
    };

    /** The one place the aspect-fit arithmetic lives. Both the viewer's DRAW rect and the click mapping below come
     *  from it, because a click that resolved against different geometry than the picture was drawn with would
     *  select a mesh next to the one under the cursor — worst at the edges, and never obviously wrong.
     *
     *  Unset when either size is degenerate: a widget that has not been laid out yet has none. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_LetterboxGeometry(
        FVector2D InLocalSize,
        FIntPoint InImageSize) -> TOptional<FCkOptimizationDebugger_LetterboxGeometry>;

    /** Widget-local point to image pixel under aspect-fit drawing. Unset in the letterbox bands, which is what
     *  stops a click on the empty margin from selecting the mesh nearest the edge. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Map_ViewerPointToPixel(
        FVector2D InLocalSize,
        FIntPoint InImageSize,
        FVector2D InLocalPoint) -> TOptional<FIntPoint>;

    /** Plain click replaces (an empty pixel clears), Shift adds, Ctrl removes. Mutates `SelectedPrims` and nothing
     *  else. Indices past the prim table are dropped on the way through: a selection set is only ever as trustworthy
     *  as the snapshot it was made against. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Apply_SnapshotClick(
        FCkOptimizationDebugger_Snapshot& InSnapshot,
        TOptional<int32> InPrimIndex,
        ECkOptimizationDebugger_SnapshotClickModifier InModifier) -> void;

    /** The ONE false-colour rule for painting an ID map — the debug dump and the report both use it, so the two
     *  images a reader compares cannot colour one mesh differently. Seeded per index rather than a gradient on
     *  purpose: adjacent indices getting unrelated colours is what lets two meshes sharing an edge be told apart,
     *  which is exactly where identification failures show. Sentinel is black. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_PrimIndexColor(
        uint32 InId) -> FColor;

    /** Whether this snapshot knows where it was taken from, and can therefore be captured again from the same
     *  place. A capture always writes a positive FOV; a snapshot loaded from a bare image file has no POV to write,
     *  and a zero-FOV camera is not a view anything could be reproduced through. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_HasPov(
        const FCkOptimizationDebugger_Snapshot& InSnapshot) -> bool;

    // ----------------------------------------------------------------------------------------------------------------

    /** Whole-view totals over the prim table. The detail panel's nothing-selected view and the report BOTH read
     *  this — one implementation, so the two can never show a reader different totals for one snapshot. */
    struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_SnapshotAggregates
    {
        int64 TotalLod0Triangles = 0;
        int32 TotalLod0Sections = 0;
        int32 TotalInstances = 0;

        int32 StaticCount = 0;
        int32 InstancedCount = 0;
        int32 SkeletalCount = 0;
        int32 NaniteCount = 0;
    };

    CKOPTIMIZATIONDEBUGGER_API auto
    Get_SnapshotAggregates(
        const TArray<FCkOptimizationDebugger_SnapshotPrim>& InPrims) -> FCkOptimizationDebugger_SnapshotAggregates;

    /** Deliberately approximate, and it says so where the reader can see it. A true per-pass draw count depends on
     *  what the renderer batched this frame, which is not attributable to one primitive offline — LOD0 sections is
     *  the honest proxy, and naming it in the string is what keeps the number from being read as measured. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_EstimatedDrawCallText(
        const FCkOptimizationDebugger_SnapshotPrim& InPrim) -> FString;

    // ----------------------------------------------------------------------------------------------------------------

    struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_SnapshotSelectionTotals
    {
        int32 PrimCount = 0;
        int64 Lod0Triangles = 0;
        int32 Lod0Sections = 0;
        int32 InstanceCount = 0;
    };

    /** What the detail panel prints above a multi-selection. Counts the SELECTED prims only, and counts LOD0 —
     *  the LOD the reader is looking at in the picture. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_SelectionTotals(
        const FCkOptimizationDebugger_Snapshot& InSnapshot) -> FCkOptimizationDebugger_SnapshotSelectionTotals;
}

// --------------------------------------------------------------------------------------------------------------------
