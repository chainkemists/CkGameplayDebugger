#include "CkDebugTools_Base_Discovery.h"

// --------------------------------------------------------------------------------------------------------------------

auto FCkDebugToolsDiscovery_Base::Request_RefreshFromEntities(const TArray<FCk_Handle>& InEntities) -> void
{
    _CurrentEntities = InEntities;
    DoDiscoverData();
    DoValidateData();
    Request_ApplySearchFilter(_CurrentSearchFilter);
}

auto FCkDebugToolsDiscovery_Base::Request_ApplySearchFilter(const FString& InSearchText) -> void
{
    _CurrentSearchFilter = InSearchText;
    // Base implementation - derived classes should override for specific filtering
}

auto FCkDebugToolsDiscovery_Base::Request_ValidateData() -> void
{
    _TotalCount = 0;
    _ErrorCount = 0;
    _WarningCount = 0;
    DoValidateData();
}

// --------------------------------------------------------------------------------------------------------------------