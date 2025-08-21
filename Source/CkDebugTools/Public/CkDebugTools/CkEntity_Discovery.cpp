#include "CkDebugTools/CkEntity_Discovery.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

auto FCkEntityDiscovery::DoDiscoverData() -> void
{
    _ValidEntities.Empty();
    _InvalidEntities.Empty();

    for (const auto& Entity : _CurrentEntities)
    {
        if (ck::IsValid(Entity))
        {
            _ValidEntities.Add(Entity);
        }
        else
        {
            _InvalidEntities.Add(Entity);
        }
    }
}

auto FCkEntityDiscovery::DoValidateData() -> void
{
    _TotalCount = _CurrentEntities.Num();
    _ErrorCount = _InvalidEntities.Num();
    _WarningCount = 0;

    // Check for entities without actors (might be warnings depending on use case)
    for (const auto& Entity : _ValidEntities)
    {
        if (NOT UCk_Utils_OwningActor_UE::Has(Entity))
        {
            _WarningCount++;
        }
    }
}

auto FCkEntityDiscovery::Request_ApplySearchFilter(const FString& InSearchText) -> void
{
    FCkDebugToolsDiscovery_Base::Request_ApplySearchFilter(InSearchText);

    _FilteredEntities.Empty();

    if (InSearchText.IsEmpty())
    {
        _FilteredEntities = _CurrentEntities;
    }
    else
    {
        const auto SearchTextLower = InSearchText.ToLower();

        for (const auto& Entity : _CurrentEntities)
        {
            bool ShouldInclude = false;

            // Search by entity string representation
            if (Entity.ToString().ToLower().Contains(SearchTextLower))
            {
                ShouldInclude = true;
            }

            // Search by debug name
            const auto DebugName = Entity.Get_DebugName();
            if (NOT DebugName.IsNone() && DebugName.ToString().ToLower().Contains(SearchTextLower))
            {
                ShouldInclude = true;
            }

            // Search by actor name if it has one
            if (ck::IsValid(Entity) && UCk_Utils_OwningActor_UE::Has(Entity))
            {
                const auto EntityActor = UCk_Utils_OwningActor_UE::Get_EntityOwningActor(Entity);
                if (IsValid(EntityActor) && EntityActor->GetName().ToLower().Contains(SearchTextLower))
                {
                    ShouldInclude = true;
                }
            }

            if (ShouldInclude)
            {
                _FilteredEntities.Add(Entity);
            }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------