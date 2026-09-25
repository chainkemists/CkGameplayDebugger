#include "CkProceduralAnimationDebugger/Data/CkProceduralAnimationDebugger_History.h"

// --------------------------------------------------------------------------------------------------------------------

FCkProceduralAnimationDebugger_History::
    FCkProceduralAnimationDebugger_History(
        int32 InCapacity)
{
    _Samples.SetNum(FMath::Clamp(InCapacity, 1, 4096));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_History::
    Push(
        const FCk_ProceduralAnimation_DebugSnapshot& InSample)
    -> bool
{
    if (NOT InSample.Get_Status().Get_Available() || NOT InSample.Get_Status().Get_HasAcceptedSample()
        || NOT FMath::IsFinite(InSample.Get_Sample().Get_Time().Get_Seconds()))
    { return false; }

    if (const auto* Latest = Get_Sample(_Count - 1))
    {
        if (Latest->Get_EntityId() != InSample.Get_EntityId()
            || InSample.Get_Sample().Get_Sequence() <= Latest->Get_Sample().Get_Sequence()
            || InSample.Get_Sample().Get_Time() < Latest->Get_Sample().Get_Time())
        { return false; }
    }

    if (_Count < _Samples.Num())
    {
        _Samples[(_Head + _Count) % _Samples.Num()] = InSample;
        ++_Count;
    }
    else
    {
        _Samples[_Head] = InSample;
        _Head = (_Head + 1) % _Samples.Num();
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_History::
    Hold()
    -> void
{
    if (const auto* Sample = Get_Displayed())
    { _Held = *Sample; }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_History::
    GoLive()
    -> void
{
    _Held.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_History::
    Scrub(
        int32 InChronologicalIndex)
    -> bool
{
    const auto* Sample = Get_Sample(InChronologicalIndex);
    if (Sample == nullptr)
    { return false; }

    _Held = *Sample;
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_History::
    Scrub_BySequence(
        uint64 InSequence)
    -> bool
{
    for (auto Index = 0; Index < _Count; ++Index)
    {
        if (Get_Sample(Index)->Get_Sample().Get_Sequence() == InSequence)
        { return Scrub(Index); }
    }
    return false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_History::
    Reset()
    -> void
{
    for (auto& Sample : _Samples)
    { Sample = {}; }

    _Held.Reset();
    _Head = 0;
    _Count = 0;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_History::
    Get_Sample(
        int32 InChronologicalIndex) const
    -> const FCk_ProceduralAnimation_DebugSnapshot*
{
    return InChronologicalIndex >= 0 && InChronologicalIndex < _Count
        ? &_Samples[(_Head + InChronologicalIndex) % _Samples.Num()]
        : nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_History::
    Get_Displayed() const
    -> const FCk_ProceduralAnimation_DebugSnapshot*
{
    return _Held.IsSet() ? &_Held.GetValue() : Get_Sample(_Count - 1);
}

// --------------------------------------------------------------------------------------------------------------------
