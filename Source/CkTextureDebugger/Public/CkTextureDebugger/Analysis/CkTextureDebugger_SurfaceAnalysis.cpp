#include "CkTextureDebugger/Analysis/CkTextureDebugger_SurfaceAnalysis.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::texture_debugger::surface_analysis
{
    auto
        Describe(
            UPrimitiveComponent* InComponent,
            UMaterialInterface* InMaterial) -> FCkTextureDebugger_SurfaceContext
    {
        auto Result = FCkTextureDebugger_SurfaceContext{};
        Result.Component = InComponent;
        Result.Material = InMaterial;

        if (InComponent != nullptr)
        {
            Result.CastsShadow = InComponent->CastShadow != 0;
            Result.CastsDynamicShadow = InComponent->bCastDynamicShadow != 0;
            Result.CastsStaticShadow = InComponent->bCastStaticShadow != 0;
            Result.CastsVolumetricTranslucentShadow = InComponent->bCastVolumetricTranslucentShadow != 0;
            Result.ReceivesDecals = InComponent->bReceivesDecals != 0;
            Result.HasStaticLighting = InComponent->HasStaticLighting();

            auto LightMapWidth = 0;
            auto LightMapHeight = 0;
            InComponent->GetLightMapResolution(LightMapWidth, LightMapHeight);
            if (LightMapWidth > 0 && LightMapHeight > 0)
            { Result.LightMapResolution = FIntPoint{LightMapWidth, LightMapHeight}; }

            if (const auto* StaticMeshComponent = Cast<UStaticMeshComponent>(InComponent))
            { Result.HasNaniteData = StaticMeshComponent->HasValidNaniteData(); }
        }

        if (InMaterial == nullptr)
        { return Result; }

        Result.HasMaterial = true;
        Result.BlendMode = InMaterial->GetBlendMode();
        Result.ShadingModels = InMaterial->GetShadingModels();
        Result.IsTwoSided = InMaterial->IsTwoSided();
        Result.IsMasked = InMaterial->IsMasked();
        Result.OpacityMaskClipValue = InMaterial->GetOpacityMaskClipValue();
        Result.IsTranslucent = Result.BlendMode == BLEND_Translucent ||
            Result.BlendMode == BLEND_Additive ||
            Result.BlendMode == BLEND_Modulate ||
            Result.BlendMode == BLEND_AlphaComposite;

        return Result;
    }
}
