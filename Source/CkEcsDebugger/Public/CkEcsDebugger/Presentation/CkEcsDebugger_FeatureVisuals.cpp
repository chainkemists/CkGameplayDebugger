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
        Map.Add(TEXT("VfxCue"),           FFeatureVisual{ TEXT("Vfx"),          FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("F06FD0"))) });
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
        Map.Add(TEXT("RotatorAttribute"), FFeatureVisual{ TEXT("AttributeRotator"), FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C95F9E"))) });

        // Third flag batch (2026-07-11 audit): bespoke glyphs where the feature earned one,
        // otherwise a hand-picked semantic match from the general pool (Icons/General/).
        Map.Add(TEXT("EntityExtension"),    FFeatureVisual{ TEXT("Plug"),            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8FA0C9"))) });
        Map.Add(TEXT("UnrealComponent"),    FFeatureVisual{ TEXT("Gear"),            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("9AA6B8"))) });
        Map.Add(TEXT("Snapshot"),           FFeatureVisual{ TEXT("Key"),             FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("4FC9B8"))) });
        Map.Add(TEXT("TagSet"),             FFeatureVisual{ TEXT("Ticket"),          FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C9A44F"))) });
        Map.Add(TEXT("EntityTag"),          FFeatureVisual{ TEXT("Pencil"),          FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D9BC5F"))) });
        Map.Add(TEXT("CrowdAgent"),         FFeatureVisual{ TEXT("People"),          FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E8A05F"))) });
        Map.Add(TEXT("Grid"),               FFeatureVisual{ TEXT("Grid"),            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5F9FD0"))) });
        Map.Add(TEXT("Marker"),             FFeatureVisual{ TEXT("Crosshair"),       FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E86F6B"))) });
        Map.Add(TEXT("Sensor"),             FFeatureVisual{ TEXT("Antenna"),         FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FD0C9"))) });
        Map.Add(TEXT("RaySense"),           FFeatureVisual{ TEXT("Bolt"),            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E8D05F"))) });
        Map.Add(TEXT("Velocity"),           FFeatureVisual{ TEXT("Wind"),            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("7FC9E8"))) });
        Map.Add(TEXT("Spline"),             FFeatureVisual{ TEXT("Wave"),            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FB8D9"))) });
        Map.Add(TEXT("InteractSource"),     FFeatureVisual{ TEXT("Hand"),            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E8B45F"))) });
        Map.Add(TEXT("InteractTarget"),     FFeatureVisual{ TEXT("Target"),          FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FC97D"))) });
        Map.Add(TEXT("Inventory"),          FFeatureVisual{ TEXT("Backpack"),        FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("B8895F"))) });
        Map.Add(TEXT("Item"),               FFeatureVisual{ TEXT("Coin"),            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E0B93F"))) });
        Map.Add(TEXT("Team"),               FFeatureVisual{ TEXT("Flag"),            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("4F9FE0"))) });
        Map.Add(TEXT("Player"),             FFeatureVisual{ TEXT("Person"),          FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FD08F"))) });
        Map.Add(TEXT("Projectile"),         FFeatureVisual{ TEXT("ArrowProjectile"), FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E8845F"))) });
        Map.Add(TEXT("ResolverSource"),     FFeatureVisual{ TEXT("Sword"),           FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E86F5F"))) });
        Map.Add(TEXT("ResolverTarget"),     FFeatureVisual{ TEXT("Shield"),          FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5F8FE8"))) });
        Map.Add(TEXT("GeometryCollection"), FFeatureVisual{ TEXT("Bomb"),            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D96F5F"))) });
        Map.Add(TEXT("AnimPlan"),           FFeatureVisual{ TEXT("Book"),            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("A87FE8"))) });
        Map.Add(TEXT("MontagePlayer"),      FFeatureVisual{ TEXT("FilmReel"),        FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D97FB8"))) });
        Map.Add(TEXT("VatProxy"),           FFeatureVisual{ TEXT("Cassette"),        FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("9F8FD9"))) });
        Map.Add(TEXT("RenderTarget"),       FFeatureVisual{ TEXT("Tv"),              FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("6F9FE8"))) });
        Map.Add(TEXT("WorldSpaceWidget"),   FFeatureVisual{ TEXT("Monitor"),         FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8FD0E8"))) });
        Map.Add(TEXT("CameraShake"),        FFeatureVisual{ TEXT("Camcorder"),       FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E87F9F"))) });
        Map.Add(TEXT("Vfx"),                FFeatureVisual{ TEXT("Flame"),           FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E87FD0"))) });
        Map.Add(TEXT("AudioDirector"),      FFeatureVisual{ TEXT("Radio"),           FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D9A45F"))) });
        Map.Add(TEXT("Sfx"),                FFeatureVisual{ TEXT("SpeakerBox"),      FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E8A87F"))) });

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
        for (const auto& Id : { TEXT("ActorBridge"), TEXT("UnrealComponent"), TEXT("EntityExtension"), TEXT("EntityCollection"), TEXT("Snapshot") })
        { Map.Add(Id, TEXT("Core")); }
        for (const auto& Id : { TEXT("Label"), TEXT("TagSet"), TEXT("EntityTag") })
        { Map.Add(Id, TEXT("Tags")); }
        for (const auto& Id : { TEXT("FloatAttribute"), TEXT("ByteAttribute"), TEXT("IntegerAttribute"), TEXT("VectorAttribute"), TEXT("RotatorAttribute") })
        { Map.Add(Id, TEXT("Attributes")); }
        for (const auto& Id : { TEXT("StateMachine"), TEXT("Aggro"), TEXT("Goap"), TEXT("Eqs"), TEXT("CrowdAgent") })
        { Map.Add(Id, TEXT("AI")); }
        for (const auto& Id : { TEXT("Transform"), TEXT("SceneNode"), TEXT("Grid"), TEXT("Probe"), TEXT("Marker"), TEXT("Sensor"), TEXT("RaySense") })
        { Map.Add(Id, TEXT("Spatial")); }
        for (const auto& Id : { TEXT("Velocity"), TEXT("Tween"), TEXT("Spline") })
        { Map.Add(Id, TEXT("Motion")); }
        for (const auto& Id : { TEXT("Timer"), TEXT("InteractionResolver"), TEXT("InteractSource"), TEXT("InteractTarget"), TEXT("Objective"), TEXT("Inventory"), TEXT("Item"), TEXT("Team"), TEXT("Player") })
        { Map.Add(Id, TEXT("Gameplay")); }
        for (const auto& Id : { TEXT("Projectile"), TEXT("ResolverSource"), TEXT("ResolverTarget"), TEXT("GeometryCollection") })
        { Map.Add(Id, TEXT("Combat")); }
        for (const auto& Id : { TEXT("AnimPlan"), TEXT("MontagePlayer") })
        { Map.Add(Id, TEXT("Animation")); }
        for (const auto& Id : { TEXT("IsmProxy"), TEXT("IskmProxy"), TEXT("VatProxy"), TEXT("VfxCue"), TEXT("Vfx"), TEXT("RenderTarget"), TEXT("WorldSpaceWidget"), TEXT("Camera"), TEXT("CameraShake") })
        { Map.Add(Id, TEXT("Rendering")); }
        for (const auto& Id : { TEXT("AudioTrack"), TEXT("AudioDirector"), TEXT("Sfx") })
        { Map.Add(Id, TEXT("Audio")); }
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
        TEXT("Core"), TEXT("Tags"), TEXT("Attributes"), TEXT("AI"),
        TEXT("Spatial"), TEXT("Motion"), TEXT("Gameplay"), TEXT("Combat"),
        TEXT("Animation"), TEXT("Rendering"), TEXT("Audio"), TEXT("Other")
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
            // Label stays excluded (the name column already IS the label); Transform
            // is deliberately IN — near-universal, but its absence marks transformless
            // logic entities, which is exactly the signal worth a badge/rail chip.
            if (FeatureId == TEXT("Label") || FeatureId.ToString().StartsWith(TEXT("_")))
            { continue; }

            const auto Bit = debug_feature_flags::Get_BitIndex(FeatureId);
            if (Bit != INDEX_NONE)
            { Result.Emplace(FeatureId, Bit); }
        }
        return Result;
    }();

    return Features;
}
