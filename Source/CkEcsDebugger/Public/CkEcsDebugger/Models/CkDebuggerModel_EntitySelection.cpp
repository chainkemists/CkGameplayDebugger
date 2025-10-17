#include "CkDebuggerModel_EntitySelection.h"

#include "CkCore/Validation/CkIsValid.h"

auto FCkDebuggerModel_EntitySelection::Set_SelectedEntities(const TArray<FCk_Handle>& InEntities) -> void
{
    SelectedEntities.Empty(InEntities.Num());
    SelectedEntitiesSet.Empty(InEntities.Num());

    for (const auto& Entity : InEntities)
    {
        if (ck::Is_NOT_Valid(Entity))
        { continue; }

        SelectedEntities.Add(Entity);
        SelectedEntitiesSet.Add(Entity);
    }

    BroadcastChange();
}

auto FCkDebuggerModel_EntitySelection::Add_SelectedEntity(const FCk_Handle& InEntity) -> void
{
    if (ck::Is_NOT_Valid(InEntity))
    { return; }

    if (SelectedEntitiesSet.Contains(InEntity))
    { return; }

    SelectedEntities.Add(InEntity);
    SelectedEntitiesSet.Add(InEntity);

    BroadcastChange();
}

auto FCkDebuggerModel_EntitySelection::Remove_SelectedEntity(const FCk_Handle& InEntity) -> void
{
    if (NOT SelectedEntitiesSet.Contains(InEntity))
    { return; }

    SelectedEntities.Remove(InEntity);
    SelectedEntitiesSet.Remove(InEntity);

    BroadcastChange();
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
    if (SelectedEntities.Num() == 0)
    { return; }

    SelectedEntities.Empty();
    SelectedEntitiesSet.Empty();

    BroadcastChange();
}

auto FCkDebuggerModel_EntitySelection::BroadcastChange() -> void
{
    OnSelectionChanged.Broadcast(SelectedEntities);
}