#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Snapshot.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity, and a same-named helper in another .cpp would collide in
// the merged translation unit.
namespace ck_optimization_debugger_snapshot_impl
{
    constexpr auto k_BytesPerRun = 8;

    auto
        Append_Uint32(
            TArray<uint8>& OutBytes,
            uint32 InValue)
        -> void
    {
        OutBytes.Add(static_cast<uint8>(InValue & 0xFF));
        OutBytes.Add(static_cast<uint8>((InValue >> 8) & 0xFF));
        OutBytes.Add(static_cast<uint8>((InValue >> 16) & 0xFF));
        OutBytes.Add(static_cast<uint8>((InValue >> 24) & 0xFF));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Read_Uint32(
            const TArray<uint8>& InBytes,
            int32 InOffset)
        -> uint32
    {
        return static_cast<uint32>(InBytes[InOffset])
            | (static_cast<uint32>(InBytes[InOffset + 1]) << 8)
            | (static_cast<uint32>(InBytes[InOffset + 2]) << 16)
            | (static_cast<uint32>(InBytes[InOffset + 3]) << 24);
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_snapshot
{
    auto
        Encode_IdMapRle(
            const TArray<uint32>& InIds)
        -> TArray<uint8>
    {
        using namespace ck_optimization_debugger_snapshot_impl;

        auto Encoded = TArray<uint8>{};

        if (InIds.IsEmpty())
        { return Encoded; }

        auto RunValue = InIds[0];
        auto RunLength = static_cast<uint32>(1);

        for (auto Index = 1; Index < InIds.Num(); ++Index)
        {
            if (InIds[Index] == RunValue)
            {
                ++RunLength;
                continue;
            }

            Append_Uint32(Encoded, RunLength);
            Append_Uint32(Encoded, RunValue);

            RunValue = InIds[Index];
            RunLength = 1;
        }

        Append_Uint32(Encoded, RunLength);
        Append_Uint32(Encoded, RunValue);

        return Encoded;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Decode_IdMapRle(
            const TArray<uint8>& InRle)
        -> TArray<uint32>
    {
        using namespace ck_optimization_debugger_snapshot_impl;

        auto Decoded = TArray<uint32>{};

        // A buffer that is not a whole number of runs was truncated or is not an ID map at all. Decoding what fits
        // would hand back a map whose every pixel past the damage names the wrong mesh, which is worse than none.
        if (InRle.Num() % k_BytesPerRun != 0)
        { return Decoded; }

        for (auto Offset = 0; Offset < InRle.Num(); Offset += k_BytesPerRun)
        {
            const auto RunLength = Read_Uint32(InRle, Offset);
            const auto RunValue = Read_Uint32(InRle, Offset + 4);

            Decoded.Reserve(Decoded.Num() + static_cast<int32>(RunLength));

            for (auto Repeat = static_cast<uint32>(0); Repeat < RunLength; ++Repeat)
            { Decoded.Add(RunValue); }
        }

        return Decoded;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_IdAt(
            const TArray<uint32>& InDecodedIds,
            int32 InWidth,
            int32 InHeight,
            FIntPoint InPixel)
        -> TOptional<int32>
    {
        if (InWidth <= 0 || InHeight <= 0)
        { return {}; }

        if (InPixel.X < 0 || InPixel.X >= InWidth || InPixel.Y < 0 || InPixel.Y >= InHeight)
        { return {}; }

        const auto Index = InPixel.Y * InWidth + InPixel.X;

        if (NOT InDecodedIds.IsValidIndex(Index))
        { return {}; }

        const auto Id = InDecodedIds[Index];

        if (Id == k_NoPrim)
        { return {}; }

        return static_cast<int32>(Id);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_StencilSlot(
            int32 InPrimIndex)
        -> FCkOptimizationDebugger_StencilSlot
    {
        auto Slot = FCkOptimizationDebugger_StencilSlot{};

        if (InPrimIndex < 0)
        { return Slot; }

        Slot.PassIndex = InPrimIndex / k_StencilBatchSize;

        // 1-based within the pass: stencil 0 is what every primitive NOT in this pass is set to, so it can never
        // also mean "the first one".
        Slot.StencilValue = static_cast<uint8>((InPrimIndex % k_StencilBatchSize) + 1);

        return Slot;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_StencilPassCount(
            int32 InPrimCount)
        -> int32
    {
        if (InPrimCount <= 0)
        { return 0; }

        return FMath::DivideAndRoundUp(InPrimCount, k_StencilBatchSize);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Resolve_PrimFromPassValues(
            const TArray<uint8>& InPerPassStencil,
            int32 InPrimCount,
            int32& OutConflictCount)
        -> TOptional<int32>
    {
        auto Resolved = TOptional<int32>{};

        for (auto PassIndex = 0; PassIndex < InPerPassStencil.Num(); ++PassIndex)
        {
            const auto Value = InPerPassStencil[PassIndex];

            if (Value == 0)
            { continue; }

            const auto PrimIndex = PassIndex * k_StencilBatchSize + (static_cast<int32>(Value) - 1);

            // The last pass is usually partial, so a value past the end of the table is a misread rather than a
            // primitive - taking it would name a mesh that is not in the snapshot.
            if (PrimIndex < 0 || PrimIndex >= InPrimCount)
            { continue; }

            if (Resolved.IsSet())
            {
                ++OutConflictCount;
                continue;
            }

            Resolved = PrimIndex;
        }

        return Resolved;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_LetterboxGeometry(
            FVector2D InLocalSize,
            FIntPoint InImageSize)
        -> TOptional<FCkOptimizationDebugger_LetterboxGeometry>
    {
        if (InLocalSize.X <= 0.0 || InLocalSize.Y <= 0.0 || InImageSize.X <= 0 || InImageSize.Y <= 0)
        { return {}; }

        auto Geometry = FCkOptimizationDebugger_LetterboxGeometry{};

        Geometry.Scale = FMath::Min(
            InLocalSize.X / static_cast<double>(InImageSize.X),
            InLocalSize.Y / static_cast<double>(InImageSize.Y));

        Geometry.DrawnSize = FVector2D{InImageSize.X * Geometry.Scale, InImageSize.Y * Geometry.Scale};
        Geometry.Offset = (InLocalSize - Geometry.DrawnSize) * 0.5;

        return Geometry;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Map_ViewerPointToPixel(
            FVector2D InLocalSize,
            FIntPoint InImageSize,
            FVector2D InLocalPoint)
        -> TOptional<FIntPoint>
    {
        const auto Geometry = Get_LetterboxGeometry(InLocalSize, InImageSize);

        if (NOT Geometry.IsSet())
        { return {}; }

        const auto InImage = InLocalPoint - Geometry->Offset;

        if (InImage.X < 0.0 || InImage.Y < 0.0 ||
            InImage.X >= Geometry->DrawnSize.X || InImage.Y >= Geometry->DrawnSize.Y)
        { return {}; }

        // Floored, then clamped: the clamp only ever catches the last row and column, where floating-point error at
        // exactly the far edge would otherwise index one past the image.
        const auto Pixel = FIntPoint{
            FMath::Clamp(FMath::FloorToInt32(InImage.X / Geometry->Scale), 0, InImageSize.X - 1),
            FMath::Clamp(FMath::FloorToInt32(InImage.Y / Geometry->Scale), 0, InImageSize.Y - 1)};

        return Pixel;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Apply_SnapshotClick(
            FCkOptimizationDebugger_Snapshot& InSnapshot,
            TOptional<int32> InPrimIndex,
            ECkOptimizationDebugger_SnapshotClickModifier InModifier)
        -> void
    {
        // A selection set is only as trustworthy as the snapshot it was made against, and this is the one place
        // every click passes through, so it is the one place worth distrusting it.
        for (auto StaleIt = InSnapshot.SelectedPrims.CreateIterator(); StaleIt; ++StaleIt)
        {
            if (NOT InSnapshot.Prims.IsValidIndex(*StaleIt))
            { StaleIt.RemoveCurrent(); }
        }

        if (NOT InPrimIndex.IsSet())
        {
            // Only a plain click clears. A modifier says "adjust what I have", and adjusting by nothing is nothing —
            // a Shift-click that missed must not throw away the selection it was extending.
            if (InModifier == ECkOptimizationDebugger_SnapshotClickModifier::None)
            { InSnapshot.SelectedPrims.Reset(); }

            return;
        }

        const auto PrimIndex = InPrimIndex.GetValue();

        if (NOT InSnapshot.Prims.IsValidIndex(PrimIndex))
        { return; }

        switch (InModifier)
        {
            case ECkOptimizationDebugger_SnapshotClickModifier::Shift:
            {
                InSnapshot.SelectedPrims.Add(PrimIndex);
                break;
            }
            case ECkOptimizationDebugger_SnapshotClickModifier::Ctrl:
            {
                InSnapshot.SelectedPrims.Remove(PrimIndex);
                break;
            }
            default:
            {
                InSnapshot.SelectedPrims.Reset();
                InSnapshot.SelectedPrims.Add(PrimIndex);
                break;
            }
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_EstimatedDrawCallText(
            const FCkOptimizationDebugger_SnapshotPrim& InPrim)
        -> FString
    {
        const auto Sections = InPrim.Lods.IsEmpty() ? 0 : InPrim.Lods[0].Sections;

        return ck::Format_UE(TEXT("≈ {} draw calls (LOD0 sections)"), Sections);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_SelectionTotals(
            const FCkOptimizationDebugger_Snapshot& InSnapshot)
        -> FCkOptimizationDebugger_SnapshotSelectionTotals
    {
        auto Totals = FCkOptimizationDebugger_SnapshotSelectionTotals{};

        for (const auto& PrimIndex : InSnapshot.SelectedPrims)
        {
            if (NOT InSnapshot.Prims.IsValidIndex(PrimIndex))
            { continue; }

            const auto& Prim = InSnapshot.Prims[PrimIndex];

            ++Totals.PrimCount;
            Totals.InstanceCount += Prim.InstanceCount;

            if (Prim.Lods.IsEmpty())
            { continue; }

            Totals.Lod0Triangles += Prim.Lods[0].Triangles;
            Totals.Lod0Sections += Prim.Lods[0].Sections;
        }

        return Totals;
    }
}

// --------------------------------------------------------------------------------------------------------------------
