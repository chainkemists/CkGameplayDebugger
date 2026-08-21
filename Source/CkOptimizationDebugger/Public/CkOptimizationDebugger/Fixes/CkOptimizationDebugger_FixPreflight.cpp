#include "CkOptimizationDebugger/Fixes/CkOptimizationDebugger_FixPreflight.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/StaticMeshComponent.h"
#include "Editor/GroupActor.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "LevelUtils.h"
#include "MaterialCachedData.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "SceneTypes.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_PlacementAudit::
    Get_IsConvertible() const
    -> bool
{
    return Refusals.IsEmpty();
}

// --------------------------------------------------------------------------------------------------------------------

// File-local helpers in the module's own named namespace rather than an anonymous one — this module compiles unity.
namespace ck_optimization_debugger_preflight_impl
{
    auto
        Make_Refusal(
            ECkOptimizationDebugger_RefusalKind InKind,
            const FString& InDetail)
        -> FCkOptimizationDebugger_Refusal
    {
        auto Refusal = FCkOptimizationDebugger_Refusal{};
        Refusal.Kind = InKind;
        Refusal.Detail = InDetail;

        return Refusal;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_RefusalKindLabel(
            ECkOptimizationDebugger_RefusalKind InKind)
        -> FString
    {
        switch (InKind)
        {
            case ECkOptimizationDebugger_RefusalKind::NotPlainStaticMeshActor: return TEXT("not a plain Static Mesh Actor");
            case ECkOptimizationDebugger_RefusalKind::MeshChanged:             return TEXT("no longer uses this mesh");
            case ECkOptimizationDebugger_RefusalKind::NonStaticMobility:       return TEXT("is not static");
            case ECkOptimizationDebugger_RefusalKind::Attached:                return TEXT("is attached to or holds an attachment");
            case ECkOptimizationDebugger_RefusalKind::ExtraSceneComponents:    return TEXT("carries extra components");
            case ECkOptimizationDebugger_RefusalKind::CarriesActorTags:        return TEXT("carries actor tags");
            case ECkOptimizationDebugger_RefusalKind::CarriesComponentTags:    return TEXT("carries component tags");
            case ECkOptimizationDebugger_RefusalKind::InEditorGroup:           return TEXT("belongs to an editor Group");
            case ECkOptimizationDebugger_RefusalKind::InDataLayer:             return TEXT("belongs to a Data Layer");
            case ECkOptimizationDebugger_RefusalKind::HasHlodLayer:            return TEXT("has an HLOD layer");
            case ECkOptimizationDebugger_RefusalKind::DynamicMaterialInstance: return TEXT("uses a dynamic material instance");
            case ECkOptimizationDebugger_RefusalKind::ExternallyReferenced:    return TEXT("is referenced from outside");
            case ECkOptimizationDebugger_RefusalKind::DiffersFromTemplate:     return TEXT("differs from the template");
            case ECkOptimizationDebugger_RefusalKind::LevelLocked:             return TEXT("is in a locked level");
            default: break;
        }

        return TEXT("cannot be converted");
    }

    // ----------------------------------------------------------------------------------------------------------------

    // Enum order, so the same selection produces the same sentence on every run. Sorting by count instead would let
    // two reasons that tie swap places between identical presses.
    constexpr auto k_RefusalKindCount = static_cast<int32>(ECkOptimizationDebugger_RefusalKind::LevelLocked) + 1;

#if WITH_EDITOR
    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ActorDisplayName(
            const AActor* InActor)
        -> FString
    {
        if (InActor == nullptr)
        { return FString{}; }

        return InActor->GetActorNameOrLabel();
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Whether the placement is named by a Blueprint node in its own level's script.
     *
     *  Asked per actor rather than mapped once because it is a walk of ONE Blueprint's nodes, and because a level
     *  with no script Blueprint — the common case — answers instantly. */
    auto
        Is_ReferencedByLevelScript(
            AActor* InActor)
        -> bool
    {
        auto* Level = InActor->GetLevel();

        if (Level == nullptr)
        { return false; }

        constexpr auto DontCreate = true;
        auto* LevelScript = Level->GetLevelScriptBlueprint(DontCreate);

        if (LevelScript == nullptr)
        { return false; }

        auto ReferencingNodes = TArray<UK2Node*>{};

        return FBlueprintEditorUtils::FindReferencesToActorFromLevelScript(LevelScript, InActor, ReferencingNodes)
            && ReferencingNodes.Num() > 0;
    }
#endif
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_preflight
{
    using namespace ck_optimization_debugger_preflight_impl;

    auto
        Get_RefusalSentence(
            const FCkOptimizationDebugger_Refusal& InRefusal)
        -> FString
    {
        const auto Label = Get_RefusalKindLabel(InRefusal.Kind);

        if (InRefusal.Detail.IsEmpty())
        { return Label; }

        return ck::Format_UE(TEXT("{} ({})"), Label, InRefusal.Detail);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ConvertiblePlacements(
            const TArray<FCkOptimizationDebugger_PlacementAudit>& InAudits)
        -> TArray<FCkOptimizationDebugger_PlacementAudit>
    {
        return InAudits.FilterByPredicate([](const FCkOptimizationDebugger_PlacementAudit& InAudit) -> bool
        {
            return InAudit.Get_IsConvertible();
        });
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_RefusedPlacements(
            const TArray<FCkOptimizationDebugger_PlacementAudit>& InAudits)
        -> TArray<FCkOptimizationDebugger_PlacementAudit>
    {
        return InAudits.FilterByPredicate([](const FCkOptimizationDebugger_PlacementAudit& InAudit) -> bool
        {
            return NOT InAudit.Get_IsConvertible();
        });
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_RefusalSummary(
            const TArray<FCkOptimizationDebugger_PlacementAudit>& InAudits)
        -> FString
    {
        const auto Refused = Get_RefusedPlacements(InAudits);

        if (Refused.IsEmpty())
        { return FString{}; }

        // A placement refused for two reasons is counted under BOTH: the reader deciding what to unpick needs to
        // know every reason that stands in the way, not whichever one the audit asked about first.
        auto CountsByKind = TArray<int32>{};
        CountsByKind.SetNumZeroed(k_RefusalKindCount);

        for (const auto& Audit : Refused)
        {
            auto SeenKinds = TArray<bool>{};
            SeenKinds.SetNumZeroed(k_RefusalKindCount);

            for (const auto& Refusal : Audit.Refusals)
            {
                const auto KindIndex = static_cast<int32>(Refusal.Kind);

                if (NOT CountsByKind.IsValidIndex(KindIndex) || SeenKinds[KindIndex])
                { continue; }

                SeenKinds[KindIndex] = true;
                ++CountsByKind[KindIndex];
            }
        }

        auto Parts = TArray<FString>{};

        for (auto KindIndex = 0; KindIndex < k_RefusalKindCount; ++KindIndex)
        {
            if (CountsByKind[KindIndex] == 0)
            { continue; }

            Parts.Add(ck::Format_UE(TEXT("{} {}"),
                CountsByKind[KindIndex],
                Get_RefusalKindLabel(static_cast<ECkOptimizationDebugger_RefusalKind>(KindIndex))));
        }

        return ck::Format_UE(TEXT("{} of {} placement(s) left alone: {}"),
            Refused.Num(), InAudits.Num(), FString::Join(Parts, TEXT(", ")));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ExcludedComparisonProperties()
        -> const TArray<FName>&
    {
        // Every entry is a DECISION, and this is the only place one may be made. The comparison is reflection-driven
        // precisely so that a property cannot be forgotten; an exclusion here is the explicit opposite of forgetting.
        static const auto Excluded = TArray<FName>
        {
            // Carried: the placement's transform becomes the instance's transform, scale included.
            FName{TEXT("RelativeLocation")},
            FName{TEXT("RelativeRotation")},
            FName{TEXT("RelativeScale3D")},

            // Already gated by an earlier, better-worded refusal.
            FName{TEXT("Mobility")},
            FName{TEXT("StaticMesh")},
            FName{TEXT("Tags")},
            FName{TEXT("ComponentTags")},
            FName{TEXT("HLODLayer")},
            FName{TEXT("DataLayerAssets")},
            FName{TEXT("DataLayers")},
            FName{TEXT("AttachParent")},
            FName{TEXT("AttachSocketName")},

            // Object identity rather than value: two placements necessarily hold two different components, and
            // saying so on every row would bury every refusal that means something.
            FName{TEXT("RootComponent")},
            FName{TEXT("StaticMeshComponent")},

            // Compared by EFFECTIVE material instead — the conversion's own grouping key is the resolved
            // `GetMaterial(i)` per slot. An override that names the mesh's own default resolves to the same
            // material, and refusing that placement would be refusing a difference that does not exist.
            FName{TEXT("OverrideMaterials")},

            // The editor billboard's scale. Invisible at runtime and invisible in a cooked build.
            FName{TEXT("SpriteScale")},
        };

        return Excluded;
    }

    // ----------------------------------------------------------------------------------------------------------------

#if WITH_EDITOR

    auto
        Compare_ReflectedProperties(
            const UObject* InTemplate,
            const UObject* InCandidate)
        -> TArray<FCkOptimizationDebugger_Refusal>
    {
        auto Refusals = TArray<FCkOptimizationDebugger_Refusal>{};

        if (InTemplate == nullptr || InCandidate == nullptr || InTemplate->GetClass() != InCandidate->GetClass())
        { return Refusals; }

        const auto& Excluded = Get_ExcludedComparisonProperties();

        for (auto PropertyIt = TFieldIterator<FProperty>{InTemplate->GetClass()}; PropertyIt; ++PropertyIt)
        {
            const auto* Property = *PropertyIt;

            // Authored state only. A property nobody can set in the details panel is not a difference the reader
            // made, and a transient one is this session's scratch space rather than anything the level saved.
            if (NOT Property->HasAnyPropertyFlags(CPF_Edit))
            { continue; }

            if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_EditConst | CPF_Deprecated))
            { continue; }

            if (Excluded.Contains(Property->GetFName()))
            { continue; }

            auto IsIdentical = true;

            for (auto ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ++ArrayIndex)
            {
                if (Property->Identical_InContainer(InTemplate, InCandidate, ArrayIndex))
                { continue; }

                IsIdentical = false;
                break;
            }

            if (IsIdentical)
            { continue; }

            Refusals.Add(Make_Refusal(ECkOptimizationDebugger_RefusalKind::DiffersFromTemplate,
                Property->GetDisplayNameText().ToString()));
        }

        // `TFieldIterator` walks the class's property link list, whose order is a layout detail. Two placements
        // differing in the same two properties must produce the same sentence in both directions.
        Refusals.Sort([](const FCkOptimizationDebugger_Refusal& InLhs, const FCkOptimizationDebugger_Refusal& InRhs)
        {
            return InLhs.Detail.Compare(InRhs.Detail, ESearchCase::CaseSensitive) < 0;
        });

        return Refusals;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_ReferenceContext(
            UWorld* InWorld)
        -> FCkOptimizationDebugger_ReferenceContext
    {
        auto Context = FCkOptimizationDebugger_ReferenceContext{};

        if (ck::Is_NOT_Valid(InWorld))
        { return Context; }

        // Group actors reference every member by design, and a group is already its own, better-worded refusal.
        // Counting it here too would refuse a grouped placement twice and say nothing new the second time.
        auto ClassesToIgnore = TArray<UClass*>{AGroupActor::StaticClass()};

        FBlueprintEditorUtils::GetActorReferenceMap(InWorld, ClassesToIgnore, Context.ReferencingActors);

        Context.WasBuilt = true;

        return Context;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Audit_Placement(
            AStaticMeshActor* InCandidate,
            AStaticMeshActor* InTemplate,
            const UStaticMesh* InMesh,
            const FCkOptimizationDebugger_ReferenceContext& InReferences)
        -> FCkOptimizationDebugger_PlacementAudit
    {
        auto Audit = FCkOptimizationDebugger_PlacementAudit{};

        if (ck::Is_NOT_Valid(InCandidate))
        {
            Audit.Refusals.Add(Make_Refusal(ECkOptimizationDebugger_RefusalKind::NotPlainStaticMeshActor, {}));
            return Audit;
        }

        Audit.Path = FSoftObjectPath{InCandidate};
        Audit.DisplayName = Get_ActorDisplayName(InCandidate);

        // A subclass may run logic no instance can carry, so this one is asked first and everything below it can
        // assume a plain placement.
        if (InCandidate->GetClass() != AStaticMeshActor::StaticClass())
        {
            Audit.Refusals.Add(Make_Refusal(ECkOptimizationDebugger_RefusalKind::NotPlainStaticMeshActor,
                InCandidate->GetClass()->GetName()));

            return Audit;
        }

        const auto* Component = InCandidate->GetStaticMeshComponent();

        if (Component == nullptr || Component->GetStaticMesh().Get() != InMesh)
        {
            Audit.Refusals.Add(Make_Refusal(ECkOptimizationDebugger_RefusalKind::MeshChanged, {}));
            return Audit;
        }

        if (Component->GetMobility() != EComponentMobility::Static)
        { Audit.Refusals.Add(Make_Refusal(ECkOptimizationDebugger_RefusalKind::NonStaticMobility, {})); }

        auto AttachedActors = TArray<AActor*>{};
        InCandidate->GetAttachedActors(AttachedActors);

        if (Component->GetAttachParent() != nullptr || NOT AttachedActors.IsEmpty())
        { Audit.Refusals.Add(Make_Refusal(ECkOptimizationDebugger_RefusalKind::Attached, {})); }

        auto SceneComponents = TInlineComponentArray<USceneComponent*>{};
        InCandidate->GetComponents(SceneComponents);

        if (SceneComponents.Num() != 1)
        {
            Audit.Refusals.Add(Make_Refusal(ECkOptimizationDebugger_RefusalKind::ExtraSceneComponents,
                ck::Format_UE(TEXT("{} scene components"), SceneComponents.Num())));
        }

        // An instance is a transform inside a component: there is nowhere for a tag to live, so every tag-gated
        // query naming this placement would silently return one fewer result and nothing would say why.
        if (NOT InCandidate->Tags.IsEmpty())
        {
            auto TagNames = TArray<FString>{};
            for (const auto& Tag : InCandidate->Tags)
            { TagNames.Add(Tag.ToString()); }

            Audit.Refusals.Add(Make_Refusal(ECkOptimizationDebugger_RefusalKind::CarriesActorTags,
                FString::Join(TagNames, TEXT(", "))));
        }

        if (NOT Component->ComponentTags.IsEmpty())
        {
            auto TagNames = TArray<FString>{};
            for (const auto& Tag : Component->ComponentTags)
            { TagNames.Add(Tag.ToString()); }

            Audit.Refusals.Add(Make_Refusal(ECkOptimizationDebugger_RefusalKind::CarriesComponentTags,
                FString::Join(TagNames, TEXT(", "))));
        }

        if (auto* Group = Cast<AGroupActor>(InCandidate->GroupActor))
        {
            Audit.Refusals.Add(Make_Refusal(ECkOptimizationDebugger_RefusalKind::InEditorGroup,
                Get_ActorDisplayName(Group)));
        }

        const auto DataLayerAssets = InCandidate->GetDataLayerAssets();

        if (NOT DataLayerAssets.IsEmpty())
        {
            auto LayerNames = TArray<FString>{};
            for (const auto* DataLayerAsset : DataLayerAssets)
            {
                if (DataLayerAsset != nullptr)
                { LayerNames.Add(DataLayerAsset->GetName()); }
            }

            LayerNames.Sort();

            Audit.Refusals.Add(Make_Refusal(ECkOptimizationDebugger_RefusalKind::InDataLayer,
                FString::Join(LayerNames, TEXT(", "))));
        }

        if (const auto* HlodLayer = InCandidate->GetHLODLayer())
        {
            Audit.Refusals.Add(Make_Refusal(ECkOptimizationDebugger_RefusalKind::HasHlodLayer,
                HlodLayer->GetName()));
        }

        const auto MaterialCount = Component->GetNumMaterials();

        for (auto Index = 0; Index < MaterialCount; ++Index)
        {
            // Created and driven at runtime against THIS component. One shared instanced component cannot be the
            // target of a per-placement dynamic instance, and the parameter drive would stop where it stands.
            if (Cast<UMaterialInstanceDynamic>(Component->GetMaterial(Index)) == nullptr)
            { continue; }

            Audit.Refusals.Add(Make_Refusal(ECkOptimizationDebugger_RefusalKind::DynamicMaterialInstance,
                ck::Format_UE(TEXT("slot {}"), Index)));

            break;
        }

        const auto* Level = InCandidate->GetLevel();

        if (Level != nullptr && FLevelUtils::IsLevelLocked(const_cast<ULevel*>(Level)))
        { Audit.Refusals.Add(Make_Refusal(ECkOptimizationDebugger_RefusalKind::LevelLocked, {})); }

        // Only ever consulted when the map was actually built. An unbuilt context means nobody asked, and treating
        // that as "nothing references it" is the exact reading that lets a referenced actor be deleted.
        if (InReferences.WasBuilt)
        {
            if (const auto* ReferencingActors = InReferences.ReferencingActors.Find(InCandidate);
                ReferencingActors != nullptr && ReferencingActors->Num() > 0)
            {
                Audit.Refusals.Add(Make_Refusal(ECkOptimizationDebugger_RefusalKind::ExternallyReferenced,
                    ck::Format_UE(TEXT("{} actor(s)"), ReferencingActors->Num())));
            }
            else if (Is_ReferencedByLevelScript(InCandidate))
            {
                Audit.Refusals.Add(Make_Refusal(ECkOptimizationDebugger_RefusalKind::ExternallyReferenced,
                    TEXT("the Level Blueprint")));
            }
        }

        // The template is not compared with itself: every property would match, and the absolute reasons above are
        // the whole of what a template has to answer for.
        if (InTemplate == InCandidate || ck::Is_NOT_Valid(InTemplate))
        { return Audit; }

        Audit.Refusals.Append(Compare_ReflectedProperties(InTemplate, InCandidate));
        Audit.Refusals.Append(Compare_ReflectedProperties(InTemplate->GetStaticMeshComponent(), Component));

        return Audit;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Detach_FromEditorGroup(
            AActor* InActor)
        -> FString
    {
        if (ck::Is_NOT_Valid(InActor))
        { return FString{}; }

        auto* Group = Cast<AGroupActor>(InActor->GroupActor);

        if (Group == nullptr)
        { return FString{}; }

        const auto GroupName = Get_ActorDisplayName(Group);

        // `Remove` calls `Modify` on both sides and runs `PostRemove`, so a group left with nothing in it collapses
        // exactly as it does when the editor's own delete path empties one.
        Group->Remove(*InActor);

        return GroupName;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_WorldPositionOffsetAnswer(
            const UStaticMesh* InMesh,
            TArray<FString>& OutMaterialNames)
        -> ECkOptimizationDebugger_WpoAnswer
    {
        OutMaterialNames.Reset();

        if (ck::Is_NOT_Valid(InMesh))
        { return ECkOptimizationDebugger_WpoAnswer::Unknown; }

        auto AnyUnknown = false;

        for (const auto& StaticMaterial : InMesh->GetStaticMaterials())
        {
            const UMaterialInterface* Material = StaticMaterial.MaterialInterface.Get();

            if (Material == nullptr)
            { continue; }

            const auto& Cached = Material->GetCachedExpressionData();

            // The shared EMPTY record answers "no" to every question asked of it, so a material with no cache would
            // otherwise be reported as having no World Position Offset — a claim it never made.
            if (&Cached == &FMaterialCachedExpressionData::EmptyData)
            {
                AnyUnknown = true;
                continue;
            }

            if (NOT Cached.IsPropertyConnected(MP_WorldPositionOffset))
            { continue; }

            OutMaterialNames.AddUnique(Material->GetName());
        }

        if (NOT OutMaterialNames.IsEmpty())
        {
            OutMaterialNames.Sort();
            return ECkOptimizationDebugger_WpoAnswer::UsesWorldPositionOffset;
        }

        return AnyUnknown
            ? ECkOptimizationDebugger_WpoAnswer::Unknown
            : ECkOptimizationDebugger_WpoAnswer::NoWorldPositionOffset;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ReferencingPackageCount(
            const FSoftObjectPath& InPath)
        -> int32
    {
        if (InPath.IsNull())
        { return -1; }

        const auto* AssetRegistry = IAssetRegistry::Get();

        if (AssetRegistry == nullptr)
        { return -1; }

        const auto PackageName = FName{*InPath.GetLongPackageName()};

        auto Referencers = TArray<FName>{};

        // The DEFAULT query over the `Package` category, which is hard AND soft — `EDependencyQuery::Soft` is
        // defined as `NotHard`, so OR-ing the two would filter out everything.
        AssetRegistry->GetReferencers(PackageName, Referencers,
            UE::AssetRegistry::EDependencyCategory::Package,
            UE::AssetRegistry::EDependencyQuery::NoRequirements);

        Referencers.Remove(PackageName);

        return Referencers.Num();
    }

#endif
}

// --------------------------------------------------------------------------------------------------------------------
