#pragma once

#include "CkCore/Macros/CkMacros.h"

#include <Subsystems/WorldSubsystem.h>

#include "CkDebugger_Subsystem.generated.h"

// --------------------------------------------------------------------------------------------------------------------

class ACk_GameplayDebugger_DebugBridge_UE;

// --------------------------------------------------------------------------------------------------------------------

UCLASS()
class CKGAMEPLAYDEBUGGER_API UCk_GameplayDebugger_Subsystem_UE : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_GameplayDebugger_Subsystem_UE);

protected:
    /** Called when world is ready to start gameplay before the game mode transitions to the correct state and call BeginPlay on all actors */
    virtual auto OnWorldBeginPlay(UWorld& InWorld) -> void override;

    virtual auto ShouldCreateSubsystem(UObject* InOuter) const -> bool override;

protected:
    // Called when determining whether to create this Subsystem
    virtual auto DoesSupportWorldType(const EWorldType::Type InWorldType) const -> bool override;

private:
    auto DoSpawnDebugBridge() -> void;

private:
    UPROPERTY(Transient)
    TObjectPtr<ACk_GameplayDebugger_DebugBridge_UE> _DebugBridgeActor;
};

// --------------------------------------------------------------------------------------------------------------------
