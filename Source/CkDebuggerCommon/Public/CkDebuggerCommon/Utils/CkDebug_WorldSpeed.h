#pragma once

#include "Engine/EngineBaseTypes.h"
#include "Internationalization/Text.h"
#include "UObject/WeakObjectPtr.h"

class UWorld;

// --------------------------------------------------------------------------------------------------------------------

struct CKDEBUGGERCOMMON_API FCkDebug_WorldSpeedTarget
{
    TWeakObjectPtr<UWorld> World;
    FText Reason;

    auto CanMutate() const -> bool { return World.IsValid(); }
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck::DebugWorldSpeed
{
    /** Pure selection rule used by the runtime resolver and its truth-table tests. */
    CKDEBUGGERCOMMON_API auto Choose_AuthorityIndex(
        const TArray<ENetMode>& InCandidateModes,
        FText& OutReason) -> int32;

    /** Resolves exactly one live PIE/Game authority world without retaining it. */
    CKDEBUGGERCOMMON_API auto Resolve_AuthorityWorld() -> FCkDebug_WorldSpeedTarget;

    CKDEBUGGERCOMMON_API auto Get_Multiplier() -> TOptional<float>;

    /** Mutates only the resolved authority world's replicated AWorldSettings. */
    CKDEBUGGERCOMMON_API auto Try_SetMultiplier(
        float InMultiplier,
        FText& OutFailureReason) -> bool;
}

// --------------------------------------------------------------------------------------------------------------------
