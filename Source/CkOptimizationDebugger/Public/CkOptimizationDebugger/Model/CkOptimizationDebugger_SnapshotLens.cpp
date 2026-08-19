#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_SnapshotLens.h"

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkEditorTools/Style/CkStyle.h"

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity, and a same-named helper in another .cpp would collide in
// the merged translation unit.
namespace ck_optimization_debugger_snapshot_lens_impl
{
    /** What a mask lens paints its members. */
    auto
        Get_MemberColor()
        -> FColor
    {
        constexpr auto UseSrgb = true;
        return CkStyle::Accent().ToFColor(UseSrgb);
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Everything a mask lens does NOT flag, and everything within budget. The panel background rather than black,
     *  because the shape of what is not flagged is half of what the reader is looking at.  */
    auto
        Get_DimColor()
        -> FColor
    {
        constexpr auto UseSrgb = true;
        return CkStyle::Bg2().ToFColor(UseSrgb);
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Sky, an excluded primitive type, or a pixel no pass identified. Black on every lens, so "nothing to measure
     *  here" never reads as a measurement of zero. */
    auto
        Get_SentinelColor()
        -> FColor
    {
        return FColor::Black;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_IsMaskLens(
            ECkOptimizationDebugger_SnapshotLens InLens)
        -> bool
    {
        switch (InLens)
        {
            case ECkOptimizationDebugger_SnapshotLens::NaniteMask:
            case ECkOptimizationDebugger_SnapshotLens::MaterialFlags:
            case ECkOptimizationDebugger_SnapshotLens::Unidentified:
                return true;
            default:
                return false;
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Memory and density span orders of magnitude within one frame: one 8 MB mesh among fifty 40 KB ones would make
     *  a linear ramp paint every other mesh the same cold green and hide the ordering among them. */
    auto
        Get_IsLogScaled(
            ECkOptimizationDebugger_SnapshotLens InLens)
        -> bool
    {
        switch (InLens)
        {
            case ECkOptimizationDebugger_SnapshotLens::TriangleDensity:
            case ECkOptimizationDebugger_SnapshotLens::TextureMemory:
            case ECkOptimizationDebugger_SnapshotLens::MeshMemory:
                return true;
            default:
                return false;
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_IsMaskMember(
            const FCkOptimizationDebugger_SnapshotPrim& InPrim,
            ECkOptimizationDebugger_SnapshotLens InLens)
        -> bool
    {
        switch (InLens)
        {
            case ECkOptimizationDebugger_SnapshotLens::NaniteMask:
            {
                return InPrim.IsNanite;
            }
            case ECkOptimizationDebugger_SnapshotLens::MaterialFlags:
            {
                // Two-sided doubles the geometry the rasterizer submits, and anything not opaque leaves the opaque
                // pass. Both are things a reader hunts for by eye today, one mesh at a time.
                for (const auto& Slot : InPrim.MaterialSlots)
                {
                    const auto IsNonOpaque = NOT Slot.BlendMode.IsEmpty() && Slot.BlendMode != TEXT("Opaque");

                    if (Slot.IsTwoSided || IsNonOpaque)
                    { return true; }
                }

                return false;
            }
            default:
            {
                return false;
            }
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ScalarValue(
            const FCkOptimizationDebugger_SnapshotPrim& InPrim,
            int32 InCoverage,
            ECkOptimizationDebugger_SnapshotLens InLens)
        -> double
    {
        using namespace ck_optimization_debugger_snapshot_lens;

        switch (InLens)
        {
            case ECkOptimizationDebugger_SnapshotLens::TriangleDensity:
            {
                return static_cast<double>(Get_TrianglesPerCoveredPixel(InPrim, InCoverage));
            }
            case ECkOptimizationDebugger_SnapshotLens::SamplerCount:
            {
                return static_cast<double>(Get_MaxSamplerCount(InPrim));
            }
            case ECkOptimizationDebugger_SnapshotLens::TextureMemory:
            {
                return static_cast<double>(InPrim.TextureResidentBytes);
            }
            case ECkOptimizationDebugger_SnapshotLens::MeshMemory:
            {
                return static_cast<double>(InPrim.MeshResourceSizeBytes);
            }
            case ECkOptimizationDebugger_SnapshotLens::InstanceCount:
            {
                return static_cast<double>(InPrim.InstanceCount);
            }
            case ECkOptimizationDebugger_SnapshotLens::Distance:
            {
                return static_cast<double>(InPrim.DistanceFromCamera);
            }
            default:
            {
                return 0.0;
            }
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_Normalized(
            double InValue,
            double InMin,
            double InMax,
            bool InLogScaled)
        -> float
    {
        // One value, or fifty identical ones, is not a range: painting them all coldest is honest, and the legend
        // says the ramp is relative to this view.
        if (InMax <= InMin)
        { return 0.0f; }

        if (NOT InLogScaled)
        { return static_cast<float>((InValue - InMin) / (InMax - InMin)); }

        const auto LogMin = FMath::Loge(1.0 + InMin);
        const auto LogMax = FMath::Loge(1.0 + InMax);

        if (LogMax <= LogMin)
        { return 0.0f; }

        return static_cast<float>((FMath::Loge(1.0 + InValue) - LogMin) / (LogMax - LogMin));
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_snapshot_lens
{
    auto
        Get_AllLenses()
        -> TArray<ECkOptimizationDebugger_SnapshotLens>
    {
        return TArray<ECkOptimizationDebugger_SnapshotLens>{
            ECkOptimizationDebugger_SnapshotLens::None,
            ECkOptimizationDebugger_SnapshotLens::TriangleDensity,
            ECkOptimizationDebugger_SnapshotLens::SamplerCount,
            ECkOptimizationDebugger_SnapshotLens::TextureMemory,
            ECkOptimizationDebugger_SnapshotLens::MeshMemory,
            ECkOptimizationDebugger_SnapshotLens::InstanceCount,
            ECkOptimizationDebugger_SnapshotLens::Distance,
            ECkOptimizationDebugger_SnapshotLens::NaniteMask,
            ECkOptimizationDebugger_SnapshotLens::MaterialFlags,
            ECkOptimizationDebugger_SnapshotLens::Unidentified,
            ECkOptimizationDebugger_SnapshotLens::Budget};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_LensLabel(
            ECkOptimizationDebugger_SnapshotLens InLens)
        -> FString
    {
        switch (InLens)
        {
            case ECkOptimizationDebugger_SnapshotLens::TriangleDensity: return FString{TEXT("Triangle density")};
            case ECkOptimizationDebugger_SnapshotLens::SamplerCount:    return FString{TEXT("Texture samplers")};
            case ECkOptimizationDebugger_SnapshotLens::TextureMemory:   return FString{TEXT("Texture memory")};
            case ECkOptimizationDebugger_SnapshotLens::MeshMemory:      return FString{TEXT("Mesh memory")};
            case ECkOptimizationDebugger_SnapshotLens::InstanceCount:   return FString{TEXT("Instances")};
            case ECkOptimizationDebugger_SnapshotLens::Distance:        return FString{TEXT("Distance")};
            case ECkOptimizationDebugger_SnapshotLens::NaniteMask:      return FString{TEXT("Nanite")};
            case ECkOptimizationDebugger_SnapshotLens::MaterialFlags:   return FString{TEXT("Two-sided / non-opaque")};
            case ECkOptimizationDebugger_SnapshotLens::Unidentified:    return FString{TEXT("Unidentified pixels")};
            case ECkOptimizationDebugger_SnapshotLens::Budget:          return FString{TEXT("Over budget")};
            default:                                                    return FString{TEXT("Capture")};
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_LensLegendText(
            ECkOptimizationDebugger_SnapshotLens InLens)
        -> FString
    {
        switch (InLens)
        {
            case ECkOptimizationDebugger_SnapshotLens::TriangleDensity:
            {
                return FString{TEXT("LOD0 triangles per pixel the mesh covers, green to red across this view. Red is "
                    "detail nobody can see from here.")};
            }
            case ECkOptimizationDebugger_SnapshotLens::SamplerCount:
            {
                return FString{TEXT("The largest sampler count of any one of the mesh's materials, green to red "
                    "across this view.")};
            }
            case ECkOptimizationDebugger_SnapshotLens::TextureMemory:
            {
                return FString{TEXT("Resident bytes of the textures this mesh's materials sample, log-scaled green "
                    "to red across this view.")};
            }
            case ECkOptimizationDebugger_SnapshotLens::MeshMemory:
            {
                return FString{TEXT("The mesh asset's resource size, log-scaled green to red across this view.")};
            }
            case ECkOptimizationDebugger_SnapshotLens::InstanceCount:
            {
                return FString{TEXT("Instances per component, green to red across this view.")};
            }
            case ECkOptimizationDebugger_SnapshotLens::Distance:
            {
                return FString{TEXT("Distance from the camera, green (near) to red (far) across this view.")};
            }
            case ECkOptimizationDebugger_SnapshotLens::NaniteMask:
            {
                return FString{TEXT("Highlighted: Nanite meshes. Dimmed: everything else.")};
            }
            case ECkOptimizationDebugger_SnapshotLens::MaterialFlags:
            {
                return FString{TEXT("Highlighted: a mesh with a two-sided or non-opaque material slot.")};
            }
            case ECkOptimizationDebugger_SnapshotLens::Unidentified:
            {
                return FString{TEXT("Highlighted: pixels no mesh owns - sky, landscape, BSP, effects, or a "
                    "translucent material that writes no custom depth.")};
            }
            case ECkOptimizationDebugger_SnapshotLens::Budget:
            {
                return FString{TEXT("Meshes over a threshold, coloured by how far over - the same grading the "
                    "findings page uses. Dimmed: within budget.")};
            }
            default:
            {
                return FString{};
            }
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ScreenCoverage(
            const TArray<uint32>& InDecodedIds,
            int32 InPrimCount)
        -> TArray<int32>
    {
        using namespace ck_optimization_debugger_snapshot;

        auto Coverage = TArray<int32>{};

        if (InPrimCount <= 0)
        { return Coverage; }

        Coverage.SetNumZeroed(InPrimCount);

        for (const auto& Id : InDecodedIds)
        {
            if (Id == k_NoPrim)
            { continue; }

            const auto PrimIndex = static_cast<int32>(Id);

            // An id the table cannot back means the map was read against the wrong snapshot; counting it would put
            // pixels on a mesh that is not there.
            if (NOT Coverage.IsValidIndex(PrimIndex))
            { continue; }

            ++Coverage[PrimIndex];
        }

        return Coverage;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_TrianglesPerCoveredPixel(
            const FCkOptimizationDebugger_SnapshotPrim& InPrim,
            int32 InCoveredPixels)
        -> float
    {
        if (InCoveredPixels <= 0 || InPrim.Lods.IsEmpty())
        { return 0.0f; }

        return static_cast<float>(InPrim.Lods[0].Triangles) / static_cast<float>(InCoveredPixels);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_MaxSamplerCount(
            const FCkOptimizationDebugger_SnapshotPrim& InPrim)
        -> int32
    {
        auto Largest = 0;

        for (const auto& Slot : InPrim.MaterialSlots)
        { Largest = FMath::Max(Largest, Slot.UsedTextureCount); }

        return Largest;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_BudgetSeverity(
            const FCkOptimizationDebugger_SnapshotPrim& InPrim,
            const FCkOptimizationDebugger_Thresholds& InThresholds)
        -> TOptional<ECkOptimizationDebugger_Severity>
    {
        using namespace ck_optimization_debugger_thresholds;

        auto Worst = TOptional<ECkOptimizationDebugger_Severity>{};

        const auto Consider = [&Worst](double InValue, double InBudget) -> void
        {
            if (InBudget <= 0.0 || InValue <= InBudget)
            { return; }

            // Major at the budget, escalating to Critical at twice it - the analysis engine's own grading, called
            // rather than restated, so this picture and the findings list cannot disagree about one mesh.
            const auto Graded = Get_Graded(InValue, InBudget, ECkOptimizationDebugger_Severity::Major);

            // Severity is declared most-severe-first, so the worst of two is the SMALLER enum value.
            if (NOT Worst.IsSet() || Graded.Severity < Worst.GetValue())
            { Worst = Graded.Severity; }
        };

        const auto Lod0Triangles = InPrim.Lods.IsEmpty() ? 0 : InPrim.Lods[0].Triangles;

        Consider(static_cast<double>(Lod0Triangles), static_cast<double>(InThresholds.MaxTriangleCountLOD0));
        Consider(static_cast<double>(InPrim.MaterialSlots.Num()), static_cast<double>(InThresholds.MaxMaterialSlots));
        Consider(static_cast<double>(Get_MaxSamplerCount(InPrim)), static_cast<double>(InThresholds.MaxTextureSamplers));

        return Worst;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_LensPixels(
            const FCkOptimizationDebugger_Snapshot& InSnapshot,
            const TArray<uint32>& InDecodedIds,
            ECkOptimizationDebugger_SnapshotLens InLens,
            const FCkOptimizationDebugger_Thresholds& InThresholds)
        -> TArray<FColor>
    {
        using namespace ck_optimization_debugger_model;
        using namespace ck_optimization_debugger_snapshot;
        using namespace ck_optimization_debugger_snapshot_lens_impl;

        auto Pixels = TArray<FColor>{};

        if (InLens == ECkOptimizationDebugger_SnapshotLens::None)
        { return Pixels; }

        const auto PixelCount = InSnapshot.Width * InSnapshot.Height;

        // An ID map whose size disagrees with the picture cannot be painted onto it, and a lens drawn at the wrong
        // stride would be a convincing image of nothing.
        if (PixelCount <= 0 || InDecodedIds.Num() != PixelCount)
        { return Pixels; }

        const auto PrimCount = InSnapshot.Prims.Num();
        const auto Coverage = Get_ScreenCoverage(InDecodedIds, PrimCount);

        // One colour per PRIM, then one lookup per pixel: the per-pixel loop runs over a million entries on a
        // 1280-wide capture, and deciding a mesh's colour a million times would be that many redundant decisions.
        auto PrimColors = TArray<FColor>{};
        PrimColors.Init(Get_DimColor(), PrimCount);

        if (Get_IsMaskLens(InLens))
        {
            for (auto PrimIndex = 0; PrimIndex < PrimCount; ++PrimIndex)
            {
                // `Unidentified` is a mask over the SENTINEL, so every prim is a non-member by construction and its
                // highlight lands below, where the sentinel colour is chosen.
                PrimColors[PrimIndex] = Get_IsMaskMember(InSnapshot.Prims[PrimIndex], InLens)
                    ? Get_MemberColor()
                    : Get_DimColor();
            }
        }
        else if (InLens == ECkOptimizationDebugger_SnapshotLens::Budget)
        {
            for (auto PrimIndex = 0; PrimIndex < PrimCount; ++PrimIndex)
            {
                const auto Severity = TryGet_BudgetSeverity(InSnapshot.Prims[PrimIndex], InThresholds);

                if (NOT Severity.IsSet())
                { continue; }

                constexpr auto UseSrgb = true;
                PrimColors[PrimIndex] = CkStyle::GetToneColor(Get_SeverityTone(Severity.GetValue())).ToFColor(UseSrgb);
            }
        }
        else
        {
            const auto LogScaled = Get_IsLogScaled(InLens);

            auto Values = TArray<double>{};
            Values.SetNumZeroed(PrimCount);

            auto Smallest = TOptional<double>{};
            auto Largest = TOptional<double>{};

            for (auto PrimIndex = 0; PrimIndex < PrimCount; ++PrimIndex)
            {
                const auto CoveredPixels = Coverage.IsValidIndex(PrimIndex) ? Coverage[PrimIndex] : 0;

                Values[PrimIndex] = Get_ScalarValue(InSnapshot.Prims[PrimIndex], CoveredPixels, InLens);

                // The range comes from what is VISIBLE. A mesh the capture recorded but nothing can see contributes
                // no pixels, and letting it set the maximum would flatten the ramp across everything on screen.
                if (CoveredPixels <= 0)
                { continue; }

                Smallest = Smallest.IsSet() ? FMath::Min(Smallest.GetValue(), Values[PrimIndex]) : Values[PrimIndex];
                Largest = Largest.IsSet() ? FMath::Max(Largest.GetValue(), Values[PrimIndex]) : Values[PrimIndex];
            }

            for (auto PrimIndex = 0; PrimIndex < PrimCount; ++PrimIndex)
            {
                const auto Normalized = Get_Normalized(
                    Values[PrimIndex], Smallest.Get(0.0), Largest.Get(0.0), LogScaled);

                constexpr auto UseSrgb = true;
                PrimColors[PrimIndex] = ck::debug_axes::Get_HeatColor(Normalized).ToFColor(UseSrgb);
            }
        }

        const auto SentinelColor = InLens == ECkOptimizationDebugger_SnapshotLens::Unidentified
            ? Get_MemberColor()
            : Get_SentinelColor();

        Pixels.Reserve(PixelCount);

        for (const auto& Id : InDecodedIds)
        {
            const auto PrimIndex = static_cast<int32>(Id);

            Pixels.Add(Id != k_NoPrim && PrimColors.IsValidIndex(PrimIndex)
                ? PrimColors[PrimIndex]
                : SentinelColor);
        }

        return Pixels;
    }
}

// --------------------------------------------------------------------------------------------------------------------
