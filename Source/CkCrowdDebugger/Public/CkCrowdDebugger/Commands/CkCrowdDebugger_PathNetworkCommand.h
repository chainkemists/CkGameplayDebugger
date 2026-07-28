#pragma once

#include "CkCrowd/Agent/CkCrowdAgent_Fragment_Data.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::crowd_debugger
{
    // Projects the raw viewport destination before taking control, then issues one manual move.
    // Invalid/off-navmesh clicks leave override, follower ownership, and the active move unchanged.
    CKCROWDDEBUGGER_API auto
    Try_IssueManualMove(
        FCk_Handle_CrowdAgent& InAgent,
        const FVector& InRawDestination,
        FVector& OutProjectedDestination)
        -> bool;

    // Adds debugger-owned path-network following only when the current world has one built network.
    // An existing follower belongs to its current owner and is never replaced.
    CKCROWDDEBUGGER_API auto
    Try_EnsurePathNetworkFollower(
        FCk_Handle_CrowdAgent& InAgent)
        -> bool;

    // Stops the debugger-issued move, then removes a follower only if this debugger created it.
    // A follower owned by gameplay or another tool remains composed.
    CKCROWDDEBUGGER_API auto
    ReleaseManualMove(
        FCk_Handle_CrowdAgent& InAgent)
        -> void;
}
