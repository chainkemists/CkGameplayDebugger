#pragma once
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Provider.h"

// --------------------------------------------------------------------------------------------------------------------

class FCk_DebugOverlay_Provider_StateMachine : public ICk_DebugOverlay_Provider
{
public:
    auto Get_ProviderTag()          const -> FGameplayTag                    override;
    auto Get_FieldTags()            const -> TArray<FCk_DebugOverlay_FieldDesc> override;
    auto Get_SortPriority()         const -> int32                           override { return 50; }
    // Sub-state-machines are hierarchical and each carries its own transition trail, which a
    // value-identical merge would silently drop. They condense to one line per sub-entity instead.
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
