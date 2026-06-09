#include "CkEntityDebugOverlay/History/CkDebugOverlay_History.h"

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_History::Observe(const FCk_DebugOverlay_HistoryKey& Key, const FString& Value, double Now) -> void
{
    FRecord& R = Records.FindOrAdd(Key);
    if (R.Ring.Num() > 0 && R.Ring.Last() == Value) { return; }
    R.Ring.Add(Value);
    while (R.Ring.Num() > MaxDepth) { R.Ring.RemoveAt(0); }
    R.LastChanged = Now;
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_History::Get_Trail(const FCk_DebugOverlay_HistoryKey& Key, int32 Depth) const -> TArray<FString>
{
    TArray<FString> Out;
    if (const FRecord* R = Records.Find(Key))
    {
        const int32 PriorCount = R->Ring.Num() - 1;          // exclude current (tail)
        const int32 Start = FMath::Max(0, PriorCount - Depth);
        for (int32 i = Start; i < PriorCount; ++i) { Out.Add(R->Ring[i]); }
    }
    return Out;
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_History::Get_LastChangedTime(const FCk_DebugOverlay_HistoryKey& Key) const -> double
{
    const FRecord* R = Records.Find(Key);
    return R ? R->LastChanged : 0.0;
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_History::Forget(uint32 EntityId) -> void
{
    for (auto It = Records.CreateIterator(); It; ++It)
    {
        if (It.Key().EntityId == EntityId) { It.RemoveCurrent(); }
    }
}

// --------------------------------------------------------------------------------------------------------------------
