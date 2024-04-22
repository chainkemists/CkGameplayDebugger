#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Subsystems/GameWorldSubsytem/CkGameWorldSubsystem.h"

#include "CkDebugger_Subsystem.generated.h"

// --------------------------------------------------------------------------------------------------------------------

class ACk_GameplayDebugger_DebugBridge_UE;

// --------------------------------------------------------------------------------------------------------------------

UCLASS(DisplayName = "CkSubsystem_GameplayDebugger")
class CKGAMEPLAYDEBUGGER_API UCk_GameplayDebugger_Subsystem_UE : public UCk_Game_WorldSubsystem_Base_UE
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_GameplayDebugger_Subsystem_UE);

public:
    UFUNCTION(BlueprintCallable, Category = "Ck|GameplayDebugger|Subsytem")
    void Request_LoadNewDebugProfile(
        class UCk_GameplayDebugger_DebugProfile_PDA* InDebugProfile) const;

protected:
    /** Called when world is ready to start gameplay before the game mode transitions to the correct state and call BeginPlay on all actors */
    virtual auto OnWorldBeginPlay(UWorld& InWorld) -> void override;

private:
    auto DoSpawnDebugBridge() -> void;

private:
    UPROPERTY(Transient)
    TObjectPtr<ACk_GameplayDebugger_DebugBridge_UE> _DebugBridgeActor;
};

// --------------------------------------------------------------------------------------------------------------------
