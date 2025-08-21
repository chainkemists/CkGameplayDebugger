#pragma once

#include "CkDebugTools/CkDebugTools_Base_Discovery.h"

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGTOOLS_API FCkEntityDiscovery : public FCkDebugToolsDiscovery_Base
{
public:
    auto DoDiscoverData() -> void override;
    auto DoValidateData() -> void override;
    auto Request_ApplySearchFilter(const FString& InSearchText) -> void override;

    auto Get_FilteredEntities() const -> const TArray<FCk_Handle>& { return _FilteredEntities; }

private:
    TArray<FCk_Handle> _ValidEntities;
    TArray<FCk_Handle> _InvalidEntities;
    TArray<FCk_Handle> _FilteredEntities;
};

// --------------------------------------------------------------------------------------------------------------------