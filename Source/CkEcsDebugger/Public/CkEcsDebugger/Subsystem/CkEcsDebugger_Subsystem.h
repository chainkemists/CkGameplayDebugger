#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Subsystems/GameWorldSubsytem/CkGameWorldSubsystem.h"

#include "CkEcsDebugger_Subsystem.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(DisplayName = "CkSubsystem_EcsDebugger")
class CKECSDEBUGGER_API UCk_EcsDebugger_Subsystem_UE : public UCk_Game_WorldSubsystem_Base_UE
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_EcsDebugger_Subsystem_UE);

public:
    auto Initialize(FSubsystemCollectionBase& Collection) -> void override;
    auto Deinitialize() -> void override;

protected:
    /** Called when world is ready to start gameplay before the game mode transitions to the correct state and call BeginPlay on all actors */
    auto OnWorldBeginPlay(UWorld& InWorld) -> void override;

public:
    UPROPERTY(Transient)
    UWorld* _SelectedWorld;

private:
    UPROPERTY(Transient)
    TObjectPtr<class ACk_Ecs_DebugWindowManager_UE> _DebugWindowManager;
};

// --------------------------------------------------------------------------------------------------------------------
