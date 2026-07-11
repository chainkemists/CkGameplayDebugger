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
        Map.Add(TEXT("Objective"),        FFeatureVisual{ TEXT("Objective"),    FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D9B23F"))) });
        Map.Add(TEXT("Vfx"),              FFeatureVisual{ TEXT("Vfx"),          FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("F06FD0"))) });
        Map.Add(TEXT("Camera"),           FFeatureVisual{ TEXT("Camera"),       FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("6FA8E8"))) });
        Map.Add(TEXT("Goap"),             FFeatureVisual{ TEXT("Goap"),         FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("B06FE8"))) });
        Map.Add(TEXT("Eqs"),              FFeatureVisual{ TEXT("Eqs"),          FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FD0A0"))) });
        Map.Add(TEXT("IsmProxy"),         FFeatureVisual{ TEXT("IsmRenderer"),  FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("7F8FE8"))) });
        Map.Add(TEXT("IskmProxy"),        FFeatureVisual{ TEXT("IsmRenderer"),  FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("9F7FE8"))) });
        Map.Add(TEXT("ActorBridge"),      FFeatureVisual{ TEXT("ActorBridge"),  FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E8985F"))) });
        Map.Add(TEXT("Tween"),            FFeatureVisual{ TEXT("Tween"),        FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FBFE8"))) });
        Map.Add(TEXT("EntityCollection"), FFeatureVisual{ TEXT("EntityCollection"), FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8FB25F"))) });
        // Attribute types read apart: distinct glyphs AND distinct hues per value type.
        Map.Add(TEXT("FloatAttribute"),   FFeatureVisual{ TEXT("AttributeFloat"),   FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("3FA1AD"))) });
        Map.Add(TEXT("ByteAttribute"),    FFeatureVisual{ TEXT("AttributeByte"),    FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C9884F"))) });
        Map.Add(TEXT("IntegerAttribute"), FFeatureVisual{ TEXT("AttributeInteger"), FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("4F7DC9"))) });
        Map.Add(TEXT("VectorAttribute"),  FFeatureVisual{ TEXT("AttributeVector"),  FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("7DC94F"))) });

        return Map;
    }();

    return Visuals;
}

auto
    ck::ecs_debugger_feature_visuals::
    Get_FeatureGroup(
        FName InFeatureId)
    -> FName
{
    static const auto Groups = []() -> TMap<FName, FName>
    {
        auto Map = TMap<FName, FName>{};
        for (const auto& Id : { TEXT("ActorBridge"), TEXT("EntityCollection"), TEXT("SceneNode") })
        { Map.Add(Id, TEXT("Core")); }
        for (const auto& Id : { TEXT("FloatAttribute"), TEXT("ByteAttribute"), TEXT("IntegerAttribute"), TEXT("VectorAttribute") })
        { Map.Add(Id, TEXT("Attributes")); }
        for (const auto& Id : { TEXT("StateMachine"), TEXT("Aggro"), TEXT("Goap"), TEXT("Eqs") })
        { Map.Add(Id, TEXT("AI")); }
        for (const auto& Id : { TEXT("Timer"), TEXT("Probe"), TEXT("InteractionResolver"), TEXT("Objective"), TEXT("Tween") })
        { Map.Add(Id, TEXT("Gameplay")); }
        for (const auto& Id : { TEXT("IsmProxy"), TEXT("IskmProxy"), TEXT("Vfx"), TEXT("AudioTrack"), TEXT("Camera") })
        { Map.Add(Id, TEXT("Rendering")); }
        return Map;
    }();

    const auto* Found = Groups.Find(InFeatureId);
    return Found != nullptr ? *Found : FName{TEXT("Other")};
}

auto
    ck::ecs_debugger_feature_visuals::
    Get_FeatureGroupOrder()
    -> const TArray<FName>&
{
    static const auto Order = TArray<FName>{
        TEXT("Core"), TEXT("Attributes"), TEXT("AI"),
        TEXT("Gameplay"), TEXT("Rendering"), TEXT("Other")
    };
    return Order;
}

auto
    ck::ecs_debugger_feature_visuals::
    Get_ArchetypePalette()
    -> const TArray<FLinearColor>&
{
    static const auto Palette = []() -> TArray<FLinearColor>
    {
        const auto Hexes = TArray<FString>{
            TEXT("5B8DE8"), TEXT("4FB6E8"), TEXT("3FBFB0"), TEXT("4FC98A"),
            TEXT("8FCF5F"), TEXT("D9C94F"), TEXT("E8A44F"), TEXT("E8784F"),
            TEXT("E85F6B"), TEXT("E86FA8"), TEXT("D96FE8"), TEXT("A56FE8"),
            TEXT("7B6FE8"), TEXT("6F8FE8"), TEXT("5FB6A8"), TEXT("B0B85F")
        };

        auto Result = TArray<FLinearColor>{};
        for (const auto& Hex : Hexes)
        { Result.Add(FLinearColor::FromSRGBColor(FColor::FromHex(Hex))); }
        return Result;
    }();

    return Palette;
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
