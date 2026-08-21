#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_ProjectScan.h"

#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_ScanContext.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#if WITH_EDITOR
#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Mesh.h"
#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Texture.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureDefines.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_ProjectScanState::
    Get_TotalSteps() const
    -> int32
{
    return Assets.Num() + DeepQueue.Num();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_ProjectScanState::
    Get_CompletedSteps() const
    -> int32
{
    return NextRegistryIndex + NextDeepIndex;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_ProjectScanState::
    Get_Progress() const
    -> float
{
    const auto Total = Get_TotalSteps();

    if (Total <= 0)
    { return 0.0f; }

    // The deep queue GROWS while the registry pass walks, so the denominator moves. Clamped rather than allowed to
    // read 1.0 early: a bar that reaches full while work remains is a bar the reader stops believing.
    return FMath::Clamp(static_cast<float>(Get_CompletedSteps()) / static_cast<float>(Total), 0.0f, 1.0f);
}

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity.
namespace ck_optimization_debugger_project_scan_impl
{
    auto
        Is_PowerOfTwo(
            int32 InValue)
        -> bool
    {
        return InValue > 0 && (InValue & (InValue - 1)) == 0;
    }

#if WITH_EDITOR
    // ----------------------------------------------------------------------------------------------------------------

    /** One numeric registry tag, or -1 when it is absent or unreadable. The absence is the point: a tag a check
     *  reads as zero would report every asset saved by an older editor as clean. */
    auto
        Get_IntTag(
            const FAssetData& InAssetData,
            const FName& InTagName)
        -> int32
    {
        auto Value = FString{};

        if (NOT InAssetData.GetTagValue(InTagName, Value))
        { return -1; }

        if (NOT Value.IsNumeric())
        { return -1; }

        return FCString::Atoi(*Value);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_StringTag(
            const FAssetData& InAssetData,
            const FName& InTagName)
        -> FString
    {
        auto Value = FString{};

        InAssetData.GetTagValue(InTagName, Value);

        return Value;
    }
#endif
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_project_scan
{
    using namespace ck_optimization_debugger_project_scan_impl;

    auto
        Get_IsKnown(
            int32 InValue)
        -> bool
    {
        return InValue >= 0;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_HeaviestMeshes(
            const TArray<FCkOptimizationDebugger_AssetFacts>& InAssets,
            int32 InTopCount)
        -> TArray<FCkOptimizationDebugger_AssetFacts>
    {
        auto Ranked = InAssets.FilterByPredicate([](const FCkOptimizationDebugger_AssetFacts& InFacts) -> bool
        {
            // Unknown is not zero. A mesh whose tag nobody wrote is absent from the table rather than bottom of it.
            return InFacts.IsStaticMesh && Get_IsKnown(InFacts.TriangleCount);
        });

        Ranked.Sort([](const FCkOptimizationDebugger_AssetFacts& InLhs,
                       const FCkOptimizationDebugger_AssetFacts& InRhs) -> bool
        {
            if (InLhs.TriangleCount != InRhs.TriangleCount)
            { return InLhs.TriangleCount > InRhs.TriangleCount; }

            // Path as the final tie-break: `TArray::Sort` is unstable, so two meshes of equal density would
            // otherwise swap rows between two identical scans.
            return InLhs.Path.ToString().Compare(InRhs.Path.ToString(), ESearchCase::CaseSensitive) < 0;
        });

        if (InTopCount > 0 && Ranked.Num() > InTopCount)
        { Ranked.SetNum(InTopCount); }

        return Ranked;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Run_RegistryChecks(
            const FCkOptimizationDebugger_AssetFacts& InFacts,
            const FCkOptimizationDebugger_Thresholds& InThresholds,
            TArray<FCkOptimizationDebugger_FindingRow>& OutFindings)
        -> void
    {
        using namespace ck_optimization_debugger_scan;
        using namespace ck_optimization_debugger_thresholds;

        const auto Target = Build_AssetTarget(InFacts.Path, InFacts.DisplayName);

        // The sentence every project finding ends with. A project-scan finding names an asset nobody placed in an
        // open level, and saying so is what keeps it from reading as a level finding whose level went missing.
        const auto Provenance = FString{TEXT(" Found by the project scan, which reads the asset registry — this "
            "asset may or may not be placed in a level.")};

        if (InFacts.IsStaticMesh)
        {
            const auto TrianglesKnown = Get_IsKnown(InFacts.TriangleCount);

            // ---- Mesh.TriangleBudget ----
            if (TrianglesKnown && InFacts.TriangleCount > InThresholds.MaxTriangleCountLOD0)
            {
                const auto Graded = Get_Graded(InFacts.TriangleCount, InThresholds.MaxTriangleCountLOD0,
                    ECkOptimizationDebugger_Severity::Major);

                auto Finding = Build_Finding(FName{TEXT("Mesh.TriangleBudget")},
                    Graded.Severity,
                    ECkOptimizationDebugger_Category::Mesh,
                    Target,
                    TEXT("LOD0 triangle count over budget"),
                    ck::Format_UE(TEXT("LOD0 carries {} triangles against a budget of {}.{}"),
                        InFacts.TriangleCount, InThresholds.MaxTriangleCountLOD0, Provenance),
                    TEXT("Decimate the source mesh, split it into separate assets, or enable Nanite if this is a static prop."));

                Finding.BudgetRatio = Graded.BudgetRatio;

                OutFindings.Add(MoveTemp(Finding));
            }

            // ---- Mesh.MissingLods ----
            if (TrianglesKnown && Get_IsKnown(InFacts.LodCount)
                && InFacts.LodCount <= 1 && InFacts.NaniteEnabled == 0
                && InFacts.TriangleCount >= InThresholds.MinTrianglesForNanite)
            {
                const auto Graded = Get_Graded(InFacts.TriangleCount, InThresholds.MaxTriangleCountLOD0,
                    ECkOptimizationDebugger_Severity::Major);

                auto Finding = Build_Finding(FName{TEXT("Mesh.MissingLods")},
                    Graded.Severity,
                    ECkOptimizationDebugger_Category::Mesh,
                    Target,
                    TEXT("Dense mesh with a single LOD"),
                    ck::Format_UE(TEXT("{} triangles across exactly one LOD, with Nanite off.{}"),
                        InFacts.TriangleCount, Provenance),
                    TEXT("Generate LODs, or enable Nanite if the mesh is static and opaque."));

                Finding.BudgetRatio = Graded.BudgetRatio;
                Finding.HasAutoFix = true;
                Finding.FixDescription = TEXT("Generate a default LOD chain for this mesh through the engine's own LOD reduction settings.");

                OutFindings.Add(MoveTemp(Finding));
            }

            // ---- Mesh.NaniteCandidate ----
            if (TrianglesKnown && InFacts.NaniteEnabled == 0
                && InFacts.TriangleCount >= InThresholds.MinTrianglesForNanite)
            {
                const auto Graded = Get_Graded(InFacts.TriangleCount, InThresholds.MinTrianglesForNanite,
                    ECkOptimizationDebugger_Severity::Minor);

                auto Finding = Build_Finding(FName{TEXT("Mesh.NaniteCandidate")},
                    Graded.Severity,
                    ECkOptimizationDebugger_Category::Mesh,
                    Target,
                    TEXT("Dense mesh with Nanite off"),
                    ck::Format_UE(TEXT("{} triangles with Nanite disabled.{}"), InFacts.TriangleCount, Provenance),
                    TEXT("Enable Nanite if the mesh is static and opaque."));

                Finding.BudgetRatio = Graded.BudgetRatio;
                Finding.HasAutoFix = true;
                Finding.FixDescription = TEXT("Enable Nanite on this mesh.");

                OutFindings.Add(MoveTemp(Finding));
            }

            // ---- Mesh.NaniteOnLowPoly ----
            if (TrianglesKnown && InFacts.NaniteEnabled == 1
                && InFacts.TriangleCount < InThresholds.MaxTrianglesForNaniteWarning)
            {
                auto Finding = Build_Finding(FName{TEXT("Mesh.NaniteOnLowPoly")},
                    ECkOptimizationDebugger_Severity::Minor,
                    ECkOptimizationDebugger_Category::Mesh,
                    Target,
                    TEXT("Nanite on a low-poly mesh"),
                    ck::Format_UE(TEXT("Nanite is on for a mesh of only {} triangles.{}"),
                        InFacts.TriangleCount, Provenance),
                    TEXT("Turn Nanite off — below this density it costs more than it saves."));

                Finding.HasAutoFix = true;
                Finding.FixDescription = TEXT("Disable Nanite on this mesh.");

                OutFindings.Add(MoveTemp(Finding));
            }

            // ---- Mesh.CollisionPrimitiveCount ----
            if (Get_IsKnown(InFacts.CollisionPrimitiveCount)
                && InFacts.CollisionPrimitiveCount > InThresholds.MaxCollisionPrimitives)
            {
                const auto Graded = Get_Graded(InFacts.CollisionPrimitiveCount, InThresholds.MaxCollisionPrimitives,
                    ECkOptimizationDebugger_Severity::Minor);

                auto Finding = Build_Finding(FName{TEXT("Mesh.CollisionPrimitiveCount")},
                    Graded.Severity,
                    ECkOptimizationDebugger_Category::Mesh,
                    Target,
                    TEXT("Many simple collision primitives"),
                    ck::Format_UE(TEXT("{} simple collision primitives against a budget of {}.{}"),
                        InFacts.CollisionPrimitiveCount, InThresholds.MaxCollisionPrimitives, Provenance),
                    TEXT("Simplify the collision setup — every primitive is tested separately."));

                Finding.BudgetRatio = Graded.BudgetRatio;

                OutFindings.Add(MoveTemp(Finding));
            }

            // ---- Mesh.ComplexCollision ----
            // The registry writes the enum's own name, so this compares against that spelling rather than against a
            // display string somebody might localise.
            if (InFacts.CollisionComplexity.Contains(TEXT("UseComplexAsSimple"), ESearchCase::IgnoreCase))
            {
                auto Finding = Build_Finding(FName{TEXT("Mesh.ComplexCollision")},
                    ECkOptimizationDebugger_Severity::Major,
                    ECkOptimizationDebugger_Category::Mesh,
                    Target,
                    TEXT("Collision uses the render mesh"),
                    ck::Format_UE(TEXT("Collision complexity is Use Complex As Simple, so every query traces the render geometry.{}"),
                        Provenance),
                    TEXT("Add simple collision and set complexity to Simple And Complex."));

                Finding.HasAutoFix = true;
                Finding.FixDescription = TEXT("Set collision complexity to Simple And Complex, adding a box primitive if the mesh has none.");

                OutFindings.Add(MoveTemp(Finding));
            }

            // ---- Material.SlotCount ----
            if (Get_IsKnown(InFacts.MaterialSlotCount)
                && InFacts.MaterialSlotCount > InThresholds.MaxMaterialSlots)
            {
                const auto Graded = Get_Graded(InFacts.MaterialSlotCount, InThresholds.MaxMaterialSlots,
                    ECkOptimizationDebugger_Severity::Major);

                auto Finding = Build_Finding(FName{TEXT("Material.SlotCount")},
                    Graded.Severity,
                    ECkOptimizationDebugger_Category::Material,
                    Target,
                    TEXT("Many material slots"),
                    ck::Format_UE(TEXT("{} material slots against a budget of {}. Each slot is a separate draw call per placement.{}"),
                        InFacts.MaterialSlotCount, InThresholds.MaxMaterialSlots, Provenance),
                    TEXT("Merge materials, or split the mesh so each piece carries fewer."));

                Finding.BudgetRatio = Graded.BudgetRatio;

                OutFindings.Add(MoveTemp(Finding));
            }
        }

        if (InFacts.IsTexture)
        {
            const auto DimensionsKnown = Get_IsKnown(InFacts.Width) && Get_IsKnown(InFacts.Height);
            const auto LargestSide = FMath::Max(InFacts.Width, InFacts.Height);

            // ---- Texture.MaxSize ----
            if (DimensionsKnown && LargestSide > InThresholds.MaxTextureSize)
            {
                const auto Graded = Get_Graded(LargestSide, InThresholds.MaxTextureSize,
                    ECkOptimizationDebugger_Severity::Major);

                auto Finding = Build_Finding(FName{TEXT("Texture.MaxSize")},
                    Graded.Severity,
                    ECkOptimizationDebugger_Category::Texture,
                    Target,
                    TEXT("Texture over the size budget"),
                    ck::Format_UE(TEXT("{}x{} against a budget of {}.{}"),
                        InFacts.Width, InFacts.Height, InThresholds.MaxTextureSize, Provenance),
                    TEXT("Re-author smaller, or set a Max Texture Size on the asset."));

                Finding.BudgetRatio = Graded.BudgetRatio;

                OutFindings.Add(MoveTemp(Finding));
            }

            // ---- Texture.NonPowerOfTwo ----
            if (DimensionsKnown && (NOT Is_PowerOfTwo(InFacts.Width) || NOT Is_PowerOfTwo(InFacts.Height)))
            {
                OutFindings.Add(Build_Finding(FName{TEXT("Texture.NonPowerOfTwo")},
                    ECkOptimizationDebugger_Severity::Minor,
                    ECkOptimizationDebugger_Category::Texture,
                    Target,
                    TEXT("Texture is not a power of two"),
                    ck::Format_UE(TEXT("{}x{}. Non-power-of-two textures cannot stream or use every compression format.{}"),
                        InFacts.Width, InFacts.Height, Provenance),
                    TEXT("Re-author to a power of two, or set a Power Of Two Mode on the asset.")));
            }
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

#if WITH_EDITOR

    auto
        Parse_AssetFacts(
            const FAssetData& InAssetData)
        -> FCkOptimizationDebugger_AssetFacts
    {
        auto Facts = FCkOptimizationDebugger_AssetFacts{};
        Facts.Path = InAssetData.GetSoftObjectPath();
        Facts.DisplayName = InAssetData.AssetName.ToString();

        // `EResolveClass::No` by default — a class nobody has loaded stays unclassified rather than being loaded to
        // find out, which is the whole fence this pass exists behind.
        Facts.IsStaticMesh = InAssetData.IsInstanceOf<UStaticMesh>();
        Facts.IsTexture = InAssetData.IsInstanceOf<UTexture>();
        Facts.IsSound = InAssetData.IsInstanceOf<USoundBase>();

        if (Facts.IsStaticMesh)
        {
            Facts.TriangleCount = Get_IntTag(InAssetData, FName{TEXT("Triangles")});
            Facts.LodCount = Get_IntTag(InAssetData, FName{TEXT("LODs")});
            Facts.CollisionPrimitiveCount = Get_IntTag(InAssetData, FName{TEXT("CollisionPrims")});
            Facts.MaterialSlotCount = Get_IntTag(InAssetData, FName{TEXT("Materials")});
            Facts.CollisionComplexity = Get_StringTag(InAssetData, FName{TEXT("CollisionComplexity")});

            const auto NaniteTag = Get_StringTag(InAssetData, FName{TEXT("NaniteEnabled")});

            if (NaniteTag.Equals(TEXT("True"), ESearchCase::IgnoreCase))
            { Facts.NaniteEnabled = 1; }
            else if (NaniteTag.Equals(TEXT("False"), ESearchCase::IgnoreCase))
            { Facts.NaniteEnabled = 0; }

            // A Nanite mesh may carry materials that do not declare the usage, and the registry cannot say — the
            // answer is a predicate over each slot's base material.
            Facts.NeedsDeepPass = Facts.NaniteEnabled == 1;
        }

        if (Facts.IsTexture)
        {
            // "1024x1024" — the engine's own dimensional tag.
            const auto Dimensions = Get_StringTag(InAssetData, FName{TEXT("Dimensions")});

            auto WidthText = FString{};
            auto HeightText = FString{};

            if (Dimensions.Split(TEXT("x"), &WidthText, &HeightText))
            {
                if (WidthText.IsNumeric())
                { Facts.Width = FCString::Atoi(*WidthText); }

                if (HeightText.IsNumeric())
                { Facts.Height = FCString::Atoi(*HeightText); }
            }

            Facts.MipGenSettings = Get_StringTag(InAssetData, FName{TEXT("MipGenSettings")});

            // The mip check's OTHER half is the texture group, and the sRGB and normal-map checks are predicates
            // over the compression setting — none of which the registry carries.
            Facts.NeedsDeepPass = true;
        }

        if (Facts.IsSound)
        {
            // Every audio question needs the object: a sound class, a duration, a streaming flag. None of them is
            // a registry tag, so a sound is always a deep-pass asset or it is not checked at all.
            Facts.NeedsDeepPass = true;
        }

        return Facts;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Collect_ProjectAssets()
        -> TArray<FCkOptimizationDebugger_AssetFacts>
    {
        auto Facts = TArray<FCkOptimizationDebugger_AssetFacts>{};

        auto* AssetRegistry = IAssetRegistry::Get();

        if (AssetRegistry == nullptr)
        { return Facts; }

        auto Assets = TArray<FAssetData>{};

        constexpr auto Recursive = true;
        constexpr auto IncludeOnlyOnDiskAssets = true;

        AssetRegistry->GetAssetsByPath(FName{TEXT("/Game")}, Assets, Recursive, IncludeOnlyOnDiskAssets);

        for (const auto& AssetData : Assets)
        {
            auto Parsed = Parse_AssetFacts(AssetData);

            if (NOT Parsed.IsStaticMesh && NOT Parsed.IsTexture && NOT Parsed.IsSound)
            { continue; }

            Facts.Add(MoveTemp(Parsed));
        }

        // Sorted by path, for the same reason every other table here is: registry iteration order is a detail, and
        // a list that reordered itself between two identical scans is one nobody can diff.
        Facts.Sort([](const FCkOptimizationDebugger_AssetFacts& InLhs, const FCkOptimizationDebugger_AssetFacts& InRhs)
        {
            return InLhs.Path.ToString().Compare(InRhs.Path.ToString(), ESearchCase::CaseSensitive) < 0;
        });

        return Facts;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Run_DeepChecks(
            const FCkOptimizationDebugger_AssetFacts& InFacts,
            const FCkOptimizationDebugger_Thresholds& InThresholds,
            TArray<FCkOptimizationDebugger_FindingRow>& OutFindings)
        -> void
    {
        using namespace ck_optimization_debugger_scan;

        if (InFacts.Path.IsNull())
        { return; }

        // The deep pass is the ONE place this scan loads anything, and it only reaches assets the registry pass
        // flagged. Resolve first: an asset already in memory costs nothing.
        auto* Object = InFacts.Path.ResolveObject();

        if (Object == nullptr)
        { Object = InFacts.Path.TryLoad(); }

        if (ck::Is_NOT_Valid(Object))
        { return; }

        const auto Target = Build_AssetTarget(InFacts.Path, InFacts.DisplayName);
        const auto Provenance = FString{TEXT(" Found by the project scan.")};

        if (auto* Texture = Cast<UTexture>(Object))
        {
            // ---- Texture.MissingMipmaps ----
            // BOTH halves, the second of which is why this needed a load at all: a texture in the UI group is
            // legitimately mipless, and generating mips for it would undo a deliberate authoring decision.
            if (Texture->MipGenSettings == TMGS_NoMipmaps && Texture->LODGroup != TEXTUREGROUP_UI)
            {
                auto Finding = Build_Finding(FName{TEXT("Texture.MissingMipmaps")},
                    ECkOptimizationDebugger_Severity::Major,
                    ECkOptimizationDebugger_Category::Texture,
                    Target,
                    TEXT("Texture ships without mipmaps"),
                    ck::Format_UE(TEXT("Mip generation is off and the texture is not in the UI group, so it is sampled at full resolution however few pixels it covers.{}"),
                        Provenance),
                    TEXT("Set Mip Gen Settings to FromTextureGroup."));

                Finding.HasAutoFix = true;
                Finding.FixDescription = TEXT("Set mip generation to FromTextureGroup so the texture rebuilds with mips.");

                OutFindings.Add(MoveTemp(Finding));
            }

            // ---- Texture.DataTextureSrgb ----
            // The check's own exported predicate, never a second copy of the rule.
            if (Texture->SRGB != 0
                && ck_optimization_debugger_checks_texture::Is_DataTexture(Texture, Texture->GetName()))
            {
                auto Finding = Build_Finding(FName{TEXT("Texture.DataTextureSrgb")},
                    ECkOptimizationDebugger_Severity::Major,
                    ECkOptimizationDebugger_Category::Texture,
                    Target,
                    TEXT("Data texture with sRGB on"),
                    ck::Format_UE(TEXT("This reads as packed data rather than colour, and sRGB applies a gamma curve to it.{}"),
                        Provenance),
                    TEXT("Turn sRGB off."));

                Finding.HasAutoFix = true;
                Finding.FixDescription = TEXT("Turn sRGB off on this texture.");

                OutFindings.Add(MoveTemp(Finding));
            }
        }

        if (const auto* Sound = Cast<USoundBase>(Object))
        {
            // ---- Audio.MissingSoundClass ----
            // A sound with no class bypasses the project's whole mix: no group volume, no ducking, no submix
            // routing. It plays, which is why nothing ever reports it.
            if (Sound->SoundClassObject == nullptr)
            {
                OutFindings.Add(Build_Finding(FName{TEXT("Audio.MissingSoundClass")},
                    ECkOptimizationDebugger_Severity::Major,
                    ECkOptimizationDebugger_Category::Audio,
                    Target,
                    TEXT("Sound has no Sound Class"),
                    ck::Format_UE(TEXT("No Sound Class is assigned, so this sound is outside the project's mix — group volume, ducking and submix routing all pass it by.{}"),
                        Provenance),
                    TEXT("Assign the Sound Class this sound belongs to.")));
            }

            // ---- Audio.LongSoundNotStreaming ----
            if (const auto* Wave = Cast<USoundWave>(Object))
            {
                const auto DurationSeconds = Wave->GetDuration();
                const auto Budget = static_cast<float>(InThresholds.MinSoundDurationForStreaming);

                // `Duration` is `INDEFINITELY_LOOPING_DURATION` on a procedural wave, which is not a length and
                // must not be compared against a budget as though it were.
                const auto HasRealDuration = DurationSeconds > 0.0f && DurationSeconds < INDEFINITELY_LOOPING_DURATION;

                if (HasRealDuration && Budget > 0.0f && DurationSeconds > Budget && NOT Wave->IsStreaming())
                {
                    const auto Graded = ck_optimization_debugger_thresholds::Get_Graded(
                        FMath::RoundToInt(DurationSeconds), InThresholds.MinSoundDurationForStreaming,
                        ECkOptimizationDebugger_Severity::Major);

                    auto Finding = Build_Finding(FName{TEXT("Audio.LongSoundNotStreaming")},
                        Graded.Severity,
                        ECkOptimizationDebugger_Category::Audio,
                        Target,
                        TEXT("Long sound is not set to stream"),
                        ck::Format_UE(TEXT("{} seconds against a {}-second streaming threshold, with streaming off — the whole wave is decoded into memory when it loads.{}"),
                            FMath::RoundToInt(DurationSeconds), InThresholds.MinSoundDurationForStreaming, Provenance),
                        TEXT("Turn streaming on for this wave, or shorten it."));

                    Finding.BudgetRatio = Graded.BudgetRatio;

                    OutFindings.Add(MoveTemp(Finding));
                }
            }

            return;
        }

        if (const auto* Mesh = Cast<UStaticMesh>(Object))
        {
            // ---- Mesh.NaniteMaterialIncompatible ----
            if (NOT Mesh->GetNaniteSettings().bEnabled)
            { return; }

            auto OffendingNames = TArray<FString>{};

            for (const auto& StaticMaterial : Mesh->GetStaticMaterials())
            {
                if (NOT ck_optimization_debugger_checks_mesh::Is_NaniteIncompatible(StaticMaterial.MaterialInterface))
                { continue; }

                if (const auto* Material = StaticMaterial.MaterialInterface.Get())
                { OffendingNames.AddUnique(Material->GetName()); }
            }

            if (OffendingNames.IsEmpty())
            { return; }

            OffendingNames.Sort();

            auto Finding = Build_Finding(FName{TEXT("Mesh.NaniteMaterialIncompatible")},
                ECkOptimizationDebugger_Severity::Major,
                ECkOptimizationDebugger_Category::Mesh,
                Target,
                TEXT("Nanite mesh with a material that does not declare the usage"),
                ck::Format_UE(TEXT("Nanite is on, but {} does not declare Used With Nanite.{}"),
                    FString::Join(OffendingNames, TEXT(", ")), Provenance),
                TEXT("Tick Used With Nanite on the offending base materials."));

            Finding.HasAutoFix = true;
            Finding.FixDescription = TEXT("Set Used With Nanite on each offending base material. This queues a shader compile.");

            OutFindings.Add(MoveTemp(Finding));
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Advance(
            FCkOptimizationDebugger_ProjectScanState& InOutState,
            int32 InBudget)
        -> bool
    {
        auto Remaining = FMath::Max(InBudget, 1);

        // The registry pass first, WHOLE. It opens nothing, so it is cheap enough to finish before anything is
        // loaded — which means the reader sees most of the answer before the expensive half starts.
        while (Remaining > 0 && InOutState.Assets.IsValidIndex(InOutState.NextRegistryIndex))
        {
            const auto& Facts = InOutState.Assets[InOutState.NextRegistryIndex];

            Run_RegistryChecks(Facts, InOutState.Thresholds, InOutState.Findings);

            if (Facts.NeedsDeepPass)
            { InOutState.DeepQueue.Add(InOutState.NextRegistryIndex); }

            ++InOutState.NextRegistryIndex;
            --Remaining;
        }

        if (InOutState.Assets.IsValidIndex(InOutState.NextRegistryIndex))
        { return true; }

        while (Remaining > 0 && InOutState.DeepQueue.IsValidIndex(InOutState.NextDeepIndex))
        {
            const auto AssetIndex = InOutState.DeepQueue[InOutState.NextDeepIndex];

            if (InOutState.Assets.IsValidIndex(AssetIndex))
            {
                Run_DeepChecks(InOutState.Assets[AssetIndex], InOutState.Thresholds, InOutState.Findings);
                ++InOutState.DeepLoadedCount;
            }

            ++InOutState.NextDeepIndex;
            --Remaining;
        }

        return InOutState.DeepQueue.IsValidIndex(InOutState.NextDeepIndex);
    }

#endif
}

// --------------------------------------------------------------------------------------------------------------------
