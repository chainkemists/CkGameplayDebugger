#include "CkDebug_SessionLifecycle.h"

// ====================================================================================================================

namespace
{
    ck::DebugSessionLifecycle::FCkDebug_OnSessionInvalidated GOnSessionInvalidated;
}

// ====================================================================================================================

namespace ck::DebugSessionLifecycle
{
    auto Get_OnSessionInvalidated() -> FCkDebug_OnSessionInvalidated&
    {
        return GOnSessionInvalidated;
    }
}

// ====================================================================================================================
