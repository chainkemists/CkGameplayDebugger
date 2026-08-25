#pragma once

#include "CoreMinimal.h"

class UWorld;

// --------------------------------------------------------------------------------------------------------------------
// Acknowledgment marker for a viewport-issued world command.
//
// A debugger that commands the game from its 3D viewport (Crowd's right-click move-to) needs the
// acknowledgment to read in the GAME world, not on the Slate surface: the user is looking at level
// geometry and the command lands on it. This paints a transient PMG ring that self-destroys, so no
// caller has to own or tick it.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::debug_3d
{
CKDEBUGGERCOMMON_API auto
Draw_WorldCommandPing(UWorld* InWorld, const FVector& InLocation) -> void;
} // namespace ck::debug_3d
