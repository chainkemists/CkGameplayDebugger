#include "CkDebug_SessionLifecycle.h"

// ====================================================================================================================

namespace
{
    ck::DebugSessionLifecycle::FCkDebug_OnSessionInvalidated GOnSessionInvalidated;
    ck::DebugSessionLifecycle::FCkDebug_OnWorldInvalidated GOnWorldInvalidated;
}

// ====================================================================================================================

namespace ck::DebugSessionLifecycle
{
    auto Get_OnSessionInvalidated() -> FCkDebug_OnSessionInvalidated&
    {
        return GOnSessionInvalidated;
    }

    auto Get_OnWorldInvalidated() -> FCkDebug_OnWorldInvalidated&
    {
        return GOnWorldInvalidated;
    }
}

// ====================================================================================================================
