#include "CkEcsDebugger_Subsystem.h"

#include "CkCore/Actor/CkActor_Utils.h"
#include "CkCore/Game/CkGame_Utils.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/Net/EntityReplicationDriver/CkEntityReplicationDriver_Utils.h"

#include "CkEcsDebugger/Windows/Ability/CkAbilities_DebugWindow.h"
#include "CkEcsDebugger/Windows/Ability/CkAbilityOwnerTags_DebugWindow.h"
#include "CkEcsDebugger/Windows/Attribute/CkAttribute_DebugWindow.h"
#include "CkEcsDebugger/Windows/Entity/CkEntity_DebugWindow.h"
#include "CkEcsDebugger/Windows/EntitySelection/CkEntitySelection_DebugWindow.h"
#include "CkEcsDebugger/Windows/EntityCollection/CkEntityCollection_DebugWindow.h"
#include "CkEcsDebugger/Windows/OverlapBody/CkOverlapBody_DebugWindow.h"
#include "CkEcsDebugger/Windows/Timer/CkTimer_DebugWindow.h"
#include "CkEcsDebugger/Windows/World/CkWorld_DebugWindow.h"

#if UE_WITH_IRIS
#include <Iris/ReplicationSystem/ReplicationSystem.h>
#include <Net/Iris/ReplicationSystem/EngineReplicationBridge.h>
#endif

#include "CogEngineWindow_CollisionTester.h"
#include "CogEngineWindow_CollisionViewer.h"
#include "CogEngineWindow_CommandBindings.h"
#include "CogEngineWindow_DebugSettings.h"
#include "CogEngineWindow_ImGui.h"
#include "CogEngineWindow_Selection.h"
#include "CogSubsystem.h"

#include "CkEcsDebugger/Windows/AnimState/CkAnimPlan_DebugWindow.h"
#include "CkEcsDebugger/Windows/Probe/CkProbes_DebugWindow.h"

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

#if ENABLE_COG
    _CogSubsystem = InCollection.InitializeDependency<UCogSubsystem>();
#endif
}

auto
    UCk_EcsDebugger_Subsystem_UE::
    Deinitialize()
    -> void
{
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
            ContextWorld->GetNetMode() == NM_ListenServer ||
            NOT UCk_Utils_Game_UE::Get_IsPIE_UnderOneProcess(this))
        {
            _SelectedWorld = ContextWorld;
        }
    }
    if (ck::Is_NOT_Valid(_SelectedWorld))
    { _SelectedWorld = GetWorld(); }

#if ENABLE_COG
    // Add a custom window
    _CogSubsystem->AddWindow<FCk_World_DebugWindow>("Ck.World");
    _CogSubsystem->AddWindow<FCk_EntityBasics_DebugWindow>("Ck.Entity");
    _CogSubsystem->AddWindow<FCk_EntitySelection_DebugWindow>("Ck.EntitySelection");
    _CogSubsystem->AddWindow<FCk_EntityCollection_DebugWindow>("Ck.EntityCollection");
    _CogSubsystem->AddWindow<FCk_Timer_DebugWindow>("Ck.Timer");

    _CogSubsystem->AddWindow<FCk_AnimPlan_DebugWindow>("Ck.AnimPlan");

    _CogSubsystem->AddWindow<FCk_ByteAttribute_DebugWindow>("Ck.Attribute.Byte");
    _CogSubsystem->AddWindow<FCk_FloatAttribute_DebugWindow>("Ck.Attribute.Float");
    _CogSubsystem->AddWindow<FCk_VectorAttribute_DebugWindow>("Ck.Attribute.Vector");

    _CogSubsystem->AddWindow<FCk_AbilityOwnerTags_DebugWindow>("Ck.AbilityOwnerTags");
    _CogSubsystem->AddWindow<FCk_Abilities_DebugWindow>("Ck.Abilities");

    _CogSubsystem->AddWindow<FCk_Marker_DebugWindow>("Ck.OverlapBody.Marker");
    _CogSubsystem->AddWindow<FCk_Sensor_DebugWindow>("Ck.OverlapBody.Sensor");
    _CogSubsystem->AddWindow<FCk_Probes_DebugWindow>("Ck.Probes");

    _CogSubsystem->AddWindow<FCogEngineWindow_CollisionTester>("Engine.Collision Tester");
    _CogSubsystem->AddWindow<FCogEngineWindow_CollisionViewer>("Engine.Collision Viewer");
    _CogSubsystem->AddWindow<FCogEngineWindow_CommandBindings>("Engine.Command Bindings");
    _CogSubsystem->AddWindow<FCogEngineWindow_DebugSettings>("Engine.Debug Settings");
    _CogSubsystem->AddWindow<FCogEngineWindow_ImGui>("Engine.ImGui");
    _CogSubsystem->AddWindow<FCogEngineWindow_Selection>("Engine.Selection");
#endif
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
    _PreviousSelectionEntity = _SelectionEntity;
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
