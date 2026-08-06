#pragma once

#include "CoreMinimal.h"

// ====================================================================================================================
// Process-wide debugger session boundary.
//
// Ck debugger tabs outlive PIE worlds, while FCk_Handle must not outlive the
// registry that backs it. Common owns only this notification; each debugger
// remains responsible for synchronously dropping its own handle-bearing state.
// The notification deliberately carries no UWorld or FCk_Handle.
// ====================================================================================================================

namespace ck::DebugSessionLifecycle
{
    DECLARE_MULTICAST_DELEGATE(FCkDebug_OnSessionInvalidated);

    CKDEBUGGERCOMMON_API auto Get_OnSessionInvalidated() -> FCkDebug_OnSessionInvalidated&;
}

// ====================================================================================================================
