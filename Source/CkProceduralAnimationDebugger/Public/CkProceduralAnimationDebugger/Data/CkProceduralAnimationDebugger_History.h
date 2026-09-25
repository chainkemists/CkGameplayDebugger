#pragma once

#include "CkProceduralAnimation/Debug/CkProceduralAnimation_Debug.h"

// Samples are copies, not replay instructions. Holding/scrubbing never pauses gameplay.
// The held copy survives eviction from the bounded recording ring.
class CKPROCEDURALANIMATIONDEBUGGER_API FCkProceduralAnimationDebugger_History
{
public:
    explicit FCkProceduralAnimationDebugger_History(int32 InCapacity = 600);

    auto Push(const FCk_ProceduralAnimation_DebugSnapshot& InSample) -> bool;
    auto Hold() -> void;
    auto GoLive() -> void;
    auto Scrub(int32 InChronologicalIndex) -> bool;
    auto Reset() -> void;
    auto Get_Count() const -> int32 { return _Count; }
    auto Get_Capacity() const -> int32 { return _Samples.Num(); }
    auto Get_IsLive() const -> bool { return NOT _Held.IsSet(); }
    auto Get_Sample(int32 InChronologicalIndex) const -> const FCk_ProceduralAnimation_DebugSnapshot*;
    auto Get_Displayed() const -> const FCk_ProceduralAnimation_DebugSnapshot*;

private:
    TArray<FCk_ProceduralAnimation_DebugSnapshot> _Samples;
    TOptional<FCk_ProceduralAnimation_DebugSnapshot> _Held;
    int32 _Head = 0;
    int32 _Count = 0;
};
