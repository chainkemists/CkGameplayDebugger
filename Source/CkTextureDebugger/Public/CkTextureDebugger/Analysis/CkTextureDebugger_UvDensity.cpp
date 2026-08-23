#include "CkTextureDebugger/Analysis/CkTextureDebugger_UvDensity.h"

#include "CkCore/Macros/CkMacros.h"

#include "Components/MeshComponent.h"
#include "Engine/TextureStreamingTypes.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_texture_debugger_uv_density
{
    auto
        MakeUnavailable(
            ECkTextureDebugger_UvDensityAvailability InAvailability,
            FString InReason) -> FCkTextureDebugger_UvDensityResult
    {
        auto Result = FCkTextureDebugger_UvDensityResult{};
        Result.Availability = InAvailability;
        Result.UnavailableReason = MoveTemp(InReason);
        return Result;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::texture_debugger::uv_density
{
    auto
        EvaluateTriangleEvidence(
            const FCkTextureDebugger_UvTriangleEvidence& InEvidence) -> FCkTextureDebugger_UvDensityResult
    {
        using namespace ck_texture_debugger_uv_density;

        if (NOT InEvidence.HasAuthoritativeWorldArea || InEvidence.WorldTriangleAreaCm2 <= 0.0)
        { return MakeUnavailable(ECkTextureDebugger_UvDensityAvailability::MissingTriangleWorldArea, TEXT("Authoritative world-space triangle area is unavailable.")); }

        if (NOT InEvidence.HasAuthoritativeUvArea || InEvidence.UvTriangleArea <= 0.0)
        { return MakeUnavailable(ECkTextureDebugger_UvDensityAvailability::MissingTriangleUvArea, TEXT("Authoritative UV triangle area is unavailable.")); }

        if (NOT InEvidence.HasProvenTextureBinding)
        { return MakeUnavailable(ECkTextureDebugger_UvDensityAvailability::UnprovenTextureBinding, TEXT("The selected texture cannot be proven to bind to this material/section.")); }

        if (NOT InEvidence.HasProvenTextureTransform ||
            InEvidence.TextureCoordinateScale.X == 0.0 || InEvidence.TextureCoordinateScale.Y == 0.0)
        { return MakeUnavailable(ECkTextureDebugger_UvDensityAvailability::UnprovenTextureTransform, TEXT("The texture-coordinate transform is not proven for this triangle.")); }

        if (InEvidence.TextureWidth <= 0 || InEvidence.TextureHeight <= 0)
        { return MakeUnavailable(ECkTextureDebugger_UvDensityAvailability::InvalidTextureDimensions, TEXT("The selected texture has no usable cooked dimensions.")); }

        const auto TextureTexelArea = static_cast<double>(InEvidence.TextureWidth) * static_cast<double>(InEvidence.TextureHeight);
        const auto ScaledUvArea = InEvidence.UvTriangleArea * FMath::Abs(
            InEvidence.TextureCoordinateScale.X * InEvidence.TextureCoordinateScale.Y);

        auto Result = FCkTextureDebugger_UvDensityResult{};
        Result.Availability = ECkTextureDebugger_UvDensityAvailability::Available;
        Result.TexelsPerCm = FMath::Sqrt((TextureTexelArea * ScaledUvArea) / InEvidence.WorldTriangleAreaCm2);
        return Result;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        InspectComponentCapability(
            UMeshComponent* InComponent,
            int32 InMaterialSlot,
            int32 InUvChannel,
            int32 InSectionIndex,
            int32 InCollisionFaceIndex) -> FCkTextureDebugger_UvDensityResult
    {
        using namespace ck_texture_debugger_uv_density;

        if (InComponent == nullptr || NOT InComponent->IsRegistered())
        { return MakeUnavailable(ECkTextureDebugger_UvDensityAvailability::InvalidComponent, TEXT("The selected mesh component is unavailable or unregistered.")); }

        if (InMaterialSlot < 0 || InMaterialSlot >= InComponent->GetNumMaterials())
        { return MakeUnavailable(ECkTextureDebugger_UvDensityAvailability::InvalidMaterialSlot, TEXT("The selected material slot is invalid.")); }

        if (InUvChannel < 0)
        { return MakeUnavailable(ECkTextureDebugger_UvDensityAvailability::MissingTriangleUvArea, TEXT("The selected UV channel is invalid.")); }

        // Recorded as part of the request context for a later CPU-mesh evidence provider. The runtime streaming API
        // cannot prove a collision face's triangle/section mapping, so neither value participates in a density claim.
        static_cast<void>(InSectionIndex);
        static_cast<void>(InCollisionFaceIndex);

        auto MaterialData = FPrimitiveMaterialInfo{};
        if (NOT InComponent->GetMaterialStreamingData(InMaterialSlot, MaterialData) || NOT MaterialData.IsValid())
        { return MakeUnavailable(ECkTextureDebugger_UvDensityAvailability::MissingStreamingData, TEXT("The mesh has no authoritative runtime material UV-density data for this slot.")); }

        // FPrimitiveMaterialInfo proves that the renderer has mesh UV metadata, but it does not expose a public
        // triangle selection or a texture-sampler-to-UV-channel mapping. Returning a number here would fabricate
        // both facts; an editor/CPU-mesh caller must supply FCkTextureDebugger_UvTriangleEvidence instead.
        return MakeUnavailable(ECkTextureDebugger_UvDensityAvailability::UnprovenTextureBinding,
            TEXT("Runtime streaming data lacks a public selected-triangle texture-binding and transform proof."));
    }
}
