#include "CkDebugger_Subsystem.h"

#include "CkCore/Actor/CkActor_Utils.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkGameplayDebugger/Bridge/CkDebugger_Bridge.h"
#include "CkGameplayDebugger/Settings/CkDebugger_Settings.h"

#include <Engine/World.h>

// --------------------------------------------------------------------------------------------------------------------

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
    ShouldCreateSubsystem(
        UObject* InOuter) const
    -> bool
{
    const auto& ShouldCreateSubsystem = Super::ShouldCreateSubsystem(InOuter);

    if (NOT ShouldCreateSubsystem)
    { return false; }

    if (ck::Is_NOT_Valid(InOuter))
    { return true; }

    const auto& World = InOuter->GetWorld();

    if (ck::Is_NOT_Valid(World))
    { return true; }

    return DoesSupportWorldType(World->WorldType);
}

auto
    UCk_GameplayDebugger_Subsystem_UE::
    DoesSupportWorldType(
        const EWorldType::Type WorldType) const
    -> bool
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

auto
    UCk_GameplayDebugger_Subsystem_UE::
    DoSpawnDebugBridge()
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
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

    const auto& debugProfileUserOverride = UCk_Utils_GameplayDebugger_UserSettings_UE::Get_DebugProfileUserOverride();

    if (ck::IsValid(debugProfileUserOverride))
    {
        _DebugBridgeActor->LoadNewDebugProfile(debugProfileUserOverride);
        return;
    }

    const auto& defaultDebugProfileToUse = UCk_Utils_GameplayDebugger_ProjectSettings_UE::Get_DefaultDebugProfileToUse();

    CK_ENSURE_IF_NOT(ck::IsValid(defaultDebugProfileToUse),
        TEXT("Invalid Gameplay Debugger Debug Profile set in the Project Settings! and NO profile override set in the Editor Preferences"))
    { return; }

    _DebugBridgeActor->LoadNewDebugProfile(defaultDebugProfileToUse);
#endif
}

// --------------------------------------------------------------------------------------------------------------------
