#include "CkEcsDebugger_Subsystem.h"

#include "CkCore/Actor/CkActor_Utils.h"
#include "CkCore/Game/CkGame_Utils.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Utils.h"

#include "CkEcsDebugger/Windows/CkEcs_DebugWindowManager.h"

#if UE_WITH_IRIS
#include <Iris/ReplicationSystem/ReplicationSystem.h>
#include <Net/Iris/ReplicationSystem/EngineReplicationBridge.h>
#endif

#include <Engine/PackageMapClient.h>
#include <Engine/NetDriver.h>

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_EcsDebugger_Subsystem_UE::
    Initialize(
        FSubsystemCollectionBase& InCollection)
    -> void
{
    Super::Initialize(InCollection);
}

auto
    UCk_EcsDebugger_Subsystem_UE::
    Deinitialize()
    -> void
{
    if (ck::IsValid(_DebugWindowManager))
    {
        _DebugWindowManager = nullptr;
    }

    Super::Deinitialize();
}

auto
    UCk_EcsDebugger_Subsystem_UE::
    OnWorldBeginPlay(
        UWorld& InWorld)
    -> void
{
    Super::OnWorldBeginPlay(InWorld);

    for (auto& Context : GEngine->GetWorldContexts())
    {
        const auto& ContextWorld = Context.World();

        if (ck::Is_NOT_Valid(ContextWorld))
        { continue; }

        auto GameInstance = ContextWorld->GetGameInstance();

        if (ck::Is_NOT_Valid(GameInstance))
        { continue; }

        if (ContextWorld->GetNetMode() == NM_DedicatedServer ||
            NOT UCk_Utils_Game_UE::Get_IsPIE_UnderOneProcess(this))
        {
            _SelectedWorld = ContextWorld;
        }
    }

    _DebugWindowManager = Cast<ACk_Ecs_DebugWindowManager_UE>
    (
        UCk_Utils_Actor_UE::Request_SpawnActor
        (
            FCk_Utils_Actor_SpawnActor_Params{&InWorld, ACk_Ecs_DebugWindowManager_UE::StaticClass()}
            .Set_Label(TEXT("ACk_Ecs_DebugWindowManager"))
            .Set_SpawnPolicy(ECk_Utils_Actor_SpawnActorPolicy::CannotSpawnInPersistentLevel)
        )
    );

    CK_ENSURE_IF_NOT(ck::IsValid(_DebugWindowManager),
        TEXT("Failed to spawn EcsDebugger WindowManager. EcsDebugger will NOT work!"))
    { return; }
}

auto
    UCk_EcsDebugger_Subsystem_UE::
    Set_SelectedWorld(
        UWorld* InSelectedWorld)
    -> void
{
    if (_SelectedWorld == InSelectedWorld)
    { return; }

    const auto PreviousWorld = _SelectedWorld;

    _SelectedWorld = InSelectedWorld;

    if (ck::IsValid(_SelectionEntity))
    {
        Set_SelectionEntity(_SelectionEntity, PreviousWorld);
    }
}

auto
    UCk_EcsDebugger_Subsystem_UE::
    Set_SelectionEntity(
        const FCk_Handle& InSelectionEntity,
        UWorld* InSelectionEntityOwningWorld)
    -> void
{
    _SelectionEntity = UCk_Utils_EntityReplicationDriver_UE::Get_ReplicatedHandleForWorld(InSelectionEntity, InSelectionEntityOwningWorld, _SelectedWorld);
}

auto
    UCk_EcsDebugger_Subsystem_UE::
    Get_ActorOnSelectedWorld(AActor* InActor)
    -> AActor*
{
    if (ck::Is_NOT_Valid(InActor))
    { return {}; }

    const auto ActorWorld = InActor->GetWorld();

    if (ck::Is_NOT_Valid(ActorWorld))
    { return {}; }

    if (ck::Is_NOT_Valid(Get_SelectedWorld()))
    { return {}; }

    if (ActorWorld == Get_SelectedWorld())
    { return InActor; }

    const auto NetDriver = ActorWorld->GetNetDriver();
    if (ck::Is_NOT_Valid(NetDriver))
    { return {}; }

    if (NetDriver->IsUsingIrisReplication())
    {
#if UE_WITH_IRIS
        const auto SelectedNetDriver = Get_SelectedWorld()->GetNetDriver();
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

        const auto RefHandle = WorldRepBridge->GetReplicatedRefHandle(InActor);
        const auto SelectedWorldActor = SelectedWorldRepBridge->GetReplicatedObject(RefHandle);

        if (ck::Is_NOT_Valid(SelectedWorldActor))
        { return {}; }

        return Cast<AActor>(SelectedWorldActor);
#else
        CK_ENSURE_IF_NOT(false, TEXT("Net Driver has Iris enabled without UE_WITH_IRIS enabled, this should NOT be possible!"))
        { return {}; }
#endif
    }
    else
    {
        if (ck::Is_NOT_Valid(ActorWorld))
        { return {}; }

        const auto& WorldNetDriver = ActorWorld->GetNetDriver();
        if (ck::Is_NOT_Valid(WorldNetDriver))
        { return {}; }

        const auto& WorldGuidCache = WorldNetDriver->GuidCache;
        if (ck::Is_NOT_Valid(WorldGuidCache))
        { return {}; }

        const auto WorldNetGuid = WorldGuidCache->GetNetGUID(InActor);
        if (ck::Is_NOT_Valid(WorldNetGuid))
        { return {}; }

        const auto SelectedWorldNetDriver = Get_SelectedWorld()->GetNetDriver();
        if (ck::Is_NOT_Valid(SelectedWorldNetDriver))
        { return {}; }

        const auto& SelectedWorldGuidCache = SelectedWorldNetDriver->GuidCache;
        if (ck::Is_NOT_Valid(SelectedWorldGuidCache))
        { return {}; }

        constexpr auto IgnoreMustBeMapped = true;
        return Cast<AActor>(SelectedWorldGuidCache->GetObjectFromNetGUID(WorldNetGuid, IgnoreMustBeMapped));
    }
}

// --------------------------------------------------------------------------------------------------------------------
