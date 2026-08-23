#pragma once

#include "CoreMinimal.h"

class UMeshComponent;

enum class ECkTextureDebugger_UvDensityAvailability : uint8
{
    Available,
    InvalidComponent,
    InvalidMaterialSlot,
    MissingStreamingData,
    UnprovenTextureBinding,
    MissingTriangleWorldArea,
    MissingTriangleUvArea,
    UnprovenTextureTransform,
    InvalidTextureDimensions
};

struct CKTEXTUREDEBUGGER_API FCkTextureDebugger_UvTriangleEvidence
{
    double WorldTriangleAreaCm2 = 0.0;
    double UvTriangleArea = 0.0;
    int32 TextureWidth = 0;
    int32 TextureHeight = 0;
    FVector2d TextureCoordinateScale = FVector2d::ZeroVector;
    bool HasAuthoritativeWorldArea = false;
    bool HasAuthoritativeUvArea = false;
    bool HasProvenTextureBinding = false;
    bool HasProvenTextureTransform = false;
};

struct CKTEXTUREDEBUGGER_API FCkTextureDebugger_UvDensityResult
{
    ECkTextureDebugger_UvDensityAvailability Availability = ECkTextureDebugger_UvDensityAvailability::InvalidComponent;
    double TexelsPerCm = 0.0;
    FString UnavailableReason;
};

namespace ck::texture_debugger::uv_density
{
    /** Pure evaluator. Callers may provide numeric evidence only after they prove the selected triangle/binding/transform. */
    CKTEXTUREDEBUGGER_API auto EvaluateTriangleEvidence(
        const FCkTextureDebugger_UvTriangleEvidence& InEvidence) -> FCkTextureDebugger_UvDensityResult;

    /** Runtime component adapter. It reports streaming-data availability but deliberately never invents triangle evidence. */
    CKTEXTUREDEBUGGER_API auto InspectComponentCapability(
        UMeshComponent* InComponent,
        int32 InMaterialSlot,
        int32 InUvChannel,
        int32 InSectionIndex = INDEX_NONE,
        int32 InCollisionFaceIndex = INDEX_NONE) -> FCkTextureDebugger_UvDensityResult;
}
