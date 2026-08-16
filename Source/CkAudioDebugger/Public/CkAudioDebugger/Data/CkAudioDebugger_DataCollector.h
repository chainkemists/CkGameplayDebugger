#pragma once

#include "CkAudioDebugger/Data/CkAudioDebugger_Types.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// --------------------------------------------------------------------------------------------------------------------

/** Rebuilds a read-only snapshot of every AudioDirector in a world each refresh. Never mutates ECS state, never
 *  touches a `UAudioComponent`, and never calls a `Request_*`.
 *
 *  That last one is not incidental: CkAudio's first anti-pattern is driving `UAudioComponent` directly instead of
 *  routing through track entities, and a debugger that reads the component to describe it would be the defect it
 *  exists to surface. Everything here comes off fragments the processors already maintain. */
class FCkAudioDebugger_DataCollector
{
public:
    auto Collect(UWorld* InWorld) -> void;
    auto Reset() -> void;

    auto Get_Snapshot() const -> const FCkAudioDebugger_Snapshot&;

private:
    FCkAudioDebugger_Snapshot _Snapshot;
};

// --------------------------------------------------------------------------------------------------------------------
