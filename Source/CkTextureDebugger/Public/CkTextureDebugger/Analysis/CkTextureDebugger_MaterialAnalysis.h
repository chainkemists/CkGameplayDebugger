#pragma once

#include "CoreMinimal.h"
#include "MaterialShared.h"
#include "UObject/SoftObjectPath.h"

class UMaterialInterface;
class UTexture;

enum class ECkTextureDebugger_MaterialTextureProvenance : uint8
{
    Parameter,
    UsedTexture,
    Unavailable
};

struct CKTEXTUREDEBUGGER_API FCkTextureDebugger_MaterialTextureRow
{
    ECkTextureDebugger_MaterialTextureProvenance Provenance = ECkTextureDebugger_MaterialTextureProvenance::Unavailable;
    FMaterialParameterInfo ParameterInfo;
    TWeakObjectPtr<UTexture> Texture;
    FSoftObjectPath TexturePath;
    FString DisplayName;
    FString UnavailableReason;
};

struct CKTEXTUREDEBUGGER_API FCkTextureDebugger_MaterialAnalysisOptions
{
    TOptional<EMaterialQualityLevel::Type> QualityLevel;
    TOptional<EShaderPlatform> ShaderPlatform;
};

struct CKTEXTUREDEBUGGER_API FCkTextureDebugger_MaterialAnalysis
{
    TWeakObjectPtr<UMaterialInterface> Material;
    EMaterialQualityLevel::Type ActiveQualityLevel = EMaterialQualityLevel::High;
    EShaderPlatform ActiveShaderPlatform = SP_NumPlatforms;
    TArray<FCkTextureDebugger_MaterialTextureRow> Rows;
};

namespace ck::texture_debugger::material_analysis
{
    CKTEXTUREDEBUGGER_API auto Get_ActiveOptions() -> FCkTextureDebugger_MaterialAnalysisOptions;
    CKTEXTUREDEBUGGER_API auto Analyze(
        UMaterialInterface* InMaterial,
        const FCkTextureDebugger_MaterialAnalysisOptions& InOptions = {}) -> FCkTextureDebugger_MaterialAnalysis;
}
