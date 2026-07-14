#pragma once
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Provider.h"

// --------------------------------------------------------------------------------------------------------------------

class FCk_DebugOverlay_Provider_PathNetworkFollower : public ICk_DebugOverlay_Provider
{
public:
    auto Get_ProviderTag()    const -> FGameplayTag                       override;
    auto Get_FieldTags()      const -> TArray<FCk_DebugOverlay_FieldDesc> override;
    auto Get_SortPriority()   const -> int32                              override { return 26; }
    auto CanProvide(const FCk_Handle& Entity) const -> bool               override;
    auto Collect(const FCk_Handle& Entity, const FCk_DebugOverlay_ProviderConfig& Cfg,
                 FCk_DebugOverlay_Section& Out) -> void                   override;
    auto Get_CompactToken(const FCk_Handle& Entity,
                          const FCk_DebugOverlay_ProviderConfig& Cfg) const -> FString override;
};

// --------------------------------------------------------------------------------------------------------------------
