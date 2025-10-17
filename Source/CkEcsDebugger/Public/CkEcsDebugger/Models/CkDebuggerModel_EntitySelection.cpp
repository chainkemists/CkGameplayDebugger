#include "CkDebuggerModel_EntitySelection.h"

#include "CkCore/Validation/CkIsValid.h"

auto FCkDebuggerModel_EntitySelection::Set_SelectedEntities(const TArray<FCk_Handle>& InEntities) -> void
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

    BroadcastSelectionChanged();
}

auto FCkDebuggerModel_EntitySelection::Add_SelectedEntity(const FCk_Handle& InEntity) -> void
{
    if (ck::Is_NOT_Valid(InEntity))
    { return; }

    if (SelectedEntitiesSet.Contains(InEntity))
    { return; }

    SelectedEntities.Add(InEntity);
    SelectedEntitiesSet.Add(InEntity);

    BroadcastSelectionChanged();
}

auto FCkDebuggerModel_EntitySelection::Remove_SelectedEntity(const FCk_Handle& InEntity) -> void
{
    if (NOT SelectedEntitiesSet.Contains(InEntity))
    { return; }

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

    SelectedEntities.Empty();
    SelectedEntitiesSet.Empty();

    BroadcastSelectionChanged();
}

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

auto FCkDebuggerModel_EntitySelection::BroadcastSelectionChanged() -> void
{
    OnSelectionChanged.Broadcast(SelectedEntities);
}