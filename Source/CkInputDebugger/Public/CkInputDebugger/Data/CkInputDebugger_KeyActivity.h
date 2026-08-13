#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

#include "Framework/Application/IInputProcessor.h"

// ====================================================================================================================
// Passive application-wide input observer feeding the Input Debugger's live surfaces (held-key
// strip, device-visual pressed states). PASSIVE is the contract: every handler returns false so
// nothing downstream — viewport camera tracking included — is starved (ck-slate-tools §3).
//
// Records physical KEY state only (FKey + timestamps); it holds no handles and no UObjects, so it
// is safe across PIE sessions. Held state is still cleared on EndPIE — a key held while PIE dies
// would otherwise read held forever (its release goes to a dead world).
// ====================================================================================================================

struct FCkInputDebugger_HeldKey
{
    FKey   Key;
    double PressedAt = 0.0;
};

struct FCkInputDebugger_RecentKey
{
    FKey   Key;
    double ReleasedAt = 0.0;
};

class FCkInputDebugger_KeyActivityObserver : public IInputProcessor
{
public:
    auto Tick(const float InDeltaTime, FSlateApplication& InSlateApp, TSharedRef<ICursor> InCursor) -> void override {}

    auto HandleKeyDownEvent(FSlateApplication& InSlateApp, const FKeyEvent& InKeyEvent) -> bool override;
    auto HandleKeyUpEvent(FSlateApplication& InSlateApp, const FKeyEvent& InKeyEvent) -> bool override;
    auto HandleMouseButtonDownEvent(FSlateApplication& InSlateApp, const FPointerEvent& InPointerEvent) -> bool override;
    auto HandleMouseButtonUpEvent(FSlateApplication& InSlateApp, const FPointerEvent& InPointerEvent) -> bool override;
    auto HandleAnalogInputEvent(FSlateApplication& InSlateApp, const FAnalogInputEvent& InAnalogEvent) -> bool override;

public:
    auto Get_IsHeld(const FKey& InKey) const -> bool;
    auto Get_HeldKeys() const -> const TArray<FCkInputDebugger_HeldKey>& { return _Held; }
    auto Get_RecentKeys() const -> const TArray<FCkInputDebugger_RecentKey>& { return _Recent; }

    /** Last observed magnitude for an analog key (gamepad axes), decayed to 0 on returning to rest. */
    auto Get_AnalogMagnitude(const FKey& InKey) const -> float;

    /** Changes whenever the held/recent sets change — the strip's cheap rebuild gate. */
    auto Get_ActivityRevision() const -> int32 { return _Revision; }

    auto Clear() -> void;

private:
    auto DoPress(const FKey& InKey) -> void;
    auto DoRelease(const FKey& InKey) -> void;

private:
    TArray<FCkInputDebugger_HeldKey>   _Held;
    TArray<FCkInputDebugger_RecentKey> _Recent;
    TMap<FKey, float>                  _AnalogMagnitudes;
    int32                              _Revision = 0;
};

// ====================================================================================================================
