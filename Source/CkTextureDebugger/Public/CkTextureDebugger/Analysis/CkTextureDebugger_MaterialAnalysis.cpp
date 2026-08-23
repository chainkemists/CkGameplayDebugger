#include "CkTextureDebugger/Analysis/CkTextureDebugger_MaterialAnalysis.h"

#include "CkCore/Macros/CkMacros.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInterface.h"
#include "RHIShaderPlatform.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_texture_debugger_material_analysis
{
    auto
        GetQualityLevel() -> EMaterialQualityLevel::Type
    {
        return static_cast<EMaterialQualityLevel::Type>(UKismetSystemLibrary::GetRenderingMaterialQualityLevel());
    }

    auto
        MakeUnavailableRow(
            const FMaterialParameterInfo& InParameterInfo,
            FString InReason) -> FCkTextureDebugger_MaterialTextureRow
    {
        auto Result = FCkTextureDebugger_MaterialTextureRow{};
        Result.Provenance = ECkTextureDebugger_MaterialTextureProvenance::Unavailable;
        Result.ParameterInfo = InParameterInfo;
        Result.DisplayName = InParameterInfo.Name.ToString();
        Result.UnavailableReason = MoveTemp(InReason);
        return Result;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::texture_debugger::material_analysis
{
    auto
        Get_ActiveOptions() -> FCkTextureDebugger_MaterialAnalysisOptions
    {
        auto Result = FCkTextureDebugger_MaterialAnalysisOptions{};
        Result.QualityLevel = ck_texture_debugger_material_analysis::GetQualityLevel();
        Result.ShaderPlatform = GMaxRHIShaderPlatform;
        return Result;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Analyze(
            UMaterialInterface* InMaterial,
            const FCkTextureDebugger_MaterialAnalysisOptions& InOptions) -> FCkTextureDebugger_MaterialAnalysis
    {
        auto Result = FCkTextureDebugger_MaterialAnalysis{};
        Result.Material = InMaterial;

        const auto Options = InOptions.QualityLevel.IsSet() || InOptions.ShaderPlatform.IsSet()
            ? InOptions
            : Get_ActiveOptions();

        Result.ActiveQualityLevel = Options.QualityLevel.Get(EMaterialQualityLevel::High);
        Result.ActiveShaderPlatform = Options.ShaderPlatform.Get(GMaxRHIShaderPlatform);

        if (InMaterial == nullptr)
        {
            Result.Rows.Add(ck_texture_debugger_material_analysis::MakeUnavailableRow(
                FMaterialParameterInfo{}, TEXT("No material is resolved for this slot.")));
            return Result;
        }

        // Parameter rows are the only rows that can make the stronger claim "resolved value". The virtual query
        // walks the material-instance inheritance chain for us; no editor-only expression traversal is required.
        auto ParameterInfos = TArray<FMaterialParameterInfo>{};
        auto ParameterIds = TArray<FGuid>{};
        InMaterial->GetAllTextureParameterInfo(ParameterInfos, ParameterIds);
        for (const auto& ParameterInfo : ParameterInfos)
        {
            auto* Texture = static_cast<UTexture*>(nullptr);
            if (NOT InMaterial->GetTextureParameterValue(FHashedMaterialParameterInfo{ParameterInfo}, Texture))
            {
                Result.Rows.Add(ck_texture_debugger_material_analysis::MakeUnavailableRow(
                    ParameterInfo, TEXT("The active material instance does not resolve this texture parameter.")));
                continue;
            }

            if (Texture == nullptr)
            {
                Result.Rows.Add(ck_texture_debugger_material_analysis::MakeUnavailableRow(
                    ParameterInfo, TEXT("The active material instance resolves this texture parameter to null.")));
                continue;
            }

            auto Row = FCkTextureDebugger_MaterialTextureRow{};
            Row.Provenance = ECkTextureDebugger_MaterialTextureProvenance::Parameter;
            Row.ParameterInfo = ParameterInfo;
            Row.Texture = Texture;
            Row.TexturePath = FSoftObjectPath{Texture};
            Row.DisplayName = Texture->GetName();
            Result.Rows.Add(MoveTemp(Row));
        }

        // Used-texture rows are active-quality/platform candidates, not parameter-to-sampler evidence. Preserve that
        // distinction even when the same texture also appeared above as a resolved parameter.
        auto UsedTextures = TArray<UTexture*>{};
        InMaterial->GetUsedTextures(UsedTextures, Options.QualityLevel, Options.ShaderPlatform);

        auto SeenTextures = TSet<FSoftObjectPath>{};
        for (auto* Texture : UsedTextures)
        {
            if (Texture == nullptr)
            { continue; }

            const auto TexturePath = FSoftObjectPath{Texture};
            if (SeenTextures.Contains(TexturePath))
            { continue; }

            SeenTextures.Add(TexturePath);

            auto Row = FCkTextureDebugger_MaterialTextureRow{};
            Row.Provenance = ECkTextureDebugger_MaterialTextureProvenance::UsedTexture;
            Row.Texture = Texture;
            Row.TexturePath = TexturePath;
            Row.DisplayName = Texture->GetName();
            Row.UnavailableReason = TEXT("Potential active-variant texture; no public runtime API proves its sampler or material slot.");
            Result.Rows.Add(MoveTemp(Row));
        }

        return Result;
    }
}
