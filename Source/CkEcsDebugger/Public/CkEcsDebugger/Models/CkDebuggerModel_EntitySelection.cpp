#include "CkDebuggerModel_EntitySelection.h"

#include "CkCore/Validation/CkIsValid.h"

// =====================================================================================================================

auto FCkDebuggerModel_EntitySelection::Set_SelectedEntities(const TArray<FCk_Handle>& InEntities) -> void
{
    if (NOT bIsNavigating)
    {
        PushToHistory();
    }

    ApplySelection(InEntities);
    BroadcastSelectionChanged();
}

auto FCkDebuggerModel_EntitySelection::Add_SelectedEntity(const FCk_Handle& InEntity) -> void
{
    if (ck::Is_NOT_Valid(InEntity))
    { return; }

    if (SelectedEntitiesSet.Contains(InEntity))
    { return; }

    if (NOT bIsNavigating)
    {
        PushToHistory();
    }

    SelectedEntities.Add(InEntity);
    SelectedEntitiesSet.Add(InEntity);

    BroadcastSelectionChanged();
}

auto FCkDebuggerModel_EntitySelection::Remove_SelectedEntity(const FCk_Handle& InEntity) -> void
{
    if (NOT SelectedEntitiesSet.Contains(InEntity))
    { return; }

    if (NOT bIsNavigating)
    {
        PushToHistory();
    }

    SelectedEntities.Remove(InEntity);
    SelectedEntitiesSet.Remove(InEntity);

    BroadcastSelectionChanged();
}

auto FCkDebuggerModel_EntitySelection::Toggle_SelectedEntity(const FCk_Handle& InEntity) -> void
{
    if (ck::Is_NOT_Valid(InEntity))
    { return; }

    if (SelectedEntitiesSet.Contains(InEntity))
    {
        Remove_SelectedEntity(InEntity);
    }
    else
    {
        Add_SelectedEntity(InEntity);
    }
}

auto FCkDebuggerModel_EntitySelection::Clear_Selection() -> void
{
    if (SelectedEntities.IsEmpty())
    { return; }

    if (NOT bIsNavigating)
    {
        PushToHistory();
    }

    SelectedEntities.Empty();
    SelectedEntitiesSet.Empty();

    BroadcastSelectionChanged();
}

// =====================================================================================================================

auto FCkDebuggerModel_EntitySelection::Get_SelectedEntities() const -> const TArray<FCk_Handle>&
{
    return SelectedEntities;
}

auto FCkDebuggerModel_EntitySelection::Get_PrimarySelection() const -> FCk_Handle
{
    if (SelectedEntities.IsEmpty())
    { return FCk_Handle{}; }

    return SelectedEntities[0];
}

auto FCkDebuggerModel_EntitySelection::IsSelected(const FCk_Handle& InEntity) const -> bool
{
    return SelectedEntitiesSet.Contains(InEntity);
}

auto FCkDebuggerModel_EntitySelection::Get_SelectionCount() const -> int32
{
    return SelectedEntities.Num();
}

// =====================================================================================================================

auto FCkDebuggerModel_EntitySelection::NavigateBack() -> bool
{
    if (NOT CanNavigateBack())
    { return false; }

    // If at the tip, save current state so forward navigation returns here
    if (HistoryIndex == INDEX_NONE || HistoryIndex == History.Num() - 1)
    {
        PushToHistory();
        HistoryIndex--;
    }

    HistoryIndex--;

    bIsNavigating = true;
    ApplySelection(History[HistoryIndex]);
    BroadcastSelectionChanged();
    bIsNavigating = false;

    return true;
}

auto FCkDebuggerModel_EntitySelection::NavigateForward() -> bool
{
    if (NOT CanNavigateForward())
    { return false; }

    HistoryIndex++;

    bIsNavigating = true;
    ApplySelection(History[HistoryIndex]);
    BroadcastSelectionChanged();
    bIsNavigating = false;

    return true;
}

auto FCkDebuggerModel_EntitySelection::CanNavigateBack() const -> bool
{
    if (History.IsEmpty())
    { return false; }

    if (HistoryIndex == INDEX_NONE)
    { return History.Num() > 0; }

    return HistoryIndex > 0;
}

auto FCkDebuggerModel_EntitySelection::CanNavigateForward() const -> bool
{
    if (History.IsEmpty() || HistoryIndex == INDEX_NONE)
    { return false; }

    return HistoryIndex < History.Num() - 1;
}

auto FCkDebuggerModel_EntitySelection::ClearHistory() -> void
{
    History.Empty();
    HistoryIndex = INDEX_NONE;
}

// =====================================================================================================================

auto FCkDebuggerModel_EntitySelection::ApplySelection(const TArray<FCk_Handle>& InEntities) -> void
{
    SelectedEntities.Empty();
    SelectedEntitiesSet.Empty();

    for (const auto& Entity : InEntities)
    {
        if (ck::Is_NOT_Valid(Entity))
        { continue; }

        SelectedEntities.AddUnique(Entity);
        SelectedEntitiesSet.Add(Entity);
    }
}

auto FCkDebuggerModel_EntitySelection::PushToHistory() -> void
{
    if (SelectedEntities.IsEmpty())
    { return; }

    // Discard forward history when making a new selection after navigating back
    if (HistoryIndex != INDEX_NONE && HistoryIndex < History.Num() - 1)
    {
        History.SetNum(HistoryIndex + 1);
    }

    // Don't push duplicates
    if (History.Num() > 0 && History.Last() == SelectedEntities)
    { return; }

    History.Add(SelectedEntities);

    // Cap history size
    if (History.Num() > MaxHistorySize)
    {
        History.RemoveAt(0);
    }

    HistoryIndex = History.Num() - 1;
}

auto FCkDebuggerModel_EntitySelection::BroadcastSelectionChanged() -> void
{
    OnSelectionChanged.Broadcast(SelectedEntities);
}
