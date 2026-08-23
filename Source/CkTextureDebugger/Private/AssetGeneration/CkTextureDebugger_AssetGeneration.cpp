#include "AssetGeneration/CkTextureDebugger_AssetGeneration.h"

#if WITH_EDITOR

#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "Interfaces/IPluginManager.h"
#include "MaterialDomain.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "TextureCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace ck::texture_debugger::asset_generation
{
namespace
{
    constexpr auto PluginName = TEXT("CkDebugger");
    constexpr auto TextureDirectory = TEXT("/CkDebugger/TextureDebugger/Textures");
    constexpr auto MaterialPackageName = TEXT("/CkDebugger/TextureDebugger/Materials/M_CkTextureChecker");
    constexpr auto MaterialAssetName = TEXT("M_CkTextureChecker");
    constexpr auto CheckerTextureParameter = TEXT("CheckerTexture");

    struct FFixture
    {
        const TCHAR* SourceFileName;
        const TCHAR* AssetName;
        const TCHAR* ExpectedMd5;
        int32        ExpectedWidth;
        int32        ExpectedHeight;
    };

    constexpr FFixture Fixtures[] =
    {
        { TEXT("CustomUVChecker_ColorGrid_2K.png"), TEXT("T_CkTextureChecker_ColorGrid_2K"), TEXT("150A4BDAD5268415E6C555DCA507842B"), 2050, 2050 },
        { TEXT("CustomUVChecker_ColorGrid_4K.png"), TEXT("T_CkTextureChecker_ColorGrid_4K"), TEXT("309B0A867632B6D89562DEDC5A857EE0"), 4096, 4096 },
        { TEXT("CustomUVChecker_GoldGray_4K.png"), TEXT("T_CkTextureChecker_GoldGray_4K"), TEXT("8C701D3EB2538CAAA2F202C1889E1F8B"), 4096, 4096 },
        { TEXT("CustomUVChecker_RoundedSpectrum_4K.png"), TEXT("T_CkTextureChecker_RoundedSpectrum_4K"), TEXT("C5D862C06EC57C16F568956A7B199233"), 4096, 4096 },
        { TEXT("CustomUVChecker_DirectionalMono_4K.png"), TEXT("T_CkTextureChecker_DirectionalMono_4K"), TEXT("36E50F292064A92B62C102729DFD0533"), 4096, 4096 },
    };

    auto
    AddError(
        FResult& InOutResult,
        const FString& InError)
        -> void
    {
        InOutResult.Errors.Add(InError);
    }

    auto
    GetPluginBaseDir(
        FResult& InOutResult)
        -> FString
    {
        const auto Plugin = IPluginManager::Get().FindPlugin(PluginName);
        if (NOT Plugin.IsValid())
        {
            AddError(InOutResult, TEXT("CkDebugger plugin was not found; fixture paths cannot be resolved."));
            return {};
        }

        return Plugin->GetBaseDir();
    }

    auto
    GetSourceArtPath(
        const FString& InPluginBaseDir,
        const FFixture& InFixture)
        -> FString
    {
        return FPaths::Combine(InPluginBaseDir, TEXT("Content/TextureDebugger/SourceArt"), InFixture.SourceFileName);
    }

    auto
    GetTexturePackageName(
        const FFixture& InFixture)
        -> FString
    {
        return FString::Printf(TEXT("%s/%s"), TextureDirectory, InFixture.AssetName);
    }

    auto
    GetObjectPath(
        const FString& InPackageName,
        const TCHAR* InAssetName)
        -> FString
    {
        return FString::Printf(TEXT("%s.%s"), *InPackageName, InAssetName);
    }

    auto
    ValidateSourceArt(
        const FString& InPluginBaseDir,
        FResult& InOutResult)
        -> bool
    {
        auto IsValid = true;
        for (const auto& Fixture : Fixtures)
        {
            const auto SourcePath = GetSourceArtPath(InPluginBaseDir, Fixture);
            if (NOT FPaths::FileExists(SourcePath))
            {
                AddError(InOutResult, FString::Printf(TEXT("Checker source art is missing: %s"), *SourcePath));
                IsValid = false;
                continue;
            }

            const auto Hash = LexToString(FMD5Hash::HashFile(*SourcePath));
            if (NOT Hash.Equals(Fixture.ExpectedMd5, ESearchCase::IgnoreCase))
            {
                AddError(InOutResult, FString::Printf(
                    TEXT("Checker source hash mismatch for %s. Expected %s, got %s."),
                    Fixture.SourceFileName, Fixture.ExpectedMd5, *Hash));
                IsValid = false;
            }
        }
        return IsValid;
    }

    auto
    FindTexture(
        const FFixture& InFixture)
        -> UTexture2D*
    {
        const auto PackageName = GetTexturePackageName(InFixture);
        return LoadObject<UTexture2D>(nullptr, *GetObjectPath(PackageName, InFixture.AssetName));
    }

    auto
    FindMaterial()
        -> UMaterial*
    {
        return LoadObject<UMaterial>(nullptr, *GetObjectPath(MaterialPackageName, MaterialAssetName));
    }

    auto
    ValidateGeneratedAssets(
        FResult& InOutResult)
        -> bool
    {
        auto IsValid = true;
        for (const auto& Fixture : Fixtures)
        {
            auto* Texture = FindTexture(Fixture);
            if (Texture == nullptr)
            {
                AddError(InOutResult, FString::Printf(TEXT("Checker texture is missing: %s"), Fixture.AssetName));
                IsValid = false;
                continue;
            }

            const auto ImportedWidth = Texture->Source.GetSizeX();
            const auto ImportedHeight = Texture->Source.GetSizeY();
            if (ImportedWidth != Fixture.ExpectedWidth || ImportedHeight != Fixture.ExpectedHeight)
            {
                AddError(InOutResult, FString::Printf(
                    TEXT("Checker texture dimensions differ for %s. Expected %dx%d, got %dx%d."),
                    Fixture.AssetName, Fixture.ExpectedWidth, Fixture.ExpectedHeight,
                    ImportedWidth, ImportedHeight));
                IsValid = false;
            }
        }

        auto* Material = FindMaterial();
        if (Material == nullptr)
        {
            AddError(InOutResult, TEXT("Checker master material is missing."));
            return false;
        }

        const auto HasUsagePermutations =
            Material->CheckMaterialUsage_Concurrent(MATUSAGE_SkeletalMesh) &&
            Material->CheckMaterialUsage_Concurrent(MATUSAGE_InstancedStaticMeshes) &&
            Material->CheckMaterialUsage_Concurrent(MATUSAGE_Nanite);
        if (NOT HasUsagePermutations)
        {
            AddError(InOutResult, TEXT("Checker master material lacks a required skeletal, instanced-static-mesh, or Nanite usage permutation."));
            IsValid = false;
        }

        if (Material->GetShadingModels().HasShadingModel(MSM_Unlit) == false || Material->TwoSided == false)
        {
            AddError(InOutResult, TEXT("Checker master material must remain unlit and two-sided."));
            IsValid = false;
        }

        auto ParameterInfos = TArray<FMaterialParameterInfo>{};
        auto ParameterIds = TArray<FGuid>{};
        Material->GetAllTextureParameterInfo(ParameterInfos, ParameterIds);
        if (NOT ParameterInfos.ContainsByPredicate([](const FMaterialParameterInfo& InInfo)
            { return InInfo.Name == CheckerTextureParameter; }))
        {
            AddError(InOutResult, TEXT("Checker master material has no CheckerTexture texture parameter."));
            IsValid = false;
        }

        return IsValid;
    }

    auto
    CanBootstrap(
        FResult& InOutResult)
        -> bool
    {
        auto CanRunBootstrap = true;
        for (const auto& Fixture : Fixtures)
        {
            if (FPackageName::DoesPackageExist(GetTexturePackageName(Fixture)))
            {
                AddError(InOutResult, FString::Printf(
                    TEXT("Bootstrap refuses to overwrite existing checker package: %s. Use ValidateOnly after committing generated assets."),
                    *GetTexturePackageName(Fixture)));
                CanRunBootstrap = false;
            }
        }
        if (FPackageName::DoesPackageExist(MaterialPackageName))
        {
            AddError(InOutResult, FString::Printf(
                TEXT("Bootstrap refuses to overwrite existing checker package: %s. Use ValidateOnly after committing generated assets."),
                MaterialPackageName));
            CanRunBootstrap = false;
        }
        return CanRunBootstrap;
    }

    auto
    ImportTexture(
        const FString& InPluginBaseDir,
        const FFixture& InFixture,
        FResult& InOutResult)
        -> UTexture2D*
    {
        auto* Task = NewObject<UAssetImportTask>();
        auto* Factory = NewObject<UTextureFactory>(Task);
        Task->Filename = GetSourceArtPath(InPluginBaseDir, InFixture);
        Task->DestinationPath = TextureDirectory;
        Task->DestinationName = InFixture.AssetName;
        Task->Factory = Factory;
        Task->bAutomated = true;
        Task->bAsync = false;
        Task->bSave = true;
        Task->bReplaceExisting = false;
        Task->bReplaceExistingSettings = false;

        FAssetToolsModule::GetModule().Get().ImportAssetTasks({Task});
        const auto& Imported = Task->GetObjects();
        auto* Texture = Imported.Num() == 1 ? Cast<UTexture2D>(Imported[0]) : nullptr;
        if (Texture == nullptr)
        {
            AddError(InOutResult, FString::Printf(TEXT("Failed to import checker texture: %s"), InFixture.SourceFileName));
            return nullptr;
        }

        InOutResult.GeneratedPackageNames.Add(Texture->GetOutermost()->GetName());
        return Texture;
    }

    auto
    CreateMaterial(
        UTexture2D* InDefaultTexture,
        FResult& InOutResult)
        -> UMaterial*
    {
        if (InDefaultTexture == nullptr)
        {
            AddError(InOutResult, TEXT("Cannot create checker material without the default checker texture."));
            return nullptr;
        }

        auto* Package = CreatePackage(MaterialPackageName);
        if (Package == nullptr)
        {
            AddError(InOutResult, TEXT("Failed to create the checker material package."));
            return nullptr;
        }

        auto* Material = NewObject<UMaterial>(Package, MaterialAssetName, RF_Public | RF_Standalone);
        if (Material == nullptr)
        {
            AddError(InOutResult, TEXT("Failed to create the checker master material."));
            return nullptr;
        }

        Material->MaterialDomain = MD_Surface;
        Material->BlendMode = BLEND_Opaque;
        Material->SetShadingModel(MSM_Unlit);
        Material->TwoSided = true;
        Material->bUsedWithSkeletalMesh = true;
        Material->bUsedWithInstancedStaticMeshes = true;
        Material->bUsedWithNanite = true;

        auto* Sample = Cast<UMaterialExpressionTextureSampleParameter2D>(
            UMaterialEditingLibrary::CreateMaterialExpression(
                Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), -400, 0));
        if (Sample == nullptr)
        {
            AddError(InOutResult, TEXT("Failed to create the CheckerTexture material expression."));
            return nullptr;
        }

        Sample->ParameterName = CheckerTextureParameter;
        Sample->Texture = InDefaultTexture;
        Sample->SamplerType = UMaterialExpressionTextureBase::GetSamplerTypeForTexture(InDefaultTexture);
        if (NOT UMaterialEditingLibrary::ConnectMaterialProperty(Sample, TEXT("RGB"), MP_EmissiveColor))
        {
            AddError(InOutResult, TEXT("Failed to connect CheckerTexture to the unlit emissive output."));
            return nullptr;
        }

        UMaterialEditingLibrary::RecompileMaterial(Material);
        Material->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(Material);

        const auto Filename = FPackageName::LongPackageNameToFilename(
            MaterialPackageName, FPackageName::GetAssetPackageExtension());
        auto SaveArgs = FSavePackageArgs{};
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        if (NOT UPackage::SavePackage(Package, Material, *Filename, SaveArgs))
        {
            AddError(InOutResult, TEXT("Failed to save the checker master material."));
            return nullptr;
        }

        InOutResult.GeneratedPackageNames.Add(MaterialPackageName);
        return Material;
    }
}

auto
Run(
    EMode InMode)
    -> FResult
{
    auto Result = FResult{};
    const auto PluginBaseDir = GetPluginBaseDir(Result);
    if (PluginBaseDir.IsEmpty() || NOT ValidateSourceArt(PluginBaseDir, Result))
    {
        return Result;
    }

    if (InMode == EMode::ValidateOnly)
    {
        Result.Succeeded = ValidateGeneratedAssets(Result);
        return Result;
    }

    if (NOT CanBootstrap(Result))
    {
        return Result;
    }

    auto ImportedTextures = TArray<UTexture2D*>{};
    ImportedTextures.Reserve(UE_ARRAY_COUNT(Fixtures));
    for (const auto& Fixture : Fixtures)
    {
        auto* Texture = ImportTexture(PluginBaseDir, Fixture, Result);
        if (Texture == nullptr)
        {
            return Result;
        }
        ImportedTextures.Add(Texture);
    }

    auto TexturesToFinish = TArray<UTexture*>{};
    TexturesToFinish.Reserve(ImportedTextures.Num());
    for (auto* Texture : ImportedTextures)
    {
        TexturesToFinish.Add(Texture);
    }
    FTextureCompilingManager::Get().FinishCompilation(TexturesToFinish);

    if (CreateMaterial(ImportedTextures[0], Result) == nullptr)
    {
        return Result;
    }

    Result.Errors.Reset();
    Result.Succeeded = ValidateGeneratedAssets(Result);
    return Result;
}
}

#endif
