#include "CkInputHudOverlay/Model/CkInputHud_Model.h"

#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_InputHud_Model::
    Reset()
    -> void
{
    _Events.Reset();

    _LayerCompareKey.Reset();
    _LayerLine.Reset();
    _LayerPrimary.Reset();
    _LayerRemainder.Reset();

    _ActiveInputType = ECommonInputType::MouseAndKeyboard;

    _LeftStick  = FVector2f::ZeroVector;
    _RightStick = FVector2f::ZeroVector;

    _HasSource = false;

    _LastSeenSamplerFrame = INDEX_NONE;
}

auto
    FCk_InputHud_Model::
    Reset_VolatileState(
        double InNowSeconds)
    -> void
{
    for (auto& Event : _Events)
    {
        if (NOT Get_IsHeld(Event))
        { continue; }

        Event.UpTimeSeconds = InNowSeconds;
    }

    _LayerCompareKey.Reset();
    _LayerLine.Reset();
    _LayerPrimary.Reset();
    _LayerRemainder.Reset();

    _LeftStick  = FVector2f::ZeroVector;
    _RightStick = FVector2f::ZeroVector;

    _LastSeenSamplerFrame = INDEX_NONE;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_InputHud_Model::
    Open_Event(
        const FString& InKeyLabel,
        const FName&   InKeyName,
        int32          InDownFrame,
        double         InDownTimeSeconds,
        bool           InIsModifier)
    -> int32
{
    auto Event = FCk_InputHud_Event{};
    Event.KeyLabel        = InKeyLabel;
    Event.KeyName         = InKeyName;
    Event.DownFrame       = InDownFrame;
    Event.DownTimeSeconds = InDownTimeSeconds;
    Event.Modifier        = InIsModifier;

    return _Events.Add(MoveTemp(Event));
}

auto
    FCk_InputHud_Model::
    TryGet_OpenEvent(
        const FString& InKeyLabel) const
    -> int32
{
    // Newest backwards: a key pressed twice before either release resolves to the press the release belongs to.
    for (auto Index = _Events.Num() - 1; Index >= 0; --Index)
    {
        const auto& Event = _Events[Index];

        if (Get_IsHeld(Event) && Event.KeyLabel == InKeyLabel)
        { return Index; }
    }

    return INDEX_NONE;
}

auto
    FCk_InputHud_Model::
    Close_Event(
        int32  InIndex,
        int32  InUpFrame,
        double InUpTimeSeconds)
    -> void
{
    if (NOT _Events.IsValidIndex(InIndex))
    { return; }

    auto& Event = _Events[InIndex];

    Event.UpFrame = InUpFrame;

    // A release stamped at or before the press would read as still-held and pin the chip forever.
    Event.UpTimeSeconds = FMath::Max(InUpTimeSeconds, Event.DownTimeSeconds + UE_KINDA_SMALL_NUMBER);
}

auto
    FCk_InputHud_Model::
    Set_EventResolution(
        int32          InIndex,
        const FString& InIntentLabel,
        bool           InResolved)
    -> void
{
    if (NOT _Events.IsValidIndex(InIndex))
    { return; }

    auto& Event = _Events[InIndex];

    Event.IntentLabel = InIntentLabel;
    Event.Resolved    = InResolved;
}

auto
    FCk_InputHud_Model::
    Get_HeldNum() const
    -> int32
{
    auto Num = 0;

    for (const auto& Event : _Events)
    {
        if (Get_IsHeld(Event))
        { ++Num; }
    }

    return Num;
}

auto
    FCk_InputHud_Model::
    Get_ReleasedNum() const
    -> int32
{
    return _Events.Num() - Get_HeldNum();
}

auto
    FCk_InputHud_Model::
    Enforce_HistoryCap(
        int32 InHistoryCap)
    -> void
{
    const auto Cap = FMath::Max(InHistoryCap, 0);

    auto Excess = Get_ReleasedNum() - Cap;
    if (Excess <= 0)
    { return; }

    // Oldest first, and only released entries — a held chip is pinned and outlives any number of releases.
    for (auto Index = 0; Index < _Events.Num() && Excess > 0;)
    {
        if (Get_IsHeld(_Events[Index]))
        { ++Index; continue; }

        _Events.RemoveAt(Index);
        --Excess;
    }
}

auto
    FCk_InputHud_Model::
    Close_OpenEventsNotHeld(
        TFunctionRef<bool(const FName&)> InIsPhysicallyHeld,
        double                           InNowSeconds)
    -> void
{
    for (auto Index = 0; Index < _Events.Num(); ++Index)
    {
        const auto& Event = _Events[Index];

        if (NOT Get_IsHeld(Event))
        { continue; }

        if (InIsPhysicallyHeld(Event.KeyName))
        { continue; }

        Close_Event(Index, INDEX_NONE, InNowSeconds);
    }
}

auto
    FCk_InputHud_Model::
    Prune_FadedEvents(
        double InNowSeconds,
        float  InFadeLifetimeSeconds)
    -> void
{
    _Events.RemoveAll([InNowSeconds, InFadeLifetimeSeconds](const FCk_InputHud_Event& InEvent) -> bool
    {
        return Get_FadeOpacity(InEvent, InNowSeconds, InFadeLifetimeSeconds) <= 0.0f;
    });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_InputHud_Model::
    Get_DurationSeconds(
        const FCk_InputHud_Event& InEvent,
        double                    InNowSeconds)
    -> double
{
    const auto End = Get_IsHeld(InEvent) ? InNowSeconds : InEvent.UpTimeSeconds;

    return FMath::Max(0.0, End - InEvent.DownTimeSeconds);
}

auto
    FCk_InputHud_Model::
    Get_EventKind(
        const FCk_InputHud_Event& InEvent,
        double                    InNowSeconds,
        float                     InTapHoldThresholdMs)
    -> ECk_InputHud_EventKind
{
    const auto DurationMs  = Get_DurationSeconds(InEvent, InNowSeconds) * 1000.0;
    const auto PastThreshold = DurationMs >= static_cast<double>(InTapHoldThresholdMs);

    if (Get_IsHeld(InEvent))
    { return PastThreshold ? ECk_InputHud_EventKind::Hold : ECk_InputHud_EventKind::Press; }

    return PastThreshold ? ECk_InputHud_EventKind::HoldRelease : ECk_InputHud_EventKind::Tap;
}

auto
    FCk_InputHud_Model::
    Get_FadeOpacity(
        const FCk_InputHud_Event& InEvent,
        double                    InNowSeconds,
        float                     InFadeLifetimeSeconds)
    -> float
{
    if (Get_IsHeld(InEvent))
    { return 1.0f; }

    const auto Lifetime = static_cast<double>(FMath::Max(InFadeLifetimeSeconds, UE_KINDA_SMALL_NUMBER));
    const auto Elapsed  = FMath::Max(0.0, InNowSeconds - InEvent.UpTimeSeconds);

    return static_cast<float>(FMath::Clamp(1.0 - Elapsed / Lifetime, 0.0, 1.0));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_InputHud_Model::
    Set_Layers(
        const TArray<TPair<int32, FString>>& InLayers)
    -> bool
{
    auto CompareKey = FString{};
    for (const auto& Layer : InLayers)
    {
        CompareKey.Appendf(TEXT("%d:%s|"), Layer.Key, *Layer.Value);
    }

    if (CompareKey == _LayerCompareKey)
    { return false; }

    _LayerCompareKey = MoveTemp(CompareKey);

    _LayerLine.Reset();
    _LayerPrimary.Reset();
    _LayerRemainder.Reset();
    for (auto Index = 0; Index < InLayers.Num(); ++Index)
    {
        if (Index > 0)
        { _LayerLine.Append(TEXT(" · ")); }

        _LayerLine.Append(InLayers[Index].Value);

        if (Index == 0)
        {
            _LayerPrimary = InLayers[Index].Value;
            continue;
        }

        if (Index > 1)
        { _LayerRemainder.Append(TEXT(" · ")); }

        _LayerRemainder.Append(InLayers[Index].Value);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
