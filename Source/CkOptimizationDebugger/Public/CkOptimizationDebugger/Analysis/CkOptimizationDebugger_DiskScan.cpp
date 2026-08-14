#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_DiskScan.h"

#include "CkCore/Macros/CkMacros.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"

#include "Animation/AnimationAsset.h"
#include "Animation/Skeleton.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Sound/SoundBase.h"

#include "Containers/Map.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_diskscan
{
#if WITH_EDITOR
    /** Which family an asset belongs to.
     *
     *  `IsInstanceOf` with the default `EResolveClass::No` never loads a class to answer — an unloaded class simply
     *  fails every test and lands in `Other`. That is the right trade for a size report: the alternative is a scan
     *  that loads a few thousand classes to make a pie chart prettier.
     *
     *  Order matters where the families overlap. A `UMaterialInstanceConstant` is a `UMaterialInterface`, and a
     *  `UTexture2D` is a `UTexture`; the base classes below are chosen so no asset satisfies two of them. */
    auto
        Classify_Asset(
            const FAssetData& InAsset)
        -> ECkOptimizationDebugger_DiskCategory
    {
        if (InAsset.IsInstanceOf<UTexture>())
        { return ECkOptimizationDebugger_DiskCategory::Textures; }

        if (InAsset.IsInstanceOf<UStaticMesh>())
        { return ECkOptimizationDebugger_DiskCategory::StaticMeshes; }

        if (InAsset.IsInstanceOf<UMaterialInterface>() || InAsset.IsInstanceOf<UMaterialFunctionInterface>())
        { return ECkOptimizationDebugger_DiskCategory::Materials; }

        if (InAsset.IsInstanceOf<UBlueprint>())
        { return ECkOptimizationDebugger_DiskCategory::Blueprints; }

        if (InAsset.IsInstanceOf<UWorld>())
        { return ECkOptimizationDebugger_DiskCategory::Levels; }

        if (InAsset.IsInstanceOf<USoundBase>())
        { return ECkOptimizationDebugger_DiskCategory::Audio; }

        // Skeletal content as a whole. A project heavy in animation is heavy in skeletons and skeletal meshes too,
        // and three rows saying that is one answer split three ways.
        if (InAsset.IsInstanceOf<UAnimationAsset>() ||
            InAsset.IsInstanceOf<USkeleton>() ||
            InAsset.IsInstanceOf<USkeletalMesh>())
        { return ECkOptimizationDebugger_DiskCategory::Animation; }

        return ECkOptimizationDebugger_DiskCategory::Other;
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Which family a whole PACKAGE is attributed to, and whether that verdict came from the package's primary
     *  asset. `DiskSize` is per-package, so exactly one family may claim it. */
    struct FPackagePick
    {
        ECkOptimizationDebugger_DiskCategory Category = ECkOptimizationDebugger_DiskCategory::Other;

        // The verdict came from the asset whose name matches the package's own — the thing the Content Browser
        // shows for that package. A later non-primary asset must not overrule it.
        bool IsPrimary = false;
    };
#endif
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_disk
{
    auto
        Build_ProjectDiskBreakdown()
        -> FCkOptimizationDebugger_DiskBreakdown
    {
        auto Breakdown = FCkOptimizationDebugger_DiskBreakdown{};

#if WITH_EDITOR
        using namespace ck_optimization_debugger_diskscan;

        auto* Registry = IAssetRegistry::Get();

        if (Registry == nullptr)
        { return Breakdown; }

        Breakdown.IsAvailable = true;

        // A registry that is still indexing answers with what it has so far. Reported rather than waited on: a scan
        // is a question the user is standing in front of, and a floor they were told is a floor beats a spinner.
        Breakdown.WasStillIndexing = Registry->IsLoadingAssets();

        // Built locally rather than at namespace scope: an `FName` constructed during static initialization runs
        // before anything guarantees the name table is up.
        const auto GameRoot = FName{TEXT("/Game")};

        auto Assets = TArray<FAssetData>{};
        Registry->GetAssetsByPath(GameRoot, Assets, /*bRecursive*/ true, /*bIncludeOnlyOnDiskAssets*/ true);

        auto PickByPackage = TMap<FName, FPackagePick>{};
        PickByPackage.Reserve(Assets.Num());

        for (const auto& Asset : Assets)
        {
            // Redirectors are a cleanup question, not a content-budget one, and the bytes they cost belong on the
            // Cleanup page next to the action that removes them.
            if (Asset.IsRedirector())
            { continue; }

            if (Asset.PackageName.IsNone())
            { continue; }

            const auto Category = Classify_Asset(Asset);

            const auto ShortName = FPackageName::GetShortName(Asset.PackageName.ToString());
            const auto IsPrimary = Asset.AssetName.ToString().Equals(ShortName, ESearchCase::CaseSensitive);

            auto* Existing = PickByPackage.Find(Asset.PackageName);

            if (Existing == nullptr)
            {
                PickByPackage.Add(Asset.PackageName, FPackagePick{Category, IsPrimary});
                continue;
            }

            // First writer wins UNLESS the primary asset arrives later — registry iteration order is not the order
            // objects sit in a package, so the primary is claimed whenever it shows up rather than when it is first.
            if (IsPrimary && NOT Existing->IsPrimary)
            { *Existing = FPackagePick{Category, true}; }
        }

        auto BytesByCategory = TMap<ECkOptimizationDebugger_DiskCategory, int64>{};
        auto PackagesByCategory = TMap<ECkOptimizationDebugger_DiskCategory, int32>{};

        // One reused out-parameter rather than the batched `GetAssetPackageDatasCopy`: the batched form materializes
        // an optional per package up front, and a real project's `/Game` is tens of thousands of them.
        auto PackageData = FAssetPackageData{};

        for (const auto& Entry : PickByPackage)
        {
            if (Registry->TryGetAssetPackageData(Entry.Key, PackageData) != UE::AssetRegistry::EExists::Exists)
            { continue; }

            // An unsaved or never-written package reports no size. Counting it as zero is the honest answer; the
            // Cleanup page is where "dirty, so not on disk yet" belongs.
            if (PackageData.DiskSize <= 0)
            { continue; }

            BytesByCategory.FindOrAdd(Entry.Value.Category, int64{0}) += PackageData.DiskSize;
            ++PackagesByCategory.FindOrAdd(Entry.Value.Category, int32{0});

            Breakdown.TotalBytes += PackageData.DiskSize;
            ++Breakdown.PackageCount;
        }

        // Empty buckets are dropped: a row reading "Audio — 0 B" tells the reader nothing they did not already know
        // from its absence, and eight fixed rows would bury the two that matter.
        for (const auto Category : ck_optimization_debugger_model::Get_AllDiskCategories())
        {
            const auto* Bytes = BytesByCategory.Find(Category);

            if (Bytes == nullptr || *Bytes <= 0)
            { continue; }

            const auto* PackageCount = PackagesByCategory.Find(Category);

            auto Row = FCkOptimizationDebugger_DiskCategorySize{};
            Row.Category = Category;
            Row.TotalBytes = *Bytes;
            Row.PackageCount = PackageCount != nullptr ? *PackageCount : 0;

            Breakdown.Categories.Add(Row);
        }

        // Largest first, with the enum value as the tie-break. `TArray::Sort` is unstable, so two families that
        // happen to weigh the same would otherwise swap rows between two identical scans.
        Breakdown.Categories.Sort([](const FCkOptimizationDebugger_DiskCategorySize& InLhs,
                                     const FCkOptimizationDebugger_DiskCategorySize& InRhs)
        {
            if (InLhs.TotalBytes != InRhs.TotalBytes)
            { return InLhs.TotalBytes > InRhs.TotalBytes; }

            return static_cast<uint8>(InLhs.Category) < static_cast<uint8>(InRhs.Category);
        });
#endif

        return Breakdown;
    }
}

// --------------------------------------------------------------------------------------------------------------------
