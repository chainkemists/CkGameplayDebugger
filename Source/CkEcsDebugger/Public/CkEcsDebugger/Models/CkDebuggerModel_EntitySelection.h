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

    /**
     * Drop every handle owned by this model at a world/session boundary.
     * Unlike Clear_Selection, this does not make the old selection reachable
     * through Back history.
     */
    auto Reset_ForWorldChange() -> void;

    auto Get_SelectedEntities() const -> const TArray<FCk_Handle>&;
    auto Get_PrimarySelection() const -> FCk_Handle;
    auto IsSelected(const FCk_Handle& InEntity) const -> bool;
    auto Get_SelectionCount() const -> int32;

    /** Navigate to the previous selection in history. Returns true if navigation occurred. */
    auto NavigateBack() -> bool;

    /** Navigate to the next selection in history. Returns true if navigation occurred. */
    auto NavigateForward() -> bool;

    auto CanNavigateBack() const -> bool;
    auto CanNavigateForward() const -> bool;

    /** Clear all history. Does not affect current selection. */
    auto ClearHistory() -> void;

    FCkDebugger_OnSelectionChanged OnSelectionChanged;

private:
    auto ApplySelection(const TArray<FCk_Handle>& InEntities) -> void;
    auto PushToHistory() -> void;
    auto BroadcastSelectionChanged() -> void;

    TArray<FCk_Handle> SelectedEntities;
    TSet<FCk_Handle> SelectedEntitiesSet;

    // History: each entry is a snapshot of the selection at that point
    TArray<TArray<FCk_Handle>> History;
    int32 HistoryIndex = INDEX_NONE;
    bool IsNavigating = false;

    static constexpr int32 MaxHistorySize = 64;
};
