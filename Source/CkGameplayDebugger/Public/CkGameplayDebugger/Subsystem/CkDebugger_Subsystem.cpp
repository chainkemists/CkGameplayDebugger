#include "CkDebugger_Subsystem.h"

#include "CkCore/Actor/CkActor_Utils.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkGameplayDebugger/CkGameplayDebugger_Log.h"
#include "CkGameplayDebugger/Bridge/CkDebugger_Bridge.h"
#include "CkGameplayDebugger/Settings/CkDebugger_Settings.h"

#include <Engine/World.h>

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameplayDebugger_Subsystem_UE::
    Request_LoadNewDebugProfile(class UCk_GameplayDebugger_DebugProfile_PDA* InDebugProfile) const
    -> void
{
    if (ck::IsValid(_DebugBridgeActor))
    {
        _DebugBridgeActor->LoadNewDebugProfile(InDebugProfile);
    }
}

auto
    UCk_GameplayDebugger_Subsystem_UE::
    OnWorldBeginPlay(
        UWorld& InWorld)
    -> void
{
    Super::OnWorldBeginPlay(InWorld);

    DoSpawnDebugBridge();
}

auto
    UCk_GameplayDebugger_Subsystem_UE::
    DoSpawnDebugBridge()
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    const auto GetIsServer = [&]() -> bool
    {
        const auto* World = this->GetWorld();

        if (ck::Is_NOT_Valid(World))
        { return true; }

        return World->IsNetMode(NM_DedicatedServer) || World->IsNetMode(NM_ListenServer);
    };

    if (GetIsServer())
    { return; }

    _DebugBridgeActor = Cast<ACk_GameplayDebugger_DebugBridge_UE>
    (
        UCk_Utils_Actor_UE::Request_SpawnActor
        (
            FCk_Utils_Actor_SpawnActor_Params{GetWorld(), ACk_GameplayDebugger_DebugBridge_UE::StaticClass()}
            .Set_SpawnPolicy(ECk_Utils_Actor_SpawnActorPolicy::CannotSpawnInPersistentLevel)
        )
    );

    CK_ENSURE_IF_NOT(ck::IsValid(_DebugBridgeActor), TEXT("Failed to spawn Gameplay Debugger DebugBridge Actor!"))
    { return; }

    if (const auto& UserOverrideDebugProfile = UCk_Utils_GameplayDebugger_Settings_UE::Get_UserOverride_DebugProfile();
        ck::IsValid(UserOverrideDebugProfile))
    {
        _DebugBridgeActor->LoadNewDebugProfile(UserOverrideDebugProfile);
        return;
    }

    const auto& ProjectDefaultDebugProfile = UCk_Utils_GameplayDebugger_Settings_UE::Get_ProjectDefault_DebugProfile();

    CK_LOG_ERROR_NOTIFY_IF_NOT(ck::gameplay_debugger, ck::IsValid(ProjectDefaultDebugProfile),
        TEXT("Invalid Gameplay Debugger Debug Profile set in the Project Settings! and NO profile override set in the Editor Preferences"))
    { return; }

    _DebugBridgeActor->LoadNewDebugProfile(ProjectDefaultDebugProfile);
#endif
}

// --------------------------------------------------------------------------------------------------------------------
