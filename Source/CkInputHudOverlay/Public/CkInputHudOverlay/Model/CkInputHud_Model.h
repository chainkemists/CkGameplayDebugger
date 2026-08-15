#pragma once

#include "CommonInputBaseTypes.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// The HUD overlay's view model — PLAIN VALUES ONLY.
//
// Deliberately holds no FCk_Handle and no UObject pointer: it is co-owned by a ULocalPlayerSubsystem and by a Slate
// widget that outlives PIE, and a handle stored here would hold the PIE registry by value and access-violate on
// destruct at the next PIE start. The collector re-resolves every entity fresh on each tick and writes values in.
//
// The device area is an EVENT STREAM, not a device snapshot: one entry per press-lifecycle, so two presses of one
// key are two entries. An entry with no up-edge is still held, and held entries are pinned — never faded, never
// counted against the history cap.
// --------------------------------------------------------------------------------------------------------------------

/** What a chip's pulse glyph is saying, derived from the entry's down-time and whether it has been released. */
enum class ECk_InputHud_EventKind : uint8
{
    // Down, and not yet past the tap/hold threshold.
    Press,
    // Down, past the threshold — the bar is growing.
    Hold,
    // Released before the threshold.
    Tap,
    // Released after the threshold; the bar and its duration are frozen.
    HoldRelease
};

// --------------------------------------------------------------------------------------------------------------------

struct FCk_InputHud_Event
{
    FString KeyLabel;

    // The FKey's own name, kept so an open entry can be checked against PHYSICAL key state — the routed release
    // this entry normally closes on is not guaranteed to arrive (see Close_OpenEventsNotHeld).
    FName KeyName;

    // The intent the press resolved to, the layer that consumed it, or the unrouted marker.
    FString IntentLabel;

    bool Resolved = false;

    // Modifier keys are differentiated by SHAPE rather than by colour — colour is spoken for by resolution state.
    bool Modifier = false;

    // The sampler's own logic frames. INDEX_NONE on the routed-events fallback path, which has no frame record to
    // name, so the frame deck simply has nothing to draw there.
    int32 DownFrame = INDEX_NONE;

    // INDEX_NONE while the key is still down — and also on the fallback path, which is why HELD-ness is read off
    // UpTimeSeconds instead: a collector always stamps a release with a time, and only sometimes with a frame.
    int32 UpFrame = INDEX_NONE;

    double DownTimeSeconds = 0.0;
    double UpTimeSeconds   = 0.0;
};

// --------------------------------------------------------------------------------------------------------------------

class CKINPUTHUDOVERLAY_API FCk_InputHud_Model
{
public:
    // The fraction of a released entry's fade lifetime it holds FULL opacity for, before the linear ramp to zero.
    static constexpr float FadeHoldFraction = 0.3f;

public:
    /** Drop every volatile reading. Called on deactivate and whenever the input source stops resolving. */
    auto Reset() -> void;

    /**
     * Clear what describes a source we can no longer see. Open entries are CLOSED rather than dropped: a key held
     * when the source vanished never delivers its release, and an entry left open would pin forever.
     */
    auto Reset_VolatileState(double InNowSeconds) -> void;

public:
    // ---- Event stream ----

    /** Append a new press-lifecycle. Returns the index written — stable until the next prune/eviction. */
    auto Open_Event(
        const FString& InKeyLabel,
        const FName&   InKeyName,
        int32          InDownFrame,
        double         InDownTimeSeconds,
        bool           InIsModifier) -> int32;

    /** The newest still-open entry for InKeyLabel, or INDEX_NONE. */
    auto TryGet_OpenEvent(
        const FString& InKeyLabel) const -> int32;

    auto Close_Event(
        int32  InIndex,
        int32  InUpFrame,
        double InUpTimeSeconds) -> void;

    auto Set_EventResolution(
        int32          InIndex,
        const FString& InIntentLabel,
        bool           InResolved) -> void;

    auto Get_Events() const -> const TArray<FCk_InputHud_Event>& { return _Events; }

    auto Get_HeldNum()     const -> int32;
    auto Get_ReleasedNum() const -> int32;

    /** Evict the oldest RELEASED entries until at most InHistoryCap remain. Held entries are never evicted. */
    auto Enforce_HistoryCap(int32 InHistoryCap) -> void;

    /**
     * Close every open entry whose key the predicate says is no longer PHYSICALLY held. A routed release is not
     * guaranteed to arrive: a viewport-focus flush emits none, and within-frame ordering (raw events carry a
     * fidelity flag, not an order guarantee) can pair a rapid click as release-then-press, orphaning the press.
     * The caller supplies physical ground truth (the key-activity observer); anything it refutes closes here,
     * one collect late at worst, with no up-frame to name.
     */
    auto Close_OpenEventsNotHeld(
        TFunctionRef<bool(const FName&)> InIsPhysicallyHeld,
        double                           InNowSeconds) -> void;

    /** Drop every released entry whose fade has run out. */
    auto Prune_FadedEvents(
        double InNowSeconds,
        float  InFadeLifetimeSeconds) -> void;

public:
    // ---- Shared derivations (written ONCE — the collector's prune and the ribbon's paint read the same math) ----

    static auto Get_IsHeld(const FCk_InputHud_Event& InEvent) -> bool { return InEvent.UpTimeSeconds <= 0.0; }

    /** Down-time in seconds: live for a held entry, frozen at the release for a closed one. */
    static auto Get_DurationSeconds(
        const FCk_InputHud_Event& InEvent,
        double                    InNowSeconds) -> double;

    static auto Get_EventKind(
        const FCk_InputHud_Event& InEvent,
        double                    InNowSeconds,
        float                     InTapHoldThresholdMs) -> ECk_InputHud_EventKind;

    /** 1 while held or inside the hold fraction, then linear to 0 at the end of the lifetime. */
    static auto Get_FadeOpacity(
        const FCk_InputHud_Event& InEvent,
        double                    InNowSeconds,
        float                     InFadeLifetimeSeconds) -> float;

public:
    // ---- Layer line ----

    /**
     * Rebuild the layer line only when the (priority, name) list actually changes. Returns true when a rebuild
     * happened, so a caller can tell a no-op tick from a real one.
     */
    auto Set_Layers(
        const TArray<TPair<int32, FString>>& InLayers) -> bool;

    auto Get_LayerLine() const -> const FString& { return _LayerLine; }

public:
    // ---- Live readings ----

    auto Get_ActiveInputType() const -> ECommonInputType { return _ActiveInputType; }
    auto Set_ActiveInputType(ECommonInputType InType) -> void { _ActiveInputType = InType; }

    auto Get_LeftStick()  const -> const FVector2f& { return _LeftStick; }
    auto Get_RightStick() const -> const FVector2f& { return _RightStick; }
    auto Set_LeftStick(const FVector2f& InValue)  -> void { _LeftStick = InValue; }
    auto Set_RightStick(const FVector2f& InValue) -> void { _RightStick = InValue; }

    auto Get_HasSource() const -> bool { return _HasSource; }
    auto Set_HasSource(bool InValue) -> void { _HasSource = InValue; }

public:
    // ---- Collector bookkeeping ----

    auto Get_LastSeenSamplerFrame() const -> int32 { return _LastSeenSamplerFrame; }
    auto Set_LastSeenSamplerFrame(int32 InFrame) -> void { _LastSeenSamplerFrame = InFrame; }

private:
    // Oldest first. Held and released entries share one array so an entry's identity survives its release.
    TArray<FCk_InputHud_Event> _Events;

    // The (priority, name) list the current layer line was built from. Compared verbatim so the line is rebuilt
    // only when the stack really changed, not on every tick.
    FString _LayerCompareKey;
    FString _LayerLine;

    ECommonInputType _ActiveInputType = ECommonInputType::MouseAndKeyboard;

    FVector2f _LeftStick  = FVector2f::ZeroVector;
    FVector2f _RightStick = FVector2f::ZeroVector;

    bool _HasSource = false;

    int32 _LastSeenSamplerFrame = INDEX_NONE;
};

// --------------------------------------------------------------------------------------------------------------------
