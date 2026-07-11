#pragma once

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
        FName IconName;
        FLinearColor Color = FLinearColor::White;
    };

    // Wired inspectors contribute their declared icon/color via registry metadata;
    // flag-only features (no parity-wired inspector) get manual rows. Built once —
    // flag registration is startup-only.
    CKECSDEBUGGER_API auto Get_FeatureVisuals() -> const TMap<FName, FFeatureVisual>&;

    // (feature id, bit) pairs for decoding own-bit masks into badges/chips; skips
    // Transform/Label and underscore-prefixed infrastructure ids.
    CKECSDEBUGGER_API auto Get_BadgeFeatures() -> const TArray<TPair<FName, int32>>&;

    constexpr auto MaxBadges = 6;
}
