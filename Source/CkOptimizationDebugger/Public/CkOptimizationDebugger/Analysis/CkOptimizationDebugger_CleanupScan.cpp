#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_CleanupScan.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"

#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/Package.h"

#include "Containers/Map.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_cleanup_impl
{
    // The progress dialog's steps. Named rather than counted inline so adding a pass cannot silently make the bar
    // finish early.
    constexpr auto k_ScanStepCount = 4.0f;

#if WITH_EDITOR
    /** What one `/Game` package's primary asset is, plus the size the package weighs on disk.
     *
     *  Package-granular for the same reason the disk breakdown is: `FAssetPackageData::DiskSize` is the size of a
     *  package, not of one object inside it. Exactly one asset per package is described here — the primary one, whose
     *  name matches the package's own, which is what the Content Browser shows for that package. */
    struct FPackageEntry
    {
        FAssetData Asset;

        int64 DiskSizeBytes = 0;

        // The verdict came from the asset whose name matches the package's. A later non-primary asset must not
        // overrule it — registry iteration order is not the order objects sit in a package.
        bool IsPrimary = false;
    };

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ClassName(
            const FAssetData& InAsset)
        -> FString
    {
        return InAsset.AssetClassPath.GetAssetName().ToString();
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Whether a package name is one this pass has anything to say about. `/Game` only, and never the transient
     *  package: a scan of project content that reported the editor's own scratch would be reporting on itself. */
    auto
        Is_GameContentPackage(
            const FString& InPackageName)
        -> bool
    {
        return InPackageName.StartsWith(TEXT("/Game/"), ESearchCase::CaseSensitive);
    }
#endif
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_cleanup
{
    auto
        Get_AlwaysRootedClassNames()
        -> TArray<FString>
    {
        // Short and alphabetical on purpose. Every entry here is a class the on-disk reference graph structurally
        // cannot see a reference to, NOT a class somebody decided was important:
        //
        //  - `World` / `Level` — a map is named by project settings, by a streaming volume or by code, never by a
        //    package dependency, so every level in the project would otherwise report as unreferenced.
        //  - `PrimaryAssetLabel` — exists precisely to be DISCOVERED by the asset manager rather than referenced.
        //  - `DataAsset` / `PrimaryDataAsset` — the two shapes a project reaches for by path from config or from
        //    code. A false positive here asks the reader to delete their game's data.
        //  - `AssetManagerSettings` / `DeveloperSettings`-shaped configuration lands in `/Config`, not `/Game`, so it
        //    is deliberately absent — this list is not a place to add anything that merely feels risky.
        return {
            FString{TEXT("DataAsset")},
            FString{TEXT("Level")},
            FString{TEXT("PrimaryAssetLabel")},
            FString{TEXT("PrimaryDataAsset")},
            FString{TEXT("World")}};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Is_AlwaysRootedClassName(
            const FString& InClassName)
        -> bool
    {
        for (const auto& Name : Get_AlwaysRootedClassNames())
        {
            if (Name.Equals(InClassName, ESearchCase::IgnoreCase))
            { return true; }
        }

        return false;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Run_CleanupScan()
        -> FCkOptimizationDebugger_CleanupScanResult
    {
        auto Result = FCkOptimizationDebugger_CleanupScanResult{};

#if WITH_EDITOR
        using namespace ck_optimization_debugger_cleanup_impl;
        using namespace ck_optimization_debugger_model;

        auto* Registry = IAssetRegistry::Get();

        if (Registry == nullptr)
        {
            Result.RequiresEditor = true;
            return Result;
        }

        // Reported rather than waited on, exactly as the disk breakdown reports it: a scan is a question the reader is
        // standing in front of, and a floor they were told is a floor beats a spinner.
        Result.WasStillIndexing = Registry->IsLoadingAssets();

        // Built locally rather than at namespace scope: an `FName` constructed during static initialization runs
        // before anything guarantees the name table is up.
        const auto GameRoot = FName{TEXT("/Game")};

        auto Assets = TArray<FAssetData>{};
        Registry->GetAssetsByPath(GameRoot, Assets, /*bRecursive*/ true, /*bIncludeOnlyOnDiskAssets*/ true);

        auto SlowTask = FScopedSlowTask{k_ScanStepCount, FText::FromString(TEXT("Reviewing project content..."))};
        SlowTask.MakeDialog(true);

        // ---- Pass 1: what is in /Game, one entry per package ----
        SlowTask.EnterProgressFrame(1.0f, FText::FromString(TEXT("Reading the asset registry...")));

        auto EntryByPackage = TMap<FName, FPackageEntry>{};
        EntryByPackage.Reserve(Assets.Num());

        auto Redirectors = TArray<FAssetData>{};

        for (const auto& Asset : Assets)
        {
            if (Asset.PackageName.IsNone())
            { continue; }

            if (Asset.IsRedirector())
            {
                // A redirector is its own category. It is deliberately NOT a candidate for the other three: it has no
                // meaningful class to duplicate-match on, and "nothing references this redirector" is the ordinary
                // state of a redirector that has already done its job.
                Redirectors.Add(Asset);
                continue;
            }

            const auto ShortName = FPackageName::GetShortName(Asset.PackageName.ToString());
            const auto IsPrimary = Asset.AssetName.ToString().Equals(ShortName, ESearchCase::CaseSensitive);

            auto* Existing = EntryByPackage.Find(Asset.PackageName);

            if (Existing == nullptr)
            {
                auto Entry = FPackageEntry{};
                Entry.Asset = Asset;
                Entry.IsPrimary = IsPrimary;

                EntryByPackage.Add(Asset.PackageName, MoveTemp(Entry));
                continue;
            }

            // First writer wins UNLESS the primary asset arrives later.
            if (IsPrimary && NOT Existing->IsPrimary)
            {
                Existing->Asset = Asset;
                Existing->IsPrimary = true;
            }
        }

        auto PackageData = FAssetPackageData{};

        for (auto& Entry : EntryByPackage)
        {
            if (Registry->TryGetAssetPackageData(Entry.Key, PackageData) != UE::AssetRegistry::EExists::Exists)
            { continue; }

            Entry.Value.DiskSizeBytes = FMath::Max(PackageData.DiskSize, int64{0});
        }

        // ---- Pass 2: what nothing references ----
        SlowTask.EnterProgressFrame(1.0f, FText::FromString(TEXT("Looking for unreferenced assets...")));

        auto Referencers = TArray<FName>{};

        for (const auto& Entry : EntryByPackage)
        {
            if (SlowTask.ShouldCancel())
            {
                Result.WasCancelled = true;
                break;
            }

            const auto ClassName = Get_ClassName(Entry.Value.Asset);

            if (Is_AlwaysRootedClassName(ClassName))
            { continue; }

            Referencers.Reset();

            // The DEFAULT query, not `Hard | Soft`: `EDependencyQuery::Soft` is defined as `NotHard`, so asking for
            // both would set Required and Excluded to the same bit and filter out everything. `NoRequirements` over
            // the `Package` category is how "every package that points at this one, hard or soft" is spelled.
            Registry->GetReferencers(Entry.Key, Referencers,
                UE::AssetRegistry::EDependencyCategory::Package,
                UE::AssetRegistry::EDependencyQuery::NoRequirements);

            auto ExternalReferencerCount = 0;

            for (const auto& Referencer : Referencers)
            {
                // A package referencing itself is not a reason to keep it.
                if (Referencer == Entry.Key)
                { continue; }

                ++ExternalReferencerCount;
            }

            if (ExternalReferencerCount > 0)
            { continue; }

            auto Row = FCkOptimizationDebugger_CleanupRow{};
            Row.Category = ECkOptimizationDebugger_CleanupCategory::Unreferenced;
            Row.AssetPath = Entry.Value.Asset.GetSoftObjectPath().ToString();
            Row.DisplayName = Entry.Value.Asset.AssetName.ToString();
            Row.ClassName = ClassName;
            Row.DiskSizeBytes = Entry.Value.DiskSizeBytes;

            const auto PackagePath = Entry.Key.ToString();

            // A `/Game/Developers` asset is still fair game — a personal sandbox is exactly where unreferenced content
            // accumulates — but the row says where it lives, because deleting somebody else's scratch is a different
            // decision from deleting shipped content.
            Row.Detail = PackagePath.StartsWith(TEXT("/Game/Developers/"), ESearchCase::CaseSensitive)
                ? FString{TEXT("No package references it — in a Developers folder")}
                : FString{TEXT("No package references it")};

            Result.Rows.Add(MoveTemp(Row));
        }

        // ---- Pass 3: what LOOKS like a duplicate ----
        SlowTask.EnterProgressFrame(1.0f, FText::FromString(TEXT("Matching possible duplicates...")));

        auto PackagesByGroupKey = TMap<FString, TArray<const FPackageEntry*>>{};

        for (const auto& Entry : EntryByPackage)
        {
            // A package whose size the registry could not report cannot take part in a size match — grouping the
            // unmeasured ones together would produce a "duplicate" set whose whole evidence is a missing number.
            if (Entry.Value.DiskSizeBytes <= 0)
            { continue; }

            const auto GroupKey = Build_DuplicateGroupKey(
                Entry.Value.Asset.AssetName.ToString(),
                Get_ClassName(Entry.Value.Asset),
                Entry.Value.DiskSizeBytes);

            PackagesByGroupKey.FindOrAdd(GroupKey).Add(&Entry.Value);
        }

        for (auto& Group : PackagesByGroupKey)
        {
            // Polled here as well as in pass 2. Cancel has to mean cancel: breaking out of the unreferenced walk and
            // then running the duplicate match and the redirector walk to completion made the button a request the
            // scan finished ignoring, on the one page where the passes are long enough for somebody to press it.
            if (SlowTask.ShouldCancel())
            {
                Result.WasCancelled = true;
                break;
            }

            if (Group.Value.Num() < 2)
            { continue; }

            auto Paths = TArray<FString>{};
            Paths.Reserve(Group.Value.Num());

            for (const auto* Entry : Group.Value)
            { Paths.Add(Entry->Asset.GetSoftObjectPath().ToString()); }

            // Sorted before the sibling list is built, so every member of a group prints the SAME sentence with only
            // its own path missing — a group whose details disagreed would read as several findings.
            Paths.Sort([](const FString& InLhs, const FString& InRhs)
            {
                return InLhs.Compare(InRhs, ESearchCase::CaseSensitive) < 0;
            });

            for (const auto* Entry : Group.Value)
            {
                const auto SelfPath = Entry->Asset.GetSoftObjectPath().ToString();

                auto Siblings = TArray<FString>{};

                for (const auto& Path : Paths)
                {
                    if (Path.Equals(SelfPath, ESearchCase::CaseSensitive))
                    { continue; }

                    Siblings.Add(Path);
                }

                auto Row = FCkOptimizationDebugger_CleanupRow{};
                Row.Category = ECkOptimizationDebugger_CleanupCategory::Duplicates;
                Row.AssetPath = SelfPath;
                Row.DisplayName = Entry->Asset.AssetName.ToString();
                Row.ClassName = Get_ClassName(Entry->Asset);
                Row.DiskSizeBytes = Entry->DiskSizeBytes;
                Row.DuplicateGroupKey = Group.Key;

                // "Same as", never "identical to". The match is name + class + size and the wording says exactly that
                // much — see the module CLAUDE.md's conservative duplicate rule.
                Row.Detail = ck::Format_UE(TEXT("Same name, class and size as: {}"),
                    FString::Join(Siblings, TEXT(", ")));

                Result.Rows.Add(MoveTemp(Row));
            }
        }

        // ---- Pass 4: redirectors, and what is still unsaved ----
        SlowTask.EnterProgressFrame(1.0f, FText::FromString(TEXT("Collecting redirectors and dirty packages...")));

        for (const auto& Redirector : Redirectors)
        {
            if (SlowTask.ShouldCancel())
            {
                Result.WasCancelled = true;
                break;
            }

            Referencers.Reset();

            Registry->GetReferencers(Redirector.PackageName, Referencers,
                UE::AssetRegistry::EDependencyCategory::Package,
                UE::AssetRegistry::EDependencyQuery::NoRequirements);

            auto Row = FCkOptimizationDebugger_CleanupRow{};
            Row.Category = ECkOptimizationDebugger_CleanupCategory::Redirectors;
            Row.AssetPath = Redirector.GetSoftObjectPath().ToString();
            Row.DisplayName = Redirector.AssetName.ToString();
            Row.ClassName = Get_ClassName(Redirector);

            if (const auto* Entry = EntryByPackage.Find(Redirector.PackageName))
            { Row.DiskSizeBytes = Entry->DiskSizeBytes; }

            // The referencer COUNT rather than the redirector's destination: the destination is only readable by
            // loading the redirector, and a scan that loaded every redirector in the project to label a row would be
            // doing the thing this whole page refuses to do. The count is registry data and is the number that
            // decides how much work a fix-up is.
            Row.Detail = Referencers.IsEmpty()
                ? FString{TEXT("Nothing points at it any more")}
                : ck::Format_UE(TEXT("{} package(s) still point at it"), Referencers.Num());

            Result.Rows.Add(MoveTemp(Row));
        }

        // Live editor state, not registry data — the one category on this page that is only as current as the last
        // press of the button, and the page says so.
        //
        // Skipped outright once Cancel has been pressed. It is the cheapest pass here, but a cancelled scan that
        // still filled one category in full would make the reader trust that one, and "partial" has to mean the
        // whole answer.
        auto DirtyPackages = TArray<UPackage*>{};

        if (NOT Result.WasCancelled)
        { FEditorFileUtils::GetDirtyPackages(DirtyPackages); }

        for (const auto* Package : DirtyPackages)
        {
            if (Package == nullptr)
            { continue; }

            const auto PackageName = Package->GetName();

            if (NOT Is_GameContentPackage(PackageName))
            { continue; }

            auto Row = FCkOptimizationDebugger_CleanupRow{};
            Row.Category = ECkOptimizationDebugger_CleanupCategory::DirtyPackages;
            Row.AssetPath = PackageName;
            Row.DisplayName = FPackageName::GetShortName(PackageName);
            Row.ClassName = FString{TEXT("Package")};

            const auto PackageFName = FName{*PackageName};

            const auto IsOnDisk = Registry->TryGetAssetPackageData(PackageFName, PackageData) ==
                UE::AssetRegistry::EExists::Exists;

            Row.DiskSizeBytes = IsOnDisk ? FMath::Max(PackageData.DiskSize, int64{0}) : 0;

            // "Never saved" and "saved, then changed" are different sentences to somebody deciding what to save, and
            // the size beside them means something different too.
            Row.Detail = IsOnDisk
                ? FString{TEXT("Unsaved changes — the size is what is on disk now")}
                : FString{TEXT("Never saved — nothing on disk yet")};

            Result.Rows.Add(MoveTemp(Row));
        }
#else
        // The module ships in packaged Development/DebugGame targets, where there is no asset registry to walk and
        // no editor to hold a dirty package. Saying so is the whole point of this flag: without it the window would
        // print "0 unreferenced / 0 duplicates / 0 redirectors / 0 dirty" in Ok tone and report a project it never
        // looked at as clean. `Run_Scan` has always answered this way; this pass now does too.
        Result.RequiresEditor = true;
#endif

        return Result;
    }
}

// --------------------------------------------------------------------------------------------------------------------
