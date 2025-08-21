#pragma once

#include "CkEcs/Handle/CkHandle.h"

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGTOOLS_API FCkDebugToolsDiscovery_Base
{
public:
    virtual ~FCkDebugToolsDiscovery_Base() = default;

    // Common data patterns
    virtual auto Request_RefreshFromEntities(const TArray<FCk_Handle>& InEntities) -> void;
    virtual auto Request_ApplySearchFilter(const FString& InSearchText) -> void;
    virtual auto Request_ValidateData() -> void;
    auto Get_ValidationStats() const -> TTuple<int32, int32, int32> { return {_TotalCount, _ErrorCount, _WarningCount}; }

    // Virtual methods for specific data handling
    virtual auto DoDiscoverData() -> void = 0;
    virtual auto DoValidateData() -> void = 0;

protected:
    TArray<FCk_Handle> _CurrentEntities;
    FString _CurrentSearchFilter;
    int32 _TotalCount = 0;
    int32 _ErrorCount = 0;
    int32 _WarningCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------