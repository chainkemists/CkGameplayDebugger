#pragma once

#include "CoreMinimal.h"
#include "MaterialShared.h"

class UMaterialInterface;
class UPrimitiveComponent;

struct CKTEXTUREDEBUGGER_API FCkTextureDebugger_SurfaceContext
{
    TWeakObjectPtr<UPrimitiveComponent> Component;
    TWeakObjectPtr<UMaterialInterface> Material;

    EBlendMode BlendMode = BLEND_Opaque;
    FMaterialShadingModelField ShadingModels;
    bool HasMaterial = false;
    bool IsTwoSided = false;
    bool IsMasked = false;
    bool IsTranslucent = false;
    float OpacityMaskClipValue = 0.0f;

    bool CastsShadow = false;
    bool CastsDynamicShadow = false;
    bool CastsStaticShadow = false;
    bool CastsVolumetricTranslucentShadow = false;
    bool ReceivesDecals = false;
    bool HasStaticLighting = false;

    TOptional<bool> HasNaniteData;
    TOptional<FIntPoint> LightMapResolution;
};

namespace ck::texture_debugger::surface_analysis
{
    CKTEXTUREDEBUGGER_API auto Describe(
        UPrimitiveComponent* InComponent,
        UMaterialInterface* InMaterial) -> FCkTextureDebugger_SurfaceContext;
}
