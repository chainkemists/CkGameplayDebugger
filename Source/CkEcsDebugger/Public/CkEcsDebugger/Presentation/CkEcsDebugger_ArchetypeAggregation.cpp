#include "CkEcsDebugger_ArchetypeAggregation.h"

#include "CkEditorTools/Style/CkIconStyle.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"
#include "CkEcs/Archetype/CkArchetype_Registry.h"
#include "CkEcs/DebugFeatureFlags/CkDebugFeatureFlags.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcsDebugger/Classification/CkEcsDebugger_Classification.h"
#include "CkEcsDebugger/Presentation/CkEcsDebugger_FeatureVisuals.h"
#include "CkEcsDebugger/Query/CkEcsDebugger_Query.h"
#include "CkEcsDebugger/Settings/CkEcsDebuggerSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::ecs_debugger_aggregation::
    Aggregate(
        const TArray<FCk_Handle>& InEntities)
    -> TArray<FArchetypeBucket>
{
    namespace visuals = ecs_debugger_feature_visuals;

    const auto ClassTable = ecs_debugger_classification::BuildTable(
        UCkEcsDebuggerSettings::Get()->InternalFeatureIds);

    auto Buckets = TMap<FString, FArchetypeBucket>{};

    for (const auto& Entity : InEntities)
    {
        if (ck::Is_NOT_Valid(Entity))
        { continue; }

        const auto Bits = debug_feature_flags::Get_Flags(Entity.Get_RegistryView(), Entity.Get_Entity());
        const auto Registered = archetype_registry::TryGet_BestMatchName(Entity);

        auto Key = FString{};
        auto DisplayName = FString{};
        auto FilterToken = FString{};

        if (Registered.IsNone())
        {
            const auto CleanName = DebugNameClean::Get_CleanName(
                UCk_Utils_Handle_UE::Get_DebugName(Entity).ToString());
            Key = ecs_debugger_query::Get_InferredArchetypeKey(CleanName, Bits);

            if (NOT Key.Split(TEXT("#"), &DisplayName, nullptr))
            { DisplayName = Key; }
            FilterToken = DisplayName;
        }
        else
        {
            Key = Registered.ToString();
            DisplayName = Key;
            FilterToken = Key;
        }

        auto& Bucket = Buckets.FindOrAdd(Key);
        if (Bucket.Count == 0)
        {
            Bucket.Key = Key;
            Bucket.DisplayName = DisplayName;
            Bucket.FilterToken = FilterToken;
            Bucket.IsRegistered = NOT Registered.IsNone();
            Bucket.SignatureBits = Bits;
            Bucket.Representative = Entity;

            auto FeatureColor = TOptional<FLinearColor>{};
            auto DominantFeature = FName{};

            for (const auto& [FeatureId, Bit] : visuals::Get_BadgeFeatures())
            {
                if ((Bits & (uint64{1} << Bit)) == 0)
                { continue; }

                DominantFeature = FeatureId;
                break;
            }

            if (Bucket.IsRegistered)
            {
                if (const auto Descriptor = archetype_registry::Find(Registered); Descriptor.IsSet())
                {
                    // A game archetype names its own glyph (IconSvgPath); resolution goes through the
                    // typed registry's dynamic side-lane, which also accepts generated semantic names.
                    const auto BespokeIcon = FName{FPaths::GetBaseFilename(Descriptor->Get_IconSvgPath())};
                    if (FCkIconStyle::Get_DynamicBrush(BespokeIcon, ECk_Icon_BrushSize::Size_16x16) != nullptr)
                    { Bucket.DynamicIcon = BespokeIcon; }

                    if (Descriptor->Get_Color() != FLinearColor::White)
                    { FeatureColor = Descriptor->Get_Color(); }

                    Bucket.Family = Descriptor->Get_Family().IsNone()
                        ? FName{TEXT("Game")}
                        : Descriptor->Get_Family();
                }
            }
            else
            {
                // Inferred buckets prefer the dominant-feature glyph (semantic): a Timer
                // swarm reads as timers. Registered archetypes skip this — each named
                // archetype gets its own identity below.
                if (NOT DominantFeature.IsNone())
                {
                    if (const auto* Visual = visuals::Get_FeatureVisuals().Find(DominantFeature))
                    {
                        Bucket.Icon = Visual->Icon;
                        FeatureColor = Visual->Color;
                    }
                }

                const auto Classification = ecs_debugger_classification::Classify(Bits, ClassTable);
                Bucket.Family = Classification.IsInternal ? FName{TEXT("Internals")}
                    : NOT DominantFeature.IsNone() ? DominantFeature
                    : FName{TEXT("Other")};
            }

            // General-pool assignment: stable hash of the archetype key picks a glyph —
            // distinct identity instead of anonymous cubes. Cube only if the pool is empty.
            if (Bucket.Icon == ECk_Icon::None && Bucket.DynamicIcon.IsNone())
            {
                const auto Pool = ck::icons::Get_GeneratedPool();
                Bucket.Icon = Pool.Num() > 0
                    ? Pool[FCrc::StrCrc32(*Key) % static_cast<uint32>(Pool.Num())]
                    : ECk_Icon::Entity;
            }

            // Color: feature/descriptor hue when one exists, else a stable palette pick —
            // every card carries an accent instead of uniform grey.
            const auto& Palette = visuals::Get_ArchetypePalette();
            Bucket.IconColor = FeatureColor.IsSet() ? FeatureColor.GetValue()
                : Palette.Num() > 0 ? Palette[FCrc::StrCrc32(*Key) % static_cast<uint32>(Palette.Num())]
                : FLinearColor::White;
        }
        ++Bucket.Count;
    }

    auto Sorted = TArray<FArchetypeBucket>{};
    Buckets.GenerateValueArray(Sorted);
    Sorted.Sort([](const FArchetypeBucket& A, const FArchetypeBucket& B)
    {
        // Registered game archetypes first, then by population. Key tie-break keeps the
        // order deterministic — Sort is unstable, and a shuffling order would defeat
        // consumers' no-reslot fast paths.
        if (A.IsRegistered != B.IsRegistered)
        { return A.IsRegistered; }
        if (A.Count != B.Count)
        { return A.Count > B.Count; }
        return A.Key < B.Key;
    });

    return Sorted;
}

auto
    ck::ecs_debugger_aggregation::FArchetypeBucket::
    Get_IconBrush() const
    -> const FSlateBrush*
{
    if (NOT DynamicIcon.IsNone())
    { return FCkIconStyle::Get_DynamicBrush(DynamicIcon, ECk_Icon_BrushSize::Size_16x16); }

    return FCkIconStyle::Get_Brush(Icon, ECk_Icon_BrushSize::Size_16x16);
}
