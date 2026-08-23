#pragma once

#include "CkTextureDebugger/Data/CkTextureDebugger_Types.h"

class UWorld;

// --------------------------------------------------------------------------------------------------------------------

namespace ck::texture_debugger::collector
{
    /** Walks already-loaded actors/components only. It never loads a World Partition cell or an asset. */
    CKTEXTUREDEBUGGER_API auto Collect_LoadedWorld(
        UWorld* InWorld) -> FCkTextureDebugger_LoadedWorldSnapshot;
}
