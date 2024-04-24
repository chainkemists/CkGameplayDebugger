#pragma once

#include "CogCommon.h"
#include "CogWindowManager.h"

#include <GameFramework/Info.h>

#include "CkEcs_DebugWindowManager.generated.h"

// --------------------------------------------------------------------------------------------------------------------
// TODO: Turn into AInfo and spawn via subsystem

UCLASS(NotBlueprintType, NotBlueprintable, NotPlaceable)
class CKECSDEBUGGER_API ACk_Ecs_DebugWindowManager_UE : public AInfo
{
    GENERATED_BODY()

public:
    ACk_Ecs_DebugWindowManager_UE();

protected:
    auto
    BeginPlay() -> void override;

public:
    auto
    Tick(
        float DeltaSeconds) -> void override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UObject> _CogWindowManagerRef;

#if ENABLE_COG
    TObjectPtr<UCogWindowManager> _CogWindowManager;
#endif
};

// --------------------------------------------------------------------------------------------------------------------
