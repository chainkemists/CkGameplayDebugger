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
            if (Metadata.FeatureFlagId.IsNone() || Metadata.Icon == ECk_Icon::None)
            { continue; }

            Map.Add(Metadata.FeatureFlagId, FFeatureVisual{
                Metadata.Icon,
                Metadata.Color.Get(FLinearColor::White) });
        }

        Map.Add(TEXT("StateMachine"),     FFeatureVisual{ ECk_Icon::StateMachine, FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8F6FE8"))) });
        Map.Add(TEXT("Aggro"),            FFeatureVisual{ ECk_Icon::Aggro,        FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C94F4F"))) });
        Map.Add(TEXT("AudioTrack"),       FFeatureVisual{ ECk_Icon::Audio,        FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("4FA3C9"))) });
        Map.Add(TEXT("Label"),            FFeatureVisual{ ECk_Icon::Label,        FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8B93A1"))) });
        Map.Add(TEXT("Objective"),        FFeatureVisual{ ECk_Icon::Objective,    FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D9B23F"))) });
        Map.Add(TEXT("VfxCue"),           FFeatureVisual{ ECk_Icon::Vfx,          FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("F06FD0"))) });
        Map.Add(TEXT("Camera"),           FFeatureVisual{ ECk_Icon::Camera,       FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("6FA8E8"))) });
        Map.Add(TEXT("Goap"),             FFeatureVisual{ ECk_Icon::Goap,         FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("B06FE8"))) });
        Map.Add(TEXT("Eqs"),              FFeatureVisual{ ECk_Icon::Eqs,          FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FD0A0"))) });
        Map.Add(TEXT("IsmProxy"),         FFeatureVisual{ ECk_Icon::IsmRenderer,  FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("7F8FE8"))) });
        Map.Add(TEXT("IskmProxy"),        FFeatureVisual{ ECk_Icon::IsmRenderer,  FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("9F7FE8"))) });
        Map.Add(TEXT("ActorBridge"),      FFeatureVisual{ ECk_Icon::ActorBridge,  FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E8985F"))) });
        Map.Add(TEXT("Tween"),            FFeatureVisual{ ECk_Icon::Tween,        FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FBFE8"))) });
        Map.Add(TEXT("EntityCollection"), FFeatureVisual{ ECk_Icon::EntityCollection, FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8FB25F"))) });
        // Attribute types read apart: distinct glyphs AND distinct hues per value type.
        Map.Add(TEXT("FloatAttribute"),   FFeatureVisual{ ECk_Icon::AttributeFloat,   FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("3FA1AD"))) });
        Map.Add(TEXT("ByteAttribute"),    FFeatureVisual{ ECk_Icon::AttributeByte,    FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C9884F"))) });
        Map.Add(TEXT("IntegerAttribute"), FFeatureVisual{ ECk_Icon::AttributeInteger, FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("4F7DC9"))) });
        Map.Add(TEXT("VectorAttribute"),  FFeatureVisual{ ECk_Icon::AttributeVector,  FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("7DC94F"))) });
        Map.Add(TEXT("RotatorAttribute"), FFeatureVisual{ ECk_Icon::AttributeRotator, FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C95F9E"))) });

        // Third flag batch (2026-07-11 audit): bespoke glyphs where the feature earned one,
        // otherwise a hand-picked semantic match from the general pool (Icons/General/).
        Map.Add(TEXT("EntityExtension"),    FFeatureVisual{ ECk_Icon::Connection,            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8FA0C9"))) });
        Map.Add(TEXT("UnrealComponent"),    FFeatureVisual{ ECk_Icon::Settings,            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("9AA6B8"))) });
        Map.Add(TEXT("Snapshot"),           FFeatureVisual{ ECk_Icon::SaveKey,             FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("4FC9B8"))) });
        Map.Add(TEXT("TagSet"),             FFeatureVisual{ ECk_Icon::Ticket,          FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C9A44F"))) });
        Map.Add(TEXT("EntityTag"),          FFeatureVisual{ ECk_Icon::Edit,          FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D9BC5F"))) });
        Map.Add(TEXT("CrowdAgent"),         FFeatureVisual{ ECk_Icon::Crowd,          FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E8A05F"))) });
        Map.Add(TEXT("Grid"),               FFeatureVisual{ ECk_Icon::Grid,            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5F9FD0"))) });
        Map.Add(TEXT("Marker"),             FFeatureVisual{ ECk_Icon::Aim,       FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E86F6B"))) });
        Map.Add(TEXT("Sensor"),             FFeatureVisual{ ECk_Icon::Broadcast,         FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FD0C9"))) });
        Map.Add(TEXT("RaySense"),           FFeatureVisual{ ECk_Icon::Power,            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E8D05F"))) });
        Map.Add(TEXT("Velocity"),           FFeatureVisual{ ECk_Icon::Wind,            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("7FC9E8"))) });
        Map.Add(TEXT("Spline"),             FFeatureVisual{ ECk_Icon::Signal,            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FB8D9"))) });
        Map.Add(TEXT("InteractSource"),     FFeatureVisual{ ECk_Icon::Grab,            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E8B45F"))) });
        Map.Add(TEXT("InteractTarget"),     FFeatureVisual{ ECk_Icon::Target,          FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FC97D"))) });
        Map.Add(TEXT("Inventory"),          FFeatureVisual{ ECk_Icon::Loadout,        FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("B8895F"))) });
        Map.Add(TEXT("Item"),               FFeatureVisual{ ECk_Icon::Currency,            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E0B93F"))) });
        Map.Add(TEXT("Team"),               FFeatureVisual{ ECk_Icon::Objective,            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("4F9FE0"))) });
        Map.Add(TEXT("Player"),             FFeatureVisual{ ECk_Icon::Actor,          FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FD08F"))) });
        Map.Add(TEXT("Projectile"),         FFeatureVisual{ ECk_Icon::Projectile, FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E8845F"))) });
        Map.Add(TEXT("ResolverSource"),     FFeatureVisual{ ECk_Icon::Combat,           FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E86F5F"))) });
        Map.Add(TEXT("ResolverTarget"),     FFeatureVisual{ ECk_Icon::Protection,          FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5F8FE8"))) });
        Map.Add(TEXT("GeometryCollection"), FFeatureVisual{ ECk_Icon::Destructive,            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D96F5F"))) });
        Map.Add(TEXT("AnimPlan"),           FFeatureVisual{ ECk_Icon::Catalog,            FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("A87FE8"))) });
        Map.Add(TEXT("MontagePlayer"),      FFeatureVisual{ ECk_Icon::Cinematic,        FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D97FB8"))) });
        Map.Add(TEXT("VatProxy"),           FFeatureVisual{ ECk_Icon::SaveSlot,        FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("9F8FD9"))) });
        Map.Add(TEXT("RenderTarget"),       FFeatureVisual{ ECk_Icon::Screen,              FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("6F9FE8"))) });
        Map.Add(TEXT("WorldSpaceWidget"),   FFeatureVisual{ ECk_Icon::Display,         FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8FD0E8"))) });
        Map.Add(TEXT("CameraShake"),        FFeatureVisual{ ECk_Icon::Recording,       FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E87F9F"))) });
        Map.Add(TEXT("Vfx"),                FFeatureVisual{ ECk_Icon::HotPath,           FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E87FD0"))) });
        Map.Add(TEXT("AudioDirector"),      FFeatureVisual{ ECk_Icon::Channel,           FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D9A45F"))) });
        Map.Add(TEXT("Sfx"),                FFeatureVisual{ ECk_Icon::Speaker,      FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("E8A87F"))) });

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
