#include "CkEcsDebugger_Subsystem.h"

#include "CkCore/Actor/CkActor_Utils.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcsDebugger/Windows/CkEcs_DebugWindowManager.h"

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

    //if (InWorld.IsServer())
    //{ return; }

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

// --------------------------------------------------------------------------------------------------------------------
