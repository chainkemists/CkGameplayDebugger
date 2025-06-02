#include "CkEcs_DebugWindow.h"

#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include "CkEcsDebugger/Subsystem/CkEcsDebugger_Subsystem.h"

#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Utils.h"

#include <Engine/World.h>

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_Ecs_DebugWindow::
    Get_SelectionEntity()
    -> FCk_Handle
{
    QUICK_SCOPE_CYCLE_COUNTER(Get_SelectionEntity)
    const auto World = GetWorld();

    if (ck::Is_NOT_Valid(World))
    { return {}; }

    const auto& DebuggerSubsystem = World->GetSubsystem<UCk_EcsDebugger_Subsystem_UE>();

    CK_ENSURE_IF_NOT(ck::IsValid(DebuggerSubsystem),
        TEXT("No valid ecs debugger subsystem found on world [{}]"), World)
    { return {}; }

    auto SelectionEntity = DebuggerSubsystem->Get_SelectionEntity();
    _PreviousEntity = DebuggerSubsystem->Get_PreviousSelectionEntity();

    // If selected actor and entity don't align but selected actor is an entity, use the selected actor's entity and set that as the selection entity
    // This is done since when the actor is selected, it is in COG code that can't set the selected entity when it happens
    [&]()
    {
        const auto* SelectedActor = GetSelection();

        if (ck::Is_NOT_Valid(SelectedActor))
        { return; }

        const auto& SelectedActorWorld = SelectedActor->GetWorld();
        const auto& SelectedWorld = DebuggerSubsystem->Get_SelectedWorld();
        auto SelectionEntityFromActor = UCk_Utils_OwningActor_UE::TryGet_ActorEntityHandle(GetSelection());

        if (ck::Is_NOT_Valid(SelectionEntityFromActor))
        { return; }

        if (SelectedActorWorld != SelectedWorld)
        {
            SelectionEntityFromActor = UCk_Utils_EntityReplicationDriver_UE::Get_ReplicatedHandleForWorld(SelectionEntityFromActor, SelectedActorWorld, SelectedWorld);
        }

        if (ck::Is_NOT_Valid(SelectionEntityFromActor))
        { return; }

        if (SelectionEntityFromActor != SelectionEntity)
        {
            DebuggerSubsystem->Set_SelectionEntity(SelectionEntityFromActor, SelectedWorld);
            SelectionEntity = SelectionEntityFromActor;
        }
    }();

    return SelectionEntity;
}

// --------------------------------------------------------------------------------------------------------------------