#pragma once

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Provider.h"

// --------------------------------------------------------------------------------------------------------------------

class FCk_DebugOverlay_Provider_RotateTowards : public ICk_DebugOverlay_Provider
{
public:
    auto Get_ProviderTag() const -> FGameplayTag override;
    auto Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc> override;
    auto Get_SortPriority() const -> int32 override { return 56; }
    auto CanProvide(const FCk_Handle& InEntity) const -> bool override;
    auto Collect(const FCk_Handle& InEntity, const FCk_DebugOverlay_ProviderConfig& InConfig,
        FCk_DebugOverlay_Section& OutSection) -> void override;
    auto Get_CompactToken(const FCk_Handle& InEntity,
        const FCk_DebugOverlay_ProviderConfig& InConfig) const -> FString override;
};

// --------------------------------------------------------------------------------------------------------------------
