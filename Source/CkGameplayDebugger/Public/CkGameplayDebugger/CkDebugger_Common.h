#pragma once

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebugger_Common.generated.h"

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGAMEPLAYDEBUGGER_API FCk_GameplayDebugger_DebugActorList
{
    GENERATED_BODY()

public:
    FCk_GameplayDebugger_DebugActorList() = default;
    explicit FCk_GameplayDebugger_DebugActorList(
        TArray<AActor*> InActors);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Ck|GameplayDebugger", meta = (AllowPrivateAccess = true))
    TArray<TObjectPtr<AActor>> _DebugActors;

public:
    CK_PROPERTY_GET(_DebugActors);
};

// --------------------------------------------------------------------------------------------------------------------
