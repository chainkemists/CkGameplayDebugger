#pragma once

#include "CoreMinimal.h"

// ====================================================================================================================
// Process-wide debugger session boundary.
//
// Ck debugger tabs outlive PIE worlds, while FCk_Handle must not outlive the
// registry that backs it. Common owns only this notification; each debugger
// remains responsible for synchronously dropping its own handle-bearing state.
// The process-wide notification deliberately carries no UWorld or FCk_Handle.
// Runtime teardown is world-scoped so one live game world is never invalidated
// merely because another world in the same process is ending.
// ====================================================================================================================

namespace ck::DebugSessionLifecycle
{
    DECLARE_MULTICAST_DELEGATE(FCkDebug_OnSessionInvalidated);
    DECLARE_MULTICAST_DELEGATE_OneParam(FCkDebug_OnWorldInvalidated, UWorld*);

    CKDEBUGGERCOMMON_API auto Get_OnSessionInvalidated() -> FCkDebug_OnSessionInvalidated&;
    CKDEBUGGERCOMMON_API auto Get_OnWorldInvalidated() -> FCkDebug_OnWorldInvalidated&;
}

// ====================================================================================================================
