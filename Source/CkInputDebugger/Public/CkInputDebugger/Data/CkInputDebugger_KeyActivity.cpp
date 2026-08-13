#include "CkInputDebugger/Data/CkInputDebugger_KeyActivity.h"

#include "Framework/Application/SlateApplication.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_input_debugger_key_activity
{
    constexpr auto MaxRecentKeys = 6;
    constexpr auto AnalogRestThreshold = 0.08f;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInputDebugger_KeyActivityObserver::
    HandleKeyDownEvent(
        FSlateApplication& InSlateApp,
        const FKeyEvent& InKeyEvent)
    -> bool
{
    if (NOT InKeyEvent.IsRepeat())
    { DoPress(InKeyEvent.GetKey()); }

    return false;
}

auto
    FCkInputDebugger_KeyActivityObserver::
    HandleKeyUpEvent(
        FSlateApplication& InSlateApp,
        const FKeyEvent& InKeyEvent)
    -> bool
{
    DoRelease(InKeyEvent.GetKey());
    return false;
}

auto
    FCkInputDebugger_KeyActivityObserver::
    HandleMouseButtonDownEvent(
        FSlateApplication& InSlateApp,
        const FPointerEvent& InPointerEvent)
    -> bool
{
    DoPress(InPointerEvent.GetEffectingButton());
    return false;
}

auto
    FCkInputDebugger_KeyActivityObserver::
    HandleMouseButtonUpEvent(
        FSlateApplication& InSlateApp,
        const FPointerEvent& InPointerEvent)
    -> bool
{
    DoRelease(InPointerEvent.GetEffectingButton());
    return false;
}

auto
    FCkInputDebugger_KeyActivityObserver::
    HandleAnalogInputEvent(
        FSlateApplication& InSlateApp,
        const FAnalogInputEvent& InAnalogEvent)
    -> bool
{
    using namespace ck_input_debugger_key_activity;

    const auto Magnitude = FMath::Abs(InAnalogEvent.GetAnalogValue());
    _AnalogMagnitudes.FindOrAdd(InAnalogEvent.GetKey()) = Magnitude;

    // Analog keys ride the held set past the rest threshold so sticks light the visuals
    const auto IsHeld = Get_IsHeld(InAnalogEvent.GetKey());

    if (Magnitude >= AnalogRestThreshold && NOT IsHeld)
    { DoPress(InAnalogEvent.GetKey()); }
    else if (Magnitude < AnalogRestThreshold && IsHeld)
    { DoRelease(InAnalogEvent.GetKey()); }

    return false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInputDebugger_KeyActivityObserver::
    Get_IsHeld(
        const FKey& InKey) const
    -> bool
{
    return _Held.ContainsByPredicate([&](const FCkInputDebugger_HeldKey& InHeld) { return InHeld.Key == InKey; });
}

auto
    FCkInputDebugger_KeyActivityObserver::
    Get_AnalogMagnitude(
        const FKey& InKey) const
    -> float
{
    if (const auto* Found = _AnalogMagnitudes.Find(InKey))
    { return *Found; }

    return 0.0f;
}

auto
    FCkInputDebugger_KeyActivityObserver::
    Clear()
    -> void
{
    _Held.Reset();
    _Recent.Reset();
    _AnalogMagnitudes.Reset();
    ++_Revision;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInputDebugger_KeyActivityObserver::
    DoPress(
        const FKey& InKey)
    -> void
{
    if (Get_IsHeld(InKey))
    { return; }

    _Held.Emplace(FCkInputDebugger_HeldKey{InKey, FSlateApplication::Get().GetCurrentTime()});
    ++_Revision;
}

auto
    FCkInputDebugger_KeyActivityObserver::
    DoRelease(
        const FKey& InKey)
    -> void
{
    using namespace ck_input_debugger_key_activity;

    const auto RemovedCount = _Held.RemoveAll([&](const FCkInputDebugger_HeldKey& InHeld) { return InHeld.Key == InKey; });

    if (RemovedCount == 0)
    { return; }

    _Recent.RemoveAll([&](const FCkInputDebugger_RecentKey& InRecent) { return InRecent.Key == InKey; });
    _Recent.Insert(FCkInputDebugger_RecentKey{InKey, FSlateApplication::Get().GetCurrentTime()}, 0);

    if (_Recent.Num() > MaxRecentKeys)
    { _Recent.SetNum(MaxRecentKeys); }

    ++_Revision;
}

// --------------------------------------------------------------------------------------------------------------------
