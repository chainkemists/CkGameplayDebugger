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
};

// --------------------------------------------------------------------------------------------------------------------

struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_SnapshotLod
{
    int32 Triangles = 0;
    int32 Sections = 0;
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

    TArray<FCkOptimizationDebugger_SnapshotLod> Lods;
    TArray<FCkOptimizationDebugger_SnapshotMaterialSlot> MaterialSlots;
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

    TArray64<uint8> ColorPng;

    // A colour-only snapshot is still a snapshot: it can be viewed and cycled, it just cannot be clicked into.
    bool HasIdMap = false;
    TArray<uint8> IdMapRle;
    int32 UnidentifiedPixelCount = 0;

    FString CaptureNotes;

    TArray<FCkOptimizationDebugger_SnapshotPrim> Prims;

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
