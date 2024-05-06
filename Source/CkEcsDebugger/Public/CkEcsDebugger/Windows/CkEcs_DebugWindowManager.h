#pragma once

#include "CogCommon.h"
#include "CogWindowManager.h"

#include "CkCore/Macros/CkMacros.h"

#include <GameFramework/Info.h>

#include "CkEcs_DebugWindowManager.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(NotBlueprintType, NotBlueprintable, NotPlaceable)
class CKECSDEBUGGER_API ACk_Ecs_DebugWindowManager_UE : public AInfo
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(ACk_Ecs_DebugWindowManager_UE);

public:
    ACk_Ecs_DebugWindowManager_UE();

protected:
    auto
    BeginPlay() -> void override;

public:
    auto
    Tick(
        float DeltaSeconds) -> void override;


public:
    UPROPERTY(Transient)
    TObjectPtr<AActor> _SelectedActor;

private:
    UPROPERTY(Transient)
    TObjectPtr<UObject> _CogWindowManagerRef;

#if ENABLE_COG
    TObjectPtr<UCogWindowManager> _CogWindowManager;
#endif
};

// --------------------------------------------------------------------------------------------------------------------
