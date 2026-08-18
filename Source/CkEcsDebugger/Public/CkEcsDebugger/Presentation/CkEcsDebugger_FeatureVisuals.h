#pragma once

#include "CkEditorTools/Style/CkIcons_Generated.h"

#include <CoreMinimal.h>

// --------------------------------------------------------------------------------------------------------------------
// Shared feature-presentation metadata: flag-feature-id → glyph + accent color, and the
// badge-worthy feature list (structural carriers and infrastructure ids excluded).
// Consumed by the entity tree's badge strips, the feature rail, and the Overview cards.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::ecs_debugger_feature_visuals
{
    struct FFeatureVisual
    {
        ECk_Icon Icon = ECk_Icon::None;
        FLinearColor Color = FLinearColor::White;
    };

    // Wired inspectors contribute their declared icon/color via registry metadata;
    // flag-only features (no parity-wired inspector) get manual rows. Built once —
    // flag registration is startup-only.
    CKECSDEBUGGER_API auto Get_FeatureVisuals() -> const TMap<FName, FFeatureVisual>&;

    // (feature id, bit) pairs for decoding own-bit masks into badges/chips; skips
    // Transform/Label and underscore-prefixed infrastructure ids.
    CKECSDEBUGGER_API auto Get_BadgeFeatures() -> const TArray<TPair<FName, int32>>&;

    // Rail grouping: feature id → group (Core/Attributes/AI/Gameplay/Rendering; unknown
    // ids fall into Other) and the display order of those groups.
    CKECSDEBUGGER_API auto Get_FeatureGroup(FName InFeatureId) -> FName;
    CKECSDEBUGGER_API auto Get_FeatureGroupOrder() -> const TArray<FName>&;

    // Curated accent hues for archetype identity (dark-UI tuned). Assign by stable hash
    // of the archetype key so an archetype keeps its color across runs.
    CKECSDEBUGGER_API auto Get_ArchetypePalette() -> const TArray<FLinearColor>&;

    constexpr auto MaxBadges = 6;
}
