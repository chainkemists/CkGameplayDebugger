#pragma once

#include "CoreMinimal.h"
#include "CkEcs/Handle/CkHandle.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FCkDebugger_OnSelectionChanged, const TArray<FCk_Handle>&);

class FCkDebuggerModel_EntitySelection
{
public:
    auto Set_SelectedEntities(const TArray<FCk_Handle>& InEntities) -> void;
    auto Add_SelectedEntity(const FCk_Handle& InEntity) -> void;
    auto Remove_SelectedEntity(const FCk_Handle& InEntity) -> void;
    auto Toggle_SelectedEntity(const FCk_Handle& InEntity) -> void;
    auto Clear_Selection() -> void;

    auto Get_SelectedEntities() const -> const TArray<FCk_Handle>&;
    auto Get_PrimarySelection() const -> FCk_Handle;
    auto IsSelected(const FCk_Handle& InEntity) const -> bool;
    auto Get_SelectionCount() const -> int32;

    FCkDebugger_OnSelectionChanged OnSelectionChanged;

private:
    auto BroadcastSelectionChanged() -> void;

    TArray<FCk_Handle> SelectedEntities;
    TSet<FCk_Handle> SelectedEntitiesSet;
};