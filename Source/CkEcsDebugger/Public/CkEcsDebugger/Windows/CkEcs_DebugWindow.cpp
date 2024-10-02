#include "CkEcs_DebugWindow.h"

#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include "CkEcsDebugger/Subsystem/CkEcsDebugger_Subsystem.h"

#include <Net/Iris/ReplicationSystem/ActorReplicationBridge.h>
#include <Iris/ReplicationSystem/ReplicationSystem.h>

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_Ecs_DebugWindow::
    Get_SelectionEntity()
    -> FCk_Handle
{
    auto SelectionActor = GetSelection();

    if (ck::Is_NOT_Valid(SelectionActor))
    { return {}; }

    const auto World = SelectionActor->GetWorld();
    const auto SelectedWorld = World->GetSubsystem<UCk_EcsDebugger_Subsystem_UE>()->Get_SelectedWorld();

    [&]
    {
        if (ck::Is_NOT_Valid(World))
        { return; }

        const auto NetDriver = World->GetNetDriver();

        if (ck::Is_NOT_Valid(NetDriver))
        { return; }

        if (ck::Is_NOT_Valid(SelectedWorld))
        { return; }

        const auto SelectedNetDriver = SelectedWorld->GetNetDriver();

        if (ck::Is_NOT_Valid(SelectedNetDriver))
        { return; }

        const auto ReplicationSystem = NetDriver->GetReplicationSystem();

        if (ck::Is_NOT_Valid(ReplicationSystem))
        { return; }

        const auto SelectedReplicationSystem = SelectedNetDriver->GetReplicationSystem();

        if (ck::Is_NOT_Valid(SelectedReplicationSystem))
        { return; }

        const auto WorldRepBridge = Cast<UActorReplicationBridge>(ReplicationSystem->GetReplicationBridge());

        if (ck::Is_NOT_Valid(WorldRepBridge))
        { return; }

        const auto SelectedWorldRepBridge = Cast<UActorReplicationBridge>(SelectedReplicationSystem->GetReplicationBridge());

        if (ck::Is_NOT_Valid(SelectedWorldRepBridge))
        { return; }

        const auto RefHandle = WorldRepBridge->GetReplicatedRefHandle(SelectionActor);
        const auto SelectedWorldActor = SelectedWorldRepBridge->GetReplicatedObject(RefHandle);

        if (ck::Is_NOT_Valid(SelectedWorldActor))
        { return; }

        SelectionActor = Cast<AActor>(SelectedWorldActor);
    }();

    if (NOT UCk_Utils_OwningActor_UE::Get_IsActorEcsReady(SelectionActor))
    { return {}; }

    return UCk_Utils_OwningActor_UE::Get_ActorEntityHandle(SelectionActor);
}

// --------------------------------------------------------------------------------------------------------------------