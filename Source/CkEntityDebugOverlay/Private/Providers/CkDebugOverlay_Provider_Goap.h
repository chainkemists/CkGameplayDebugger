#pragma once
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Provider.h"

// --------------------------------------------------------------------------------------------------------------------

class FCk_DebugOverlay_Provider_Goap : public ICk_DebugOverlay_Provider
{
public:
    auto Get_ProviderTag()          const -> FGameplayTag                    override;
    auto Get_FieldTags()            const -> TArray<FCk_DebugOverlay_FieldDesc> override;
    auto Get_SortPriority()         const -> int32                           override { return 23; }
    // Planners nest: a focused NPC drags in one planner section per sub-entity, most of them
    // idle. Every row matters, so nothing merges — the primary planner keeps its full section
    // and the ones behind it condense to a line each.
    auto Get_MergeBehavior()        const -> ECk_DebugOverlay_MergeBehavior   override
    { return ECk_DebugOverlay_MergeBehavior::CondensePerSource; }
    auto Get_CondensedSourceRow(const FText& InSourceName,
                                const TArray<FCk_DebugOverlay_Row>& InRows) const
                                -> FCk_DebugOverlay_Row                      override;
    auto CanProvide(const FCk_Handle& Entity) const -> bool                  override;
    auto Collect(const FCk_Handle& Entity, const FCk_DebugOverlay_ProviderConfig& Cfg,
                 FCk_DebugOverlay_Section& Out) -> void                      override;
    auto Get_CompactToken(const FCk_Handle& Entity,
                          const FCk_DebugOverlay_ProviderConfig& Cfg) const -> FString override;
};

// --------------------------------------------------------------------------------------------------------------------
