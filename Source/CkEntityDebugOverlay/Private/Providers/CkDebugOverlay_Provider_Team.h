#pragma once
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Provider.h"

// --------------------------------------------------------------------------------------------------------------------

class FCk_DebugOverlay_Provider_Team : public ICk_DebugOverlay_Provider
{
public:
    auto Get_ProviderTag()    const -> FGameplayTag                       override;
    auto Get_FieldTags()      const -> TArray<FCk_DebugOverlay_FieldDesc> override;
    auto Get_SortPriority()   const -> int32                              override { return 14; }
    // Team id is subtree-wide — the whole NPC is on one team, so repeating it once per
    // sub-entity is pure noise and collapses to one xN row.
    auto Get_MergeBehavior()  const -> ECk_DebugOverlay_MergeBehavior     override
    { return ECk_DebugOverlay_MergeBehavior::MergeAcrossSources; }
    auto CanProvide(const FCk_Handle& Entity) const -> bool               override;
    auto Collect(const FCk_Handle& Entity, const FCk_DebugOverlay_ProviderConfig& Cfg,
                 FCk_DebugOverlay_Section& Out) -> void                   override;
    auto Get_CompactToken(const FCk_Handle& Entity,
                          const FCk_DebugOverlay_ProviderConfig& Cfg) const -> FString override;
};

// --------------------------------------------------------------------------------------------------------------------
