#pragma once

#include "CkIntentDebugger/Data/CkIntentDebugger_Types.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// --------------------------------------------------------------------------------------------------------------------
// The one place in this module that touches CkIntent / CkInput state.
//
// Sources are discovered through the public `UCk_InputSource_Subsystem::Get_InputSource()` per local player — the
// subsystem owns the one source per player, so nothing here has to guess. Layers are discovered through a read-only
// registry view over `ck::FFragment_InputLayer_Params`, filtered to the source and sorted by priority descending;
// there is no stack-enumeration Util and a debugger cannot guess an int32 priority space.
//
// Everything read is a value the runtime already computed and stored. This collector performs no matching, derives
// no octant, cleans no axis and evaluates no deferral — see the module CLAUDE.md for why that boundary is the point.
// --------------------------------------------------------------------------------------------------------------------

class CKINTENTDEBUGGER_API FCkIntentDebugger_DataCollector
{
public:
    static auto
    Collect(
        UWorld* InWorld,
        int32 InMaxRecordedFrames) -> FCkIntentDebugger_Snapshot;
};

// --------------------------------------------------------------------------------------------------------------------
