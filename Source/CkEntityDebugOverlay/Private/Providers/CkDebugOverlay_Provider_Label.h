#pragma once
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Provider.h"

// --------------------------------------------------------------------------------------------------------------------

class FCk_DebugOverlay_Provider_Label : public ICk_DebugOverlay_Provider
{
public:
    auto Get_ProviderTag()    const -> FGameplayTag                       override;
    auto Get_FieldTags()      const -> TArray<FCk_DebugOverlay_FieldDesc> override;
    auto Get_SortPriority()   const -> int32                              override { return 16; }
    // A gameplay label is a subtree-wide identity: every sub-entity reporting "Scout" is the
    // same fact, so duplicates across sources are noise and collapse to one xN row.
    auto Get_MergeBehavior()  const -> ECk_DebugOverlay_MergeBehavior     override
    { return ECk_DebugOverlay_MergeBehavior::MergeAcrossSources; }
    auto CanProvide(const FCk_Handle& Entity) const -> bool               override;
    auto Collect(const FCk_Handle& Entity, const FCk_DebugOverlay_ProviderConfig& Cfg,
                 FCk_DebugOverlay_Section& Out) -> void                   override;
    auto Get_CompactToken(const FCk_Handle& Entity,
                          const FCk_DebugOverlay_ProviderConfig& Cfg) const -> FString override;
};

// --------------------------------------------------------------------------------------------------------------------
