#pragma once

#include "CoreMinimal.h"

#include "InputCoreTypes.h"

// --------------------------------------------------------------------------------------------------------------------
// Device visualizer snapshot — PLAIN VALUES ONLY.
//
// The device widgets are pure presentation and this is their entire input: per-key edge frames, held runs and hold
// thresholds, all frame-indexed on ONE clock (the producer's logic-frame counter — never wall-clock, which disagrees
// with a frame-indexed record on the first hitch). No handles, no fragment reads, no debugger types: the same
// snapshot can be produced by an editor debugger reading privileged state or by a cooked game HUD reading public
// APIs, and the widget cannot tell the difference. That decoupling is what makes these widgets legal in a Runtime
// module while the debugger stays UncookedOnly.
// --------------------------------------------------------------------------------------------------------------------

struct FCkDebug_DeviceKeyState
{
    // Frame the newest press edge landed on; INDEX_NONE = never seen.
    int32 LatestPressFrame = INDEX_NONE;

    int32 LatestReleaseFrame = INDEX_NONE;

    // 0 = up. Counted in the producer's frames, so a widget never runs a second clock.
    int32 HeldRunFrames = 0;

    // The tap-versus-hold verdict point for this key, when the producer knows one (a matcher hold sibling).
    // 0 = no verdict exists — a held key fills fully instead of filling TOWARD something.
    int32 HoldVerdictFrames = 0;

    // A key the producer carries full per-frame history for, versus one it only witnessed in passing. The widget
    // renders the difference (bezel brightness) because it is a diagnostic fact: a minted key that never lights is
    // a dead pipeline, an unwitnessed unminted key is merely unwatched.
    bool IsMinted = false;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkDebug_DeviceSnapshot
{
    // The producer's newest logic-frame index. Every decay/fill computation is (LiveFrame - edge frame).
    int32 LiveFrame = INDEX_NONE;

    bool DeviceConnected = true;

    TMap<FKey, FCkDebug_DeviceKeyState> Keys;

    auto TryGet_Key(const FKey& InKey) const -> const FCkDebug_DeviceKeyState*
    {
        return Keys.Find(InKey);
    }
};

// --------------------------------------------------------------------------------------------------------------------
// The state vocabulary, shared by every device widget so a press reads identically on a keycap, a mouse button and
// a gamepad face button.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::debug_devices
{
    // How long a press flash takes to decay, in the producer's frames (~250ms at a 60 Hz record).
    inline constexpr auto FlashFrames = 15;

    // A whole device with no producer behind it (or a disconnected one) dims rather than vanishes.
    inline constexpr auto DisconnectedAlpha = 0.35f;

    inline auto Get_FlashAlpha(const FCkDebug_DeviceKeyState& InState, int32 InLiveFrame) -> float
    {
        if (InState.LatestPressFrame == INDEX_NONE || InLiveFrame == INDEX_NONE)
        { return 0.0f; }

        const auto Age = InLiveFrame - InState.LatestPressFrame;

        if (Age < 0 || Age >= FlashFrames)
        { return 0.0f; }

        return (1.0f - static_cast<float>(Age) / static_cast<float>(FlashFrames)) * 0.85f;
    }

    inline auto Get_FillFraction(const FCkDebug_DeviceKeyState& InState) -> float
    {
        if (InState.HeldRunFrames <= 0)
        { return 0.0f; }

        if (InState.HoldVerdictFrames <= 0)
        { return 1.0f; }

        return FMath::Min(1.0f,
            static_cast<float>(InState.HeldRunFrames) / static_cast<float>(InState.HoldVerdictFrames));
    }
}

// --------------------------------------------------------------------------------------------------------------------
