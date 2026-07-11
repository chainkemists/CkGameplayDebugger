#include "CkEcsDebugger_FeatureVisuals.h"

#include "CkEcs/DebugFeatureFlags/CkDebugFeatureFlags.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::ecs_debugger_feature_visuals::
    Get_FeatureVisuals()
    -> const TMap<FName, FFeatureVisual>&
{
    static const auto Visuals = []() -> TMap<FName, FFeatureVisual>
    {
        auto Map = TMap<FName, FFeatureVisual>{};

        for (const auto& Metadata : FCkDebuggerInspectorRegistry::Get().Get_AllMetadata())
        {
            if (Metadata.FeatureFlagId.IsNone() || Metadata.IconName.IsNone())
            { continue; }

            Map.Add(Metadata.FeatureFlagId, FFeatureVisual{
                Metadata.IconName,
                Metadata.Color.Get(FLinearColor::White) });
        }

        Map.Add(TEXT("StateMachine"),     FFeatureVisual{ TEXT("StateMachine"), FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8F6FE8"))) });
        Map.Add(TEXT("Aggro"),            FFeatureVisual{ TEXT("Aggro"),        FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C94F4F"))) });
        Map.Add(TEXT("AudioTrack"),       FFeatureVisual{ TEXT("Audio"),        FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("4FA3C9"))) });
        Map.Add(TEXT("Label"),            FFeatureVisual{ TEXT("Label"),        FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8B93A1"))) });
        Map.Add(TEXT("FloatAttribute"),   FFeatureVisual{ TEXT("Attribute"),    FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("3FA1AD"))) });
        Map.Add(TEXT("ByteAttribute"),    FFeatureVisual{ TEXT("Attribute"),    FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("3FA1AD"))) });
        Map.Add(TEXT("IntegerAttribute"), FFeatureVisual{ TEXT("Attribute"),    FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("3FA1AD"))) });

        return Map;
    }();

    return Visuals;
}

auto
    ck::ecs_debugger_feature_visuals::
    Get_BadgeFeatures()
    -> const TArray<TPair<FName, int32>>&
{
    static const auto Features = []() -> TArray<TPair<FName, int32>>
    {
        auto Result = TArray<TPair<FName, int32>>{};
        for (const auto& FeatureId : debug_feature_flags::Get_RegisteredFeatureIds())
        {
            if (FeatureId == TEXT("Transform") || FeatureId == TEXT("Label") ||
                FeatureId.ToString().StartsWith(TEXT("_")))
            { continue; }

            const auto Bit = debug_feature_flags::Get_BitIndex(FeatureId);
            if (Bit != INDEX_NONE)
            { Result.Emplace(FeatureId, Bit); }
        }
        return Result;
    }();

    return Features;
}
