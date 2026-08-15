#pragma once

#include "CkDebuggerCommon/Devices/CkDebug_DeviceTypes.h"

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

#include "Framework/Application/IInputProcessor.h"

// ====================================================================================================================
// Passive application-wide input observer feeding shared debugger surfaces (held-key strip,
// device-visual pressed states). PASSIVE is the contract: every handler returns false so nothing
// downstream — viewport camera tracking included — is starved (ck-slate-tools §3).
//
// Records physical KEY state only (FKey + timestamps); it holds no handles and no UObjects, so it
// is safe across PIE sessions. Held state is still cleared on EndPIE — a key held while PIE dies
// would otherwise read held forever (its release goes to a dead world).
//
// Doubles as the PRODUCER for the shared device visualizers (SCkDebug_DeviceKeyboard / Mouse /
// Gamepad): its Slate-tick counter is the snapshot's one clock, press/release edges land as
// frame-indexed key states, and stick deflection folds into the stick-button keys so the pad's
// sticks fill proportionally to how far they are pushed.
// ====================================================================================================================

struct FCkDebug_HeldKey
{
    FKey   Key;
    double PressedAt = 0.0;
};

struct FCkDebug_RecentKey
{
    FKey   Key;
    double ReleasedAt = 0.0;
};

// One press..release run of one key, frame-indexed on the observer's clock — the timeline's row unit.
struct FCkDebug_KeyEdgeEpisode
{
    FKey  Key;
    int32 PressFrame = 0;
    int32 ReleaseFrame = INDEX_NONE; // INDEX_NONE while still held
};

class CKDEBUGGERCOMMON_API FCkDebug_KeyActivityObserver : public IInputProcessor
{
public:
    auto Tick(const float InDeltaTime, FSlateApplication& InSlateApp, TSharedRef<ICursor> InCursor) -> void override;

    auto HandleKeyDownEvent(FSlateApplication& InSlateApp, const FKeyEvent& InKeyEvent) -> bool override;
    auto HandleKeyUpEvent(FSlateApplication& InSlateApp, const FKeyEvent& InKeyEvent) -> bool override;
    auto HandleMouseButtonDownEvent(FSlateApplication& InSlateApp, const FPointerEvent& InPointerEvent) -> bool override;
    auto HandleMouseButtonDoubleClickEvent(FSlateApplication& InSlateApp, const FPointerEvent& InPointerEvent) -> bool override;
    auto HandleMouseButtonUpEvent(FSlateApplication& InSlateApp, const FPointerEvent& InPointerEvent) -> bool override;
    auto HandleAnalogInputEvent(FSlateApplication& InSlateApp, const FAnalogInputEvent& InAnalogEvent) -> bool override;

public:
    auto Get_IsHeld(const FKey& InKey) const -> bool;
    auto Get_HeldKeys() const -> const TArray<FCkDebug_HeldKey>& { return _Held; }
    auto Get_RecentKeys() const -> const TArray<FCkDebug_RecentKey>& { return _Recent; }

    /** Last observed magnitude for an analog key (gamepad axes), decayed to 0 on returning to rest. */
    auto Get_AnalogMagnitude(const FKey& InKey) const -> float;

    /** Changes whenever the held/recent sets change — the strip's cheap rebuild gate. */
    auto Get_ActivityRevision() const -> int32 { return _Revision; }

    /**
     * Write the shared device-visual snapshot: frame-indexed press/release edges for every key
     * seen this session, held runs against the observer's Slate-tick clock, and stick deflection
     * folded into the stick-button keys (fill fraction = magnitude). Presentation overlays
     * (mapped / rebound / highlighted) are the CONSUMER's to stamp — this observer only knows
     * physical edges.
     */
    auto Fill_DeviceSnapshot(FCkDebug_DeviceSnapshot& OutSnapshot) const -> void;

    /** Frame-indexed press/release episodes (oldest first, ring-capped) — the timeline's data. */
    auto Get_EdgeHistory() const -> const TArray<FCkDebug_KeyEdgeEpisode>& { return _EdgeHistory; }

    auto Get_LiveFrame() const -> int32 { return _LiveFrame; }

    auto Clear() -> void;

private:
    auto DoPress(const FKey& InKey) -> void;
    auto DoRelease(const FKey& InKey) -> void;

private:
    TArray<FCkDebug_HeldKey>             _Held;
    TArray<FCkDebug_RecentKey>           _Recent;
    TMap<FKey, float>                    _AnalogMagnitudes;
    TMap<FKey, FCkDebug_DeviceKeyState>  _KeyStates;
    TArray<FCkDebug_KeyEdgeEpisode>      _EdgeHistory;
    int32                                _Revision = 0;
    int32                                _LiveFrame = 0;
};

// ====================================================================================================================
