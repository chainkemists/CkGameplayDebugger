#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Texture.h"

#if WITH_EDITOR

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureDefines.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Misc/CoreMisc.h"

// --------------------------------------------------------------------------------------------------------------------

// File-local helpers in the module's own named namespace — this module compiles unity, and an anonymous namespace
// would collide with the identically-shaped helpers in the other check files.
namespace ck_optimization_debugger_checks_texture
{
    auto
        TryGet_TextureDims(
            UTexture* InTexture)
        -> TOptional<FCkOptimizationDebugger_TextureDims>
    {
        if (InTexture == nullptr)
        { return {}; }

        auto Dims = FCkOptimizationDebugger_TextureDims{};

#if WITH_EDITORONLY_DATA
        if (InTexture->Source.IsValid())
        {
            // FTextureSource sizes are int64 in 5.7 — narrowed deliberately: a texture dimension past int32 is not
            // a thing this tool has to be right about.
            Dims.SourceWidth = static_cast<int32>(InTexture->Source.GetSizeX());
            Dims.SourceHeight = static_cast<int32>(InTexture->Source.GetSizeY());

            const auto* RunningPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();

            if (RunningPlatform == nullptr)
            {
                // Nothing to ask. It should not happen in an editor session, but a null deref inside an audit tool
                // is worse than a conservative answer: judge by the source, as this check did before.
                Dims.BuiltWidth = Dims.SourceWidth;
                Dims.BuiltHeight = Dims.SourceHeight;

                return Dims;
            }

            // Reads the serialized source header and the build settings, nothing else (engine Texture.cpp:4048), so
            // it cannot start a texture build — which is the whole reason the source path exists.
            InTexture->GetBuiltTextureSize(RunningPlatform, Dims.BuiltWidth, Dims.BuiltHeight);

            return Dims;
        }
#endif

        // Reached only by a texture with no source at all — a dynamic or runtime-created one. Nothing built it, so
        // there is no build size to ask for and the platform data is the only answer there is.
        if (auto* Texture2D = Cast<UTexture2D>(InTexture))
        {
            Dims.SourceWidth = Texture2D->GetSizeX();
            Dims.SourceHeight = Texture2D->GetSizeY();

            Dims.BuiltWidth = Dims.SourceWidth;
            Dims.BuiltHeight = Dims.SourceHeight;

            if (Dims.SourceWidth > 0 && Dims.SourceHeight > 0)
            { return Dims; }
        }

        return {};
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

            const auto Dims = TryGet_TextureDims(Texture);
            const auto HasDimensions = Dims.IsSet();

            // Every size judgement below is about what the build PRODUCES. A 4096 source that Resize During Build or
            // Maximum Texture Size caps to 2048 already ships inside a 2048 budget, and flagging it would report an
            // artist for having done the thing the finding asks for.
            const auto Width = HasDimensions ? Dims->BuiltWidth : 0;
            const auto Height = HasDimensions ? Dims->BuiltHeight : 0;
            const auto LargestSide = FMath::Max(Width, Height);

            const auto IsCappedAtBuild = HasDimensions &&
                (Dims->BuiltWidth != Dims->SourceWidth || Dims->BuiltHeight != Dims->SourceHeight);

            // ---- Texture.MaxSize ----
            if (HasDimensions && LargestSide > InThresholds.MaxTextureSize)
            {
                const auto Graded = Get_Graded(LargestSide, InThresholds.MaxTextureSize,
                    ECkOptimizationDebugger_Severity::Major);

                // Naming both sizes is what stops the reader re-opening a texture they already capped: the sentence
                // has to say the cap was seen and the result is still over.
                const auto Explanation = IsCappedAtBuild
                    ? ck::Format_UE(TEXT("Builds to {}x{} from a {}x{} source, against a largest-side budget of {}. Memory scales with the square of the side, so one step over budget is four times the residency.{}"),
                        Width, Height, Dims->SourceWidth, Dims->SourceHeight, InThresholds.MaxTextureSize, Usage)
                    : ck::Format_UE(TEXT("{}x{} against a largest-side budget of {}. Memory scales with the square of the side, so one step over budget is four times the residency.{}"),
                        Width, Height, InThresholds.MaxTextureSize, Usage);

                auto Finding = Build_Finding(FName{TEXT("Texture.MaxSize")},
                    Graded.Severity,
                    ECkOptimizationDebugger_Category::Texture,
                    Target,
                    TEXT("Texture larger than the budget"),
                    Explanation,
                    TEXT("Re-author at a smaller size, or cap it with a Maximum Texture Size / LOD Bias if the source has to stay big."));

                Finding.BudgetRatio = Graded.BudgetRatio;

                OutFindings.Add(MoveTemp(Finding));
            }

            // ---- Texture.NonPowerOfTwo ----
            // Only worth saying when it actually costs something: a non-power-of-two texture cannot be mipped or
            // streamed, so on a texture that would otherwise have mips it is a real defect rather than a style note.
            // A non-POT SOURCE that the build pads or resizes to POT costs nothing, which is why the built size is
            // what is tested.
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
                const auto Graded = Get_Graded(LargestSide, InThresholds.MaxTextureSize,
                    ECkOptimizationDebugger_Severity::Major);

                auto Finding = Build_Finding(FName{TEXT("Texture.MissingMipmaps")},
                    Graded.Severity,
                    ECkOptimizationDebugger_Category::Texture,
                    Target,
                    TEXT("Texture has no mipmaps"),
                    ck::Format_UE(TEXT("Mip generation is set to NoMipmaps outside the UI texture group. Without mips the full {}x{} is sampled however few pixels the surface covers, which costs bandwidth AND aliases.{}"),
                        Width, Height, Usage),
                    TEXT("Set Mip Gen Settings back to FromTextureGroup unless this texture is genuinely never minified."));

                Finding.BudgetRatio = Graded.BudgetRatio;

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
