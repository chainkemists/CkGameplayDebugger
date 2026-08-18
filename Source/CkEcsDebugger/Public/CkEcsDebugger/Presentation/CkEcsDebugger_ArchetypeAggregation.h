#pragma once

#include "CkEditorTools/Style/CkIcons_Generated.h"

struct FSlateBrush;
#include "CkEcs/Handle/CkHandle.h"

#include <CoreMinimal.h>

// --------------------------------------------------------------------------------------------------------------------
// Shared archetype aggregation (spec §3.3 consumption): entities → per-archetype buckets
// with registry-first keying and fully-resolved presentation identity (glyph, accent
// color, family). Consumed by the Archetypes grid and the Overview dashboard so both
// pages agree on keys, icons, and colors.
//
// Icon precedence: registered bespoke glyph → dominant-feature glyph (inferred buckets
// only) → stable hash pick from the general pool → Cube (pool empty).
// Color precedence: feature color (with feature glyph) → registered descriptor color
// (when not the White default) → stable hash pick from the archetype palette.
// Family: descriptor Family (None → "Game") for registered; "Internals" for classified
// internals; else the dominant feature id; else "Other".
// --------------------------------------------------------------------------------------------------------------------

namespace ck::ecs_debugger_aggregation
{
    struct FArchetypeBucket
    {
        FString Key;            // aggregation key — also the card-cache identity
        FString DisplayName;
        FString FilterToken;    // toggled as arch:<token> into the filter
        int32 Count = 0;
        uint64 SignatureBits = 0;
        bool IsRegistered = false;
        ECk_Icon Icon = ECk_Icon::None;
        FName DynamicIcon;      // game-supplied (archetype descriptor IconSvgPath) — wins over Icon when set
        FLinearColor IconColor = FLinearColor::White;
        FName Family;
        FCk_Handle Representative;   // first live instance — singleton rows link through it

        // The single resolve point for a bucket glyph: the dynamic (game) lane when set, else the typed icon.
        CKECSDEBUGGER_API auto Get_IconBrush() const -> const FSlateBrush*;
    };

    // Aggregates and sorts (registered first, then population, then key — deterministic
    // so widget caches keyed on the order stay stable).
    CKECSDEBUGGER_API auto Aggregate(
        const TArray<FCk_Handle>& InEntities) -> TArray<FArchetypeBucket>;
}
