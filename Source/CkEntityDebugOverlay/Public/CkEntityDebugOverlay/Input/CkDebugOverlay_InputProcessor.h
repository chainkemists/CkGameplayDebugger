#pragma once

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "HAL/PlatformTime.h"
#include "Containers/Map.h"

// ====================================================================================================================
// Global Slate input pre-processor for the on-screen entity debug overlay.
//
// PC->WasInputKeyJustPressed dies when the user ejects from PIE (F8): keyboard input then
// routes to the editor viewport, not the game PlayerController, so the overlay's double-tap
// gestures stop registering. This pre-processor observes key-down events from
// FSlateApplication directly (regardless of game-vs-editor viewport focus) and NEVER consumes
// them (every handler returns false), so the overlay's gestures (pin / ECS-focus / cycle /
// unpin / help) work identically in possessed and ejected modes.
//
// Edge-triggered: every valid, non-repeat key-down is recorded with its timestamp. The subsystem
// drains each configured key's complete press sequence once per tick, then Clear()s any
// unconsumed presses so nothing lingers into the next tick. Lives entirely on the game thread
// (Slate input + the overlay's FTSTicker both run there), so no synchronization is needed.
// ====================================================================================================================

class FCkDebugOverlay_InputPressBuffer
{
public:
    // Returns false without mutation for repeat or invalid events.
    auto Record_KeyDown(const FKey& InKey, const bool bIsRepeat, const double InTimeSeconds) -> bool
    {
        if (bIsRepeat || NOT InKey.IsValid())
        { return false; }

        _PressTimesByKey.FindOrAdd(InKey).Add(InTimeSeconds);
        return true;
    }

    // Removes and returns every press for InKey in arrival order, preserving other keys' events.
    auto Consume_PressTimes(const FKey& InKey) -> TArray<double>
    {
        if (NOT InKey.IsValid())
        { return TArray<double>{}; }

        TArray<double> PressTimes;
        _PressTimesByKey.RemoveAndCopyValue(InKey, PressTimes);
        return PressTimes;
    }

    auto Clear() -> void { _PressTimesByKey.Reset(); }

private:
    TMap<FKey, TArray<double>> _PressTimesByKey;
};

class FCkDebugOverlay_InputProcessor : public IInputProcessor
{
public:
    // --- IInputProcessor (observe only; never consume) ---
    virtual void Tick(const float /*InDeltaTime*/, FSlateApplication& /*InSlateApp*/, TSharedRef<ICursor> /*InCursor*/) override {}

    virtual bool HandleKeyDownEvent(FSlateApplication& /*InSlateApp*/, const FKeyEvent& InKeyEvent) override
    {
        _PressedThisPoll.Record_KeyDown(
            InKeyEvent.GetKey(), InKeyEvent.IsRepeat(), FPlatformTime::Seconds());
        return false; // observe only — never consume (consuming the key-up broke the next key-down)
    }

    virtual const TCHAR* GetDebugName() const override { return TEXT("CkDebugOverlay"); }

    // --- Polling (game thread, from DoTick) ---

    // Removes and returns every physical press for this key during the current poll, in order.
    auto Consume_PressTimes(const FKey& InKey) -> TArray<double>
    { return _PressedThisPoll.Consume_PressTimes(InKey); }

    // Drop any presses not consumed this tick so they don't leak into the next tick.
    auto Clear() -> void { _PressedThisPoll.Clear(); }

private:
    FCkDebugOverlay_InputPressBuffer _PressedThisPoll;
};
