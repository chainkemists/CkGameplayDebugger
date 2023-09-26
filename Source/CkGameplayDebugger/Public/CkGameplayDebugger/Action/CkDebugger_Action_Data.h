#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkGameplayDebugger/CkDebugger_Common.h"

#include "CkGameplayDebugger/Engine/CkDebugger_GameplayDebuggerTypes.h"

#include "CkDebugger_Action_Data.generated.h"

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGAMEPLAYDEBUGGER_API FCk_GameplayDebugger_PerformDebugAction_Params
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_GameplayDebugger_PerformDebugAction_Params);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_GameplayDebugger_DebugActorList _DebugActorList;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    int32 _CurrentlySelectedDebugActorIndex;

private:
    FGameplayDebuggerCanvasContext* _CanvasContext = nullptr;

public:
    CK_PROPERTY_GET(_DebugActorList);
    CK_PROPERTY_GET(_CurrentlySelectedDebugActorIndex);
    CK_PROPERTY_GET(_CanvasContext);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_GameplayDebugger_PerformDebugAction_Params, _DebugActorList, _CurrentlySelectedDebugActorIndex, _CanvasContext);
};

// --------------------------------------------------------------------------------------------------------------------
