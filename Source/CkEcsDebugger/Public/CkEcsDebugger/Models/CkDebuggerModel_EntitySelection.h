#pragma once

#include "CkEcs/Handle/CkHandle.h"

class FCkDebuggerModel_EntitySelection
{
public:
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnSelectionChanged, const TArray<FCk_Handle>&);

    auto Set_SelectedEntities(const TArray<FCk_Handle>& InEntities) -> void;
    auto Add_SelectedEntity(const FCk_Handle& InEntity) -> void;
    auto Remove_SelectedEntity(const FCk_Handle& InEntity) -> void;
    auto Toggle_SelectedEntity(const FCk_Handle& InEntity) -> void;
    auto Clear_Selection() -> void;

    auto Get_SelectedEntities() const -> const TArray<FCk_Handle>&
    {
        return SelectedEntities;
    }

    auto Get_PrimarySelection() const -> FCk_Handle
    {
        return SelectedEntities.Num() > 0 ? SelectedEntities[0] : FCk_Handle{};
    }

    auto IsSelected(const FCk_Handle& InEntity) const -> bool
    {
        return SelectedEntitiesSet.Contains(InEntity);
    }

    FOnSelectionChanged OnSelectionChanged;

private:
    auto BroadcastChange() -> void;

    TArray<FCk_Handle> SelectedEntities;
    TSet<FCk_Handle> SelectedEntitiesSet;
};