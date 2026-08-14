#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_LevelScan.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_DiskScan.h"

#if WITH_EDITOR
#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Actor.h"
#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Blueprint.h"
#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Lighting.h"
#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Material.h"
#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Mesh.h"
#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_ProjectSettings.h"
#include "CkOptimizationDebugger/Analysis/Checks/CkOptimizationDebugger_Checks_Texture.h"

#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/Package.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_levelscan
{
#if WITH_EDITOR
    // Gather + the seven check families + the project disk-size pass. Only used to size the progress bar, so an
    // off-by-one here costs a slightly wrong bar and nothing else.
    constexpr auto k_CheckFamilyCount = 7.0f;
    constexpr auto k_DiskPassCount    = 1.0f;

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_PackageShortName(
            const UObject* InObject)
        -> FString
    {
        if (InObject == nullptr)
        { return FString{}; }

        const auto* Package = InObject->GetOutermost();

        if (Package == nullptr)
        { return FString{}; }

        return FPackageName::GetShortName(Package->GetName());
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** The path of the ASSET a package holds, derived from the package name — `/Game/X/Foo` becomes `/Game/X/Foo.Foo`.
     *  Used for levels and for Blueprint classes, where the object in hand (a `ULevel`, a `UBlueprintGeneratedClass`)
     *  is not the thing the Content Browser can select. */
    auto
        Build_PrimaryAssetPath(
            const UObject* InObject)
        -> FSoftObjectPath
    {
        if (InObject == nullptr)
        { return FSoftObjectPath{}; }

        const auto* Package = InObject->GetOutermost();

        if (Package == nullptr)
        { return FSoftObjectPath{}; }

        const auto PackageName = Package->GetName();
        const auto ShortName = FPackageName::GetShortName(PackageName);

        return FSoftObjectPath{ck::Format_UE(TEXT("{}.{}"), PackageName, ShortName)};
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Records one placement of an asset, creating its row the first time and only widening its level list after. */
    auto
        Record_AssetUsage(
            TArray<FCkOptimizationDebugger_ScanAsset>& InOutAssets,
            TMap<FSoftObjectPath, int32>& InOutIndexByPath,
            UObject* InAsset,
            const FSoftObjectPath& InAssetPath,
            const FString& InDisplayName,
            const FString& InLevelName)
        -> void
    {
        if (InAsset == nullptr || InAssetPath.IsNull())
        { return; }

        if (const auto* Found = InOutIndexByPath.Find(InAssetPath))
        {
            auto& Existing = InOutAssets[*Found];
            ++Existing.UsageCount;
            Existing.ReferencingLevelNames.AddUnique(InLevelName);
            return;
        }

        auto Entry = FCkOptimizationDebugger_ScanAsset{};
        Entry.Asset = InAsset;
        Entry.Path = InAssetPath;
        Entry.DisplayName = InDisplayName;
        Entry.UsageCount = 1;
        Entry.ReferencingLevelNames.Add(InLevelName);

        InOutIndexByPath.Add(InAssetPath, InOutAssets.Num());
        InOutAssets.Add(MoveTemp(Entry));
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Total order on an asset table. The scan's own iteration order is world order, which is not stable across two
     *  sessions that loaded sub-levels in a different sequence — and a findings list that reshuffled between two
     *  identical scans would read as a change nobody made. */
    auto
        Finalize_AssetTable(
            TArray<FCkOptimizationDebugger_ScanAsset>& InOutAssets)
        -> void
    {
        for (auto& Asset : InOutAssets)
        {
            Asset.ReferencingLevelNames.Sort([](const FString& InLhs, const FString& InRhs)
            {
                return InLhs.Compare(InRhs, ESearchCase::IgnoreCase) < 0;
            });
        }

        InOutAssets.Sort([](const FCkOptimizationDebugger_ScanAsset& InLhs,
                            const FCkOptimizationDebugger_ScanAsset& InRhs)
        {
            return InLhs.Path.ToString().Compare(InRhs.Path.ToString(), ESearchCase::CaseSensitive) < 0;
        });
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Everything one static-mesh component contributes: its own usage row, its mesh, and its materials + their
     *  textures. Returns the instancing signature so the caller can store it on the usage row. */
    auto
        Gather_MeshComponent(
            UStaticMeshComponent* InComponent,
            AActor* InActor,
            const FString& InLevelName,
            FCkOptimizationDebugger_ScanContext& InOutContext,
            TMap<FSoftObjectPath, int32>& InOutMeshIndex,
            TMap<FSoftObjectPath, int32>& InOutMaterialIndex,
            TMap<FSoftObjectPath, int32>& InOutTextureIndex)
        -> void
    {
        if (InComponent == nullptr || InActor == nullptr)
        { return; }

        auto* Mesh = InComponent->GetStaticMesh().Get();

        auto Usage = FCkOptimizationDebugger_ScanMeshUsage{};
        Usage.Component = InComponent;
        Usage.Mesh = Mesh;
        Usage.ActorPath = FSoftObjectPath{InActor};
        Usage.ActorDisplayName = InActor->GetActorNameOrLabel();
        Usage.LevelName = InLevelName;
        Usage.Mobility = InComponent->GetMobility();
        Usage.OverridesLightMapRes = InComponent->bOverrideLightMapRes != 0;
        Usage.OverriddenLightMapRes = InComponent->OverriddenLightMapRes;
        Usage.IsPlainStaticMeshActor = InActor->GetClass() == AStaticMeshActor::StaticClass();

        if (Mesh != nullptr)
        {
            Usage.MeshPath = FSoftObjectPath{Mesh};
            Usage.MeshDisplayName = Mesh->GetName();

            Record_AssetUsage(InOutContext.StaticMeshes, InOutMeshIndex,
                Mesh, Usage.MeshPath, Usage.MeshDisplayName, InLevelName);
        }

        auto SignatureParts = TArray<FString>{};
        SignatureParts.Add(Usage.MeshPath.ToString());

        const auto MaterialCount = InComponent->GetNumMaterials();

        for (auto Index = 0; Index < MaterialCount; ++Index)
        {
            auto* Material = InComponent->GetMaterial(Index);

            if (Material == nullptr)
            {
                SignatureParts.Add(TEXT("<none>"));
                continue;
            }

            const auto MaterialPath = FSoftObjectPath{Material};
            SignatureParts.Add(MaterialPath.ToString());

            Record_AssetUsage(InOutContext.Materials, InOutMaterialIndex,
                Material, MaterialPath, Material->GetName(), InLevelName);

            // The textures a material samples are the level's texture population — walking /Game instead would
            // report on content this level never touches, which is a different (and much louder) question.
            auto UsedTextures = TArray<UTexture*>{};
            Material->GetUsedTextures(UsedTextures);

            for (auto* Texture : UsedTextures)
            {
                if (Texture == nullptr)
                { continue; }

                Record_AssetUsage(InOutContext.Textures, InOutTextureIndex,
                    Texture, FSoftObjectPath{Texture}, Texture->GetName(), InLevelName);
            }
        }

        Usage.InstancingSignature = FString::Join(SignatureParts, TEXT("|"));

        InOutContext.MeshUsages.Add(MoveTemp(Usage));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Gather_Level(
            ULevel* InLevel,
            const FString& InLevelName,
            FCkOptimizationDebugger_ScanContext& InOutContext,
            TMap<FSoftObjectPath, int32>& InOutMeshIndex,
            TMap<FSoftObjectPath, int32>& InOutMaterialIndex,
            TMap<FSoftObjectPath, int32>& InOutTextureIndex,
            TMap<FSoftObjectPath, int32>& InOutBlueprintIndex,
            int32& OutActorCount,
            int32& OutMovableLightCount)
        -> void
    {
        OutActorCount = 0;
        OutMovableLightCount = 0;

        if (InLevel == nullptr)
        { return; }

        for (const auto& ActorPtr : InLevel->Actors)
        {
            auto* Actor = ActorPtr.Get();

            if (Actor == nullptr)
            { continue; }

            ++OutActorCount;

            auto ScanActor = FCkOptimizationDebugger_ScanActor{};
            ScanActor.Actor = Actor;
            ScanActor.ActorPath = FSoftObjectPath{Actor};
            ScanActor.DisplayName = Actor->GetActorNameOrLabel();
            ScanActor.LevelName = InLevelName;
            InOutContext.Actors.Add(MoveTemp(ScanActor));

            // The Blueprint CLASS is what a tick or dependency finding belongs to — not the placement that happens
            // to be the hundredth instance of it.
            if (auto* GeneratedClass = Cast<UBlueprintGeneratedClass>(Actor->GetClass()))
            {
                Record_AssetUsage(InOutContext.BlueprintClasses, InOutBlueprintIndex,
                    GeneratedClass,
                    Build_PrimaryAssetPath(GeneratedClass),
                    Get_PackageShortName(GeneratedClass),
                    InLevelName);
            }

            auto MeshComponents = TInlineComponentArray<UStaticMeshComponent*>{};
            Actor->GetComponents(MeshComponents);

            for (auto* Component : MeshComponents)
            {
                Gather_MeshComponent(Component, Actor, InLevelName, InOutContext,
                    InOutMeshIndex, InOutMaterialIndex, InOutTextureIndex);
            }

            auto LightComponents = TInlineComponentArray<ULightComponent*>{};
            Actor->GetComponents(LightComponents);

            for (auto* Component : LightComponents)
            {
                if (Component == nullptr)
                { continue; }

                auto Light = FCkOptimizationDebugger_ScanLight{};
                Light.Light = Component;
                Light.ActorPath = FSoftObjectPath{Actor};
                Light.DisplayName = Actor->GetActorNameOrLabel();
                Light.LevelName = InLevelName;
                Light.Mobility = Component->GetMobility();

                if (Light.Mobility == EComponentMobility::Movable)
                { ++OutMovableLightCount; }

                InOutContext.Lights.Add(MoveTemp(Light));
            }
        }
    }
#endif
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_scan
{
    auto
        TryGet_EditorWorld()
        -> UWorld*
    {
#if WITH_EDITOR
        if (GEditor == nullptr)
        { return nullptr; }

        return GEditor->GetEditorWorldContext().World();
#else
        return nullptr;
#endif
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Run_Scan(
            UWorld* InEditorWorld,
            const FCkOptimizationDebugger_Thresholds& InThresholds,
            const TSet<FName>& InExcludedLevelNames)
        -> FCkOptimizationDebugger_ScanResult
    {
        auto Result = FCkOptimizationDebugger_ScanResult{};

#if WITH_EDITOR
        using namespace ck_optimization_debugger_levelscan;

        if (ck::Is_NOT_Valid(InEditorWorld))
        {
            Result.RequiresEditor = true;
            return Result;
        }

        // ---- Which levels are in scope ----
        struct FLevelToScan
        {
            ULevel* Level = nullptr;
            FString Name;
            FSoftObjectPath Path;
            bool IsPersistent = false;
        };

        auto LevelsToScan = TArray<FLevelToScan>{};

        // Every level the world listed, in world order, whether or not it was walked. The dashboard renders this
        // whole list — an excluded or unloaded level that vanished from it would make a narrowed scan look like a
        // smaller project, which is the one reading the toggles must never produce.
        auto LevelSummaries = TArray<FCkOptimizationDebugger_LevelSummary>{};

        const auto RecordLevel = [&LevelSummaries](const FString& InName, bool InIsPersistent, bool InIsLoaded,
                                                   bool InWasIncluded) -> void
        {
            auto Summary = FCkOptimizationDebugger_LevelSummary{};
            Summary.LevelName = InName;
            Summary.IsPersistentLevel = InIsPersistent;
            Summary.IsLoaded = InIsLoaded;
            Summary.WasIncluded = InWasIncluded;

            LevelSummaries.Add(MoveTemp(Summary));
        };

        if (auto* Persistent = InEditorWorld->PersistentLevel.Get())
        {
            const auto Name = Get_PackageShortName(Persistent);
            const auto Included =
                ck_optimization_debugger_model::ShouldScanLevel(FName{*Name}, InExcludedLevelNames);

            RecordLevel(Name, true, true, Included);

            if (Included)
            {
                LevelsToScan.Add(FLevelToScan{
                    Persistent,
                    Name,
                    Build_PrimaryAssetPath(Persistent),
                    true});
            }
        }

        for (const auto* Streaming : InEditorWorld->GetStreamingLevels())
        {
            if (Streaming == nullptr)
            { continue; }

            auto* Loaded = Streaming->GetLoadedLevel();

            if (Loaded == nullptr)
            {
                // Named rather than skipped silently: saying nothing about a level nobody opened is
                // indistinguishable from finding it clean.
                const auto UnloadedName =
                    FPackageName::GetShortName(Streaming->GetWorldAssetPackageFName().ToString());

                Result.SkippedUnloadedLevelNames.Add(UnloadedName);
                RecordLevel(UnloadedName, false, false, false);
                continue;
            }

            const auto Name = Get_PackageShortName(Loaded);
            const auto Included =
                ck_optimization_debugger_model::ShouldScanLevel(FName{*Name}, InExcludedLevelNames);

            RecordLevel(Name, false, true, Included);

            if (NOT Included)
            { continue; }

            LevelsToScan.Add(FLevelToScan{
                Loaded,
                Name,
                Build_PrimaryAssetPath(Loaded),
                false});
        }

        // ---- Gather ----
        auto SlowTask = FScopedSlowTask{
            static_cast<float>(LevelsToScan.Num()) + k_CheckFamilyCount + k_DiskPassCount,
            FText::FromString(TEXT("Analyzing the open levels..."))};
        SlowTask.MakeDialog(true);

        auto Context = FCkOptimizationDebugger_ScanContext{};
        Context.SkippedUnloadedLevelNames = Result.SkippedUnloadedLevelNames;

        auto MeshIndex = TMap<FSoftObjectPath, int32>{};
        auto MaterialIndex = TMap<FSoftObjectPath, int32>{};
        auto TextureIndex = TMap<FSoftObjectPath, int32>{};
        auto BlueprintIndex = TMap<FSoftObjectPath, int32>{};

        for (const auto& LevelToScan : LevelsToScan)
        {
            SlowTask.EnterProgressFrame(1.0f,
                FText::FromString(ck::Format_UE(TEXT("Walking {}..."), LevelToScan.Name)));

            if (SlowTask.ShouldCancel())
            {
                Result.WasCancelled = true;
                break;
            }

            auto ActorCount = 0;
            auto MovableLightCount = 0;

            Gather_Level(LevelToScan.Level, LevelToScan.Name, Context,
                MeshIndex, MaterialIndex, TextureIndex, BlueprintIndex,
                ActorCount, MovableLightCount);

            auto ScanLevel = FCkOptimizationDebugger_ScanLevel{};
            ScanLevel.LevelName = LevelToScan.Name;
            ScanLevel.LevelPath = LevelToScan.Path;
            ScanLevel.IsPersistentLevel = LevelToScan.IsPersistent;
            ScanLevel.ActorCount = ActorCount;
            ScanLevel.MovableLightCount = MovableLightCount;

            Context.Levels.Add(MoveTemp(ScanLevel));
        }

        Finalize_AssetTable(Context.StaticMeshes);
        Finalize_AssetTable(Context.Materials);
        Finalize_AssetTable(Context.Textures);
        Finalize_AssetTable(Context.BlueprintClasses);

        Result.ScannedLevelNames = Context.Get_ScannedLevelNames();

        // ---- Check ----
        // A cancelled gather still runs the checks over what it DID gather: a partial answer the reader was told is
        // partial beats throwing away the work they waited for.
        const auto RunFamily = [&SlowTask](const FString& InLabel) -> void
        {
            SlowTask.EnterProgressFrame(1.0f,
                FText::FromString(ck::Format_UE(TEXT("Running {} checks..."), InLabel)));
        };

        RunFamily(TEXT("mesh"));
        ck_optimization_debugger_checks_mesh::Run_Checks(Context, InThresholds, Result.Findings);

        RunFamily(TEXT("texture"));
        ck_optimization_debugger_checks_texture::Run_Checks(Context, InThresholds, Result.Findings);

        RunFamily(TEXT("material"));
        ck_optimization_debugger_checks_material::Run_Checks(Context, InThresholds, Result.Findings);

        RunFamily(TEXT("lighting"));
        ck_optimization_debugger_checks_lighting::Run_Checks(Context, InThresholds, Result.Findings);

        RunFamily(TEXT("actor"));
        ck_optimization_debugger_checks_actor::Run_Checks(Context, InThresholds, Result.Findings);

        RunFamily(TEXT("blueprint"));
        ck_optimization_debugger_checks_blueprint::Run_Checks(Context, InThresholds, Result.Findings);

        RunFamily(TEXT("project settings"));
        ck_optimization_debugger_checks_project_settings::Run_Checks(Context, InThresholds, Result.Findings);

        // ---- Census ----
        // Folded out of the SAME context the checks just read. Nothing here re-walks the world.
        auto& Summary = Result.Summary;

        Summary.ActorCount = Context.Actors.Num();
        Summary.StaticMeshUsageCount = Context.MeshUsages.Num();
        Summary.UniqueStaticMeshCount = Context.StaticMeshes.Num();
        Summary.UniqueMaterialCount = Context.Materials.Num();
        Summary.UniqueTextureCount = Context.Textures.Num();

        for (const auto& MeshAsset : Context.StaticMeshes)
        {
            const auto* Mesh = Cast<UStaticMesh>(MeshAsset.Asset.Get());

            if (Mesh == nullptr)
            { continue; }

            // Summed over UNIQUE meshes: this is the content budget, not the frame cost. Counting a mesh once per
            // placement would report a number no amount of instancing could ever move.
            Summary.Lod0TriangleTotal += static_cast<int64>(Mesh->GetNumTriangles(0));
        }

        for (const auto& Light : Context.Lights)
        {
            ++Summary.LightCount;

            switch (Light.Mobility.GetValue())
            {
                case EComponentMobility::Static:     ++Summary.StaticLightCount;     break;
                case EComponentMobility::Stationary: ++Summary.StationaryLightCount; break;
                case EComponentMobility::Movable:    ++Summary.MovableLightCount;    break;
                default: break;
            }
        }

        // Actor counts land on the summary rows the gather actually filled; excluded and unloaded levels keep the
        // zero they were recorded with.
        for (const auto& ScannedLevel : Context.Levels)
        {
            auto* Row = LevelSummaries.FindByPredicate([&ScannedLevel](const FCkOptimizationDebugger_LevelSummary& InRow)
            {
                return InRow.LevelName == ScannedLevel.LevelName;
            });

            if (Row == nullptr)
            { continue; }

            Row->ActorCount = ScannedLevel.ActorCount;
        }

        Summary.Levels = MoveTemp(LevelSummaries);
        Summary.FindingCounts = ck_optimization_debugger_model::Get_CountsBySeverity(Result.Findings);

        // The project's disk breakdown is a whole-`/Game` registry pass and easily the most expensive step here,
        // which is why it carries its own progress frame and runs last: everything the reader asked about the LEVEL
        // is already computed by the time this starts.
        SlowTask.EnterProgressFrame(1.0f,
            FText::FromString(TEXT("Measuring project disk usage...")));

        Summary.Disk = ck_optimization_debugger_disk::Build_ProjectDiskBreakdown();
#else
        // No editor world exists in a packaged Development/DebugGame target. Saying so is the whole answer — a
        // silent empty result would read as a clean level. Every parameter goes unread here on purpose.
        (void)InEditorWorld;
        (void)InThresholds;
        (void)InExcludedLevelNames;

        Result.RequiresEditor = true;
#endif

        return Result;
    }
}

// --------------------------------------------------------------------------------------------------------------------
