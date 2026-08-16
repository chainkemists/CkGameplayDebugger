#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Texture.h"

#if WITH_EDITOR

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureDefines.h"

// --------------------------------------------------------------------------------------------------------------------

// File-local helpers in the module's own named namespace — this module compiles unity, and an anonymous namespace
// would collide with the identically-shaped helpers in the other check files.
namespace ck_optimization_debugger_checks_texture
{
    /** Width and height as AUTHORED, preferring the editor-side source over the built platform data.
     *
     *  `UTexture2D::GetSizeX()` reads platform data, which in the editor can mean waiting on a DDC build for a
     *  texture nobody has touched this session — an audit tool must not make the editor build things to describe
     *  them. `FTextureSource` is serialized header data and is free to read. Dynamic textures have no valid
     *  source, which is why the platform-data path is still here as the fallback. */
    auto
        TryGet_TextureDimensions(
            UTexture* InTexture,
            int32& OutWidth,
            int32& OutHeight)
        -> bool
    {
        OutWidth = 0;
        OutHeight = 0;

        if (InTexture == nullptr)
        { return false; }

#if WITH_EDITORONLY_DATA
        if (InTexture->Source.IsValid())
        {
            // FTextureSource sizes are int64 in 5.7 — narrowed deliberately: a texture dimension past int32 is not
            // a thing this tool has to be right about.
            OutWidth = static_cast<int32>(InTexture->Source.GetSizeX());
            OutHeight = static_cast<int32>(InTexture->Source.GetSizeY());

            return OutWidth > 0 && OutHeight > 0;
        }
#endif

        if (auto* Texture2D = Cast<UTexture2D>(InTexture))
        {
            OutWidth = Texture2D->GetSizeX();
            OutHeight = Texture2D->GetSizeY();

            return OutWidth > 0 && OutHeight > 0;
        }

        return false;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Is_PowerOfTwo(
            int32 InValue)
        -> bool
    {
        return InValue > 0 && (InValue & (InValue - 1)) == 0;
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Whether the asset NAME claims this is a normal map. Name convention is the only signal available offline —
     *  the compression setting is exactly the thing under test, so it cannot also be the evidence. Kept narrow on
     *  purpose: a false positive here tells an artist their correctly-authored texture is wrong. */
    auto
        Is_NamedAsNormalMap(
            const FString& InAssetName)
        -> bool
    {
        static const auto Suffixes = TArray<FString>{
            TEXT("_N"), TEXT("_NRM"), TEXT("_Norm"), TEXT("_Normal"), TEXT("_NormalMap")};

        for (const auto& Suffix : Suffixes)
        {
            if (InAssetName.EndsWith(Suffix, ESearchCase::IgnoreCase))
            { return true; }
        }

        return false;
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Declared in the header — the `Texture.DataTextureSrgb` fix re-asks this before it mutates. sRGB on a data
     *  texture applies a colour curve to numbers that are not colours. */
    auto
        Is_DataTexture(
            const UTexture* InTexture,
            const FString& InAssetName)
        -> bool
    {
        if (InTexture == nullptr)
        { return false; }

        if (InTexture->CompressionSettings == TC_Masks ||
            InTexture->CompressionSettings == TC_Grayscale ||
            InTexture->CompressionSettings == TC_Alpha)
        { return true; }

        static const auto Suffixes = TArray<FString>{
            TEXT("_ORM"), TEXT("_RMA"), TEXT("_MRA"), TEXT("_Mask"), TEXT("_M"), TEXT("_R"), TEXT("_AO"),
            TEXT("_Rough"), TEXT("_Roughness"), TEXT("_Metallic")};

        for (const auto& Suffix : Suffixes)
        {
            if (InAssetName.EndsWith(Suffix, ESearchCase::IgnoreCase))
            { return true; }
        }

        return false;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Run_Checks(
            const FCkOptimizationDebugger_ScanContext& InContext,
            const FCkOptimizationDebugger_Thresholds& InThresholds,
            TArray<FCkOptimizationDebugger_FindingRow>& OutFindings)
        -> void
    {
        using namespace ck_optimization_debugger_scan;
        using namespace ck_optimization_debugger_thresholds;

        for (const auto& Asset : InContext.Textures)
        {
            auto* Texture = Cast<UTexture>(Asset.Asset.Get());

            if (Texture == nullptr)
            { continue; }

            const auto Target = Build_AssetTarget(Asset.Path, Asset.DisplayName);
            const auto Usage = Build_UsageSentence(Asset);

            auto Width = 0;
            auto Height = 0;
            const auto HasDimensions = TryGet_TextureDimensions(Texture, Width, Height);
            const auto LargestSide = FMath::Max(Width, Height);

            // ---- Texture.MaxSize ----
            if (HasDimensions && LargestSide > InThresholds.MaxTextureSize)
            {
                auto Finding = Build_Finding(FName{TEXT("Texture.MaxSize")},
                    Get_GraduatedSeverity(LargestSide, InThresholds.MaxTextureSize,
                        ECkOptimizationDebugger_Severity::Major),
                    ECkOptimizationDebugger_Category::Texture,
                    Target,
                    TEXT("Texture larger than the budget"),
                    ck::Format_UE(TEXT("{}x{} against a largest-side budget of {}. Memory scales with the square of the side, so one step over budget is four times the residency.{}"),
                        Width, Height, InThresholds.MaxTextureSize, Usage),
                    TEXT("Re-author at a smaller size, or cap it with a Maximum Texture Size / LOD Bias if the source has to stay big."));

                OutFindings.Add(MoveTemp(Finding));
            }

            // ---- Texture.NonPowerOfTwo ----
            // Only worth saying when it actually costs something: a non-power-of-two texture cannot be mipped or
            // streamed, so on a texture that would otherwise have mips it is a real defect rather than a style note.
            if (HasDimensions && (NOT Is_PowerOfTwo(Width) || NOT Is_PowerOfTwo(Height)))
            {
                auto Finding = Build_Finding(FName{TEXT("Texture.NonPowerOfTwo")},
                    ECkOptimizationDebugger_Severity::Minor,
                    ECkOptimizationDebugger_Category::Texture,
                    Target,
                    TEXT("Texture is not a power of two"),
                    ck::Format_UE(TEXT("{}x{} is not power-of-two on both axes. Mip generation and streaming both require power-of-two dimensions, so this texture is resident at full size for as long as anything referencing it is loaded.{}"),
                        Width, Height, Usage),
                    TEXT("Re-author to the nearest power-of-two size, unless this is deliberately an unstreamed UI or lookup texture."));

                OutFindings.Add(MoveTemp(Finding));
            }

            // ---- Texture.MissingMipmaps ----
            // UI textures legitimately ship without mips — they are drawn at one screen size and never minified —
            // which is why the group is excluded rather than the check being size-gated by a magic number.
#if WITH_EDITORONLY_DATA
            if (Texture->MipGenSettings == TMGS_NoMipmaps && Texture->LODGroup != TEXTUREGROUP_UI)
            {
                auto Finding = Build_Finding(FName{TEXT("Texture.MissingMipmaps")},
                    Get_GraduatedSeverity(LargestSide, InThresholds.MaxTextureSize,
                        ECkOptimizationDebugger_Severity::Major),
                    ECkOptimizationDebugger_Category::Texture,
                    Target,
                    TEXT("Texture has no mipmaps"),
                    ck::Format_UE(TEXT("Mip generation is set to NoMipmaps outside the UI texture group. Without mips the full {}x{} is sampled however few pixels the surface covers, which costs bandwidth AND aliases.{}"),
                        Width, Height, Usage),
                    TEXT("Set Mip Gen Settings back to FromTextureGroup unless this texture is genuinely never minified."));

                Finding.HasAutoFix = true;
                Finding.FixDescription = TEXT("Set Mip Gen Settings to FromTextureGroup.");

                OutFindings.Add(MoveTemp(Finding));
            }
#endif

            // ---- Texture.NormalMapCompression ----
            if (Is_NamedAsNormalMap(Asset.DisplayName) && Texture->CompressionSettings != TC_Normalmap)
            {
                auto Finding = Build_Finding(FName{TEXT("Texture.NormalMapCompression")},
                    ECkOptimizationDebugger_Severity::Major,
                    ECkOptimizationDebugger_Category::Texture,
                    Target,
                    TEXT("Normal map without normal-map compression"),
                    ck::Format_UE(TEXT("The asset name reads as a normal map but compression is not Normalmap. The wrong codec both costs more memory and quantizes the normal in the axis the eye notices most.{}"),
                        Usage),
                    TEXT("Set Compression Settings to Normalmap — or rename the asset if it is not one."));

                Finding.HasAutoFix = true;
                Finding.FixDescription = TEXT("Set this texture's Compression Settings to Normalmap.");

                OutFindings.Add(MoveTemp(Finding));
            }

            // ---- Texture.DataTextureSrgb ----
            if (Texture->SRGB != 0 && Is_DataTexture(Texture, Asset.DisplayName))
            {
                auto Finding = Build_Finding(FName{TEXT("Texture.DataTextureSrgb")},
                    ECkOptimizationDebugger_Severity::Major,
                    ECkOptimizationDebugger_Category::Texture,
                    Target,
                    TEXT("Data texture with sRGB enabled"),
                    ck::Format_UE(TEXT("This texture reads as packed data (mask / roughness / metallic / AO) but has sRGB on, so the sampler applies a colour transfer curve to numbers that are not colours. Every value the material reads is wrong, quietly.{}"),
                        Usage),
                    TEXT("Turn sRGB off. Only textures whose channels are literally colour should have it on."));

                Finding.HasAutoFix = true;
                Finding.FixDescription = TEXT("Disable sRGB on this texture.");

                OutFindings.Add(MoveTemp(Finding));
            }
        }
    }
}

#endif

// --------------------------------------------------------------------------------------------------------------------
