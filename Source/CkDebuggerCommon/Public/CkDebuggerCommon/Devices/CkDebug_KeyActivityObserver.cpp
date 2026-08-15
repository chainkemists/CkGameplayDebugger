#include "CkDebuggerCommon/Devices/CkDebug_KeyActivityObserver.h"

#include "Framework/Application/SlateApplication.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_key_activity
{
    constexpr auto MaxRecentKeys = 6;
    constexpr auto AnalogRestThreshold = 0.08f;
    constexpr auto MaxEdgeEpisodes = 512;

    // Deflection maps onto Get_FillFraction as HeldRunFrames / HoldVerdictFrames, so the stick
    // region fills proportionally to how far the stick is pushed.
    constexpr auto StickDeflectionVerdictFrames = 100;

    struct FStickAxisFold
    {
        FKey StickButton;
        FKey AxisX;
        FKey AxisY;
    };

    inline auto Get_StickAxisFolds() -> const TArray<FStickAxisFold>&
    {
        static const auto Folds = TArray<FStickAxisFold>{
            {EKeys::Gamepad_LeftThumbstick, EKeys::Gamepad_LeftX, EKeys::Gamepad_LeftY},
            {EKeys::Gamepad_RightThumbstick, EKeys::Gamepad_RightX, EKeys::Gamepad_RightY},
        };

        return Folds;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_KeyActivityObserver::
    Tick(
        const float InDeltaTime,
        FSlateApplication& InSlateApp,
        TSharedRef<ICursor> InCursor)
    -> void
{
    ++_LiveFrame;

    // App deactivation swallows release edges — a key held across an alt-tab would read held forever, in every
    // consumer of this observer. Releasing through DoRelease closes the episodes exactly as real key-ups would.
    if (NOT InSlateApp.IsActive() && _Held.Num() > 0)
    {
        const auto HeldCopy = _Held;
        for (const auto& Held : HeldCopy)
        { DoRelease(Held.Key); }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_KeyActivityObserver::
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
    FCkDebug_KeyActivityObserver::
    HandleKeyUpEvent(
        FSlateApplication& InSlateApp,
        const FKeyEvent& InKeyEvent)
    -> bool
{
    DoRelease(InKeyEvent.GetKey());
    return false;
}

auto
    FCkDebug_KeyActivityObserver::
    HandleMouseButtonDownEvent(
        FSlateApplication& InSlateApp,
        const FPointerEvent& InPointerEvent)
    -> bool
{
    DoPress(InPointerEvent.GetEffectingButton());
    return false;
}

auto
    FCkDebug_KeyActivityObserver::
    HandleMouseButtonDoubleClickEvent(
        FSlateApplication& InSlateApp,
        const FPointerEvent& InPointerEvent)
    -> bool
{
    // The OS classifies the second press of a rapid double click as its own event type, and the engine's
    // IInputProcessor default DROPS it — without this override the observer would read the button as not
    // held for the entire second click. DoPress is idempotent, so also receiving a regular down is fine.
    DoPress(InPointerEvent.GetEffectingButton());
    return false;
}

auto
    FCkDebug_KeyActivityObserver::
    HandleMouseButtonUpEvent(
        FSlateApplication& InSlateApp,
        const FPointerEvent& InPointerEvent)
    -> bool
{
    DoRelease(InPointerEvent.GetEffectingButton());
    return false;
}

auto
    FCkDebug_KeyActivityObserver::
    HandleAnalogInputEvent(
        FSlateApplication& InSlateApp,
        const FAnalogInputEvent& InAnalogEvent)
    -> bool
{
    using namespace ck_debug_key_activity;

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
    FCkDebug_KeyActivityObserver::
    Get_IsHeld(
        const FKey& InKey) const
    -> bool
{
    return _Held.ContainsByPredicate([&](const FCkDebug_HeldKey& InHeld) { return InHeld.Key == InKey; });
}

auto
    FCkDebug_KeyActivityObserver::
    Get_AnalogMagnitude(
        const FKey& InKey) const
    -> float
{
    if (const auto* Found = _AnalogMagnitudes.Find(InKey))
    { return *Found; }

    return 0.0f;
}

auto
    FCkDebug_KeyActivityObserver::
    Fill_DeviceSnapshot(
        FCkDebug_DeviceSnapshot& OutSnapshot) const
    -> void
{
    using namespace ck_debug_key_activity;

    OutSnapshot.LiveFrame = _LiveFrame;
    OutSnapshot.DeviceConnected = true;
    OutSnapshot.Keys = _KeyStates;

    for (auto& [Key, State] : OutSnapshot.Keys)
    {
        State.HeldRunFrames = Get_IsHeld(Key)
            ? FMath::Max(1, _LiveFrame - State.LatestPressFrame)
            : 0;
    }

    for (const auto& Fold : Get_StickAxisFolds())
    {
        const auto Deflection = FMath::Min(1.0f, static_cast<float>(
            FVector2D{Get_AnalogMagnitude(Fold.AxisX), Get_AnalogMagnitude(Fold.AxisY)}.Size()));

        if (Deflection < AnalogRestThreshold)
        { continue; }

        auto& StickState = OutSnapshot.Keys.FindOrAdd(Fold.StickButton);

        if (StickState.HeldRunFrames > 0)
        { continue; }

        StickState.HoldVerdictFrames = StickDeflectionVerdictFrames;
        StickState.HeldRunFrames = FMath::RoundToInt32(Deflection * StickDeflectionVerdictFrames);
    }
}

auto
    FCkDebug_KeyActivityObserver::
    Clear()
    -> void
{
    _Held.Reset();
    _Recent.Reset();
    _AnalogMagnitudes.Reset();
    _KeyStates.Reset();
    _EdgeHistory.Reset();
    ++_Revision;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_KeyActivityObserver::
    DoPress(
        const FKey& InKey)
    -> void
{
    if (Get_IsHeld(InKey))
    { return; }

    _Held.Emplace(FCkDebug_HeldKey{InKey, FSlateApplication::Get().GetCurrentTime()});

    auto& State = _KeyStates.FindOrAdd(InKey);
    State.LatestPressFrame = _LiveFrame;

    _EdgeHistory.Emplace(FCkDebug_KeyEdgeEpisode{InKey, _LiveFrame});

    if (_EdgeHistory.Num() > ck_debug_key_activity::MaxEdgeEpisodes)
    { _EdgeHistory.RemoveAt(0, _EdgeHistory.Num() - ck_debug_key_activity::MaxEdgeEpisodes); }

    ++_Revision;
}

auto
    FCkDebug_KeyActivityObserver::
    DoRelease(
        const FKey& InKey)
    -> void
{
    using namespace ck_debug_key_activity;

    const auto RemovedCount = _Held.RemoveAll([&](const FCkDebug_HeldKey& InHeld) { return InHeld.Key == InKey; });

    if (RemovedCount == 0)
    { return; }

    _Recent.RemoveAll([&](const FCkDebug_RecentKey& InRecent) { return InRecent.Key == InKey; });
    _Recent.Insert(FCkDebug_RecentKey{InKey, FSlateApplication::Get().GetCurrentTime()}, 0);

    if (_Recent.Num() > MaxRecentKeys)
    { _Recent.SetNum(MaxRecentKeys); }

    if (auto* State = _KeyStates.Find(InKey))
    { State->LatestReleaseFrame = _LiveFrame; }

    for (auto EpisodeIdx = _EdgeHistory.Num() - 1; EpisodeIdx >= 0; --EpisodeIdx)
    {
        if (auto& Episode = _EdgeHistory[EpisodeIdx];
            Episode.Key == InKey && Episode.ReleaseFrame == INDEX_NONE)
        {
            Episode.ReleaseFrame = _LiveFrame;
            break;
        }
    }

    ++_Revision;
}

// --------------------------------------------------------------------------------------------------------------------
