#include "CkEcs_DebugWindow.h"

#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include "CkEcsDebugger/Subsystem/CkEcsDebugger_Subsystem.h"

#if UE_WITH_IRIS
#include <Iris/ReplicationSystem/ReplicationSystem.h>
#include <Iris/ReplicationSystem/ReplicationBridge.h>
#endif

#include "Net/Iris/ReplicationSystem/EngineReplicationBridge.h"

#include <Engine/PackageMapClient.h>
#include <GameFramework/Actor.h>
#include <Engine/World.h>

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

    if (ck::Is_NOT_Valid(World))
    { return {}; }

    if (ck::Is_NOT_Valid(SelectedWorld))
    { return {}; }

    const auto NetDriver = World->GetNetDriver();
    if (ck::Is_NOT_Valid(NetDriver))
    { return {}; }

#if UE_WITH_IRIS
    if (NetDriver->IsUsingIrisReplication())
    {
        const auto SelectedNetDriver = SelectedWorld->GetNetDriver();
        if (ck::Is_NOT_Valid(SelectedNetDriver))
        { return {}; }

        const auto ReplicationSystem = NetDriver->GetReplicationSystem();
        if (ck::Is_NOT_Valid(ReplicationSystem))
        { return {}; }

        const auto SelectedReplicationSystem = SelectedNetDriver->GetReplicationSystem();
        if (ck::Is_NOT_Valid(SelectedReplicationSystem))
        { return {}; }

        const auto WorldRepBridge = ReplicationSystem->GetReplicationBridgeAs<UEngineReplicationBridge>();
        if (ck::Is_NOT_Valid(WorldRepBridge))
        { return {}; }

        const auto SelectedWorldRepBridge = SelectedReplicationSystem->GetReplicationBridgeAs<UEngineReplicationBridge>();
        if (ck::Is_NOT_Valid(SelectedWorldRepBridge))
        { return {}; }

        const auto RefHandle = WorldRepBridge->GetReplicatedRefHandle(SelectionActor);
        const auto SelectedWorldActor = SelectedWorldRepBridge->GetReplicatedObject(RefHandle);

        if (ck::Is_NOT_Valid(SelectedWorldActor))
        { return {}; }

        SelectionActor = Cast<AActor>(SelectedWorldActor);
    }
#endif
    if (NOT NetDriver->IsUsingIrisReplication())
    {
        if (ck::Is_NOT_Valid(World))
        { return {}; }

        const auto& WorldNetDriver = World->GetNetDriver();
        if (ck::Is_NOT_Valid(WorldNetDriver))
        { return {}; }

        const auto& WorldGuidCache = WorldNetDriver->GuidCache;
        if (ck::Is_NOT_Valid(WorldGuidCache))
        { return {}; }

        const auto WorldNetGuid = WorldGuidCache->GetNetGUID(SelectionActor);
        if (ck::Is_NOT_Valid(WorldNetGuid))
        { return {}; }

        const auto SelectedWorldNetDriver = SelectedWorld->GetNetDriver();
        if (ck::Is_NOT_Valid(SelectedWorldNetDriver))
        { return {}; }

        const auto& SelectedWorldGuidCache = SelectedWorldNetDriver->GuidCache;
        if (ck::Is_NOT_Valid(SelectedWorldGuidCache))
        { return {}; }

        constexpr auto IgnoreMustBeMapped = true;
        const auto SelectedActor = Cast<AActor>(SelectedWorldGuidCache->GetObjectFromNetGUID(WorldNetGuid, IgnoreMustBeMapped));
        if (ck::Is_NOT_Valid(SelectedActor))
        { return {}; }

        SelectionActor = SelectedActor;
    }

    if (NOT UCk_Utils_OwningActor_UE::Get_IsActorEcsReady(SelectionActor))
    { return {}; }

    return UCk_Utils_OwningActor_UE::Get_ActorEntityHandle(SelectionActor);
}

// --------------------------------------------------------------------------------------------------------------------