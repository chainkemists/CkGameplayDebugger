#pragma once

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Containers/Set.h"

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
// Edge-triggered: only non-repeat key-downs are recorded. The subsystem drains the set each
// tick via Consume_WasJustPressed (one read per key), then Clear()s any unconsumed presses so
// nothing lingers into the next tick. Lives entirely on the game thread (Slate input + the
// overlay's FTSTicker both run there), so no synchronization is needed.
// ====================================================================================================================

class FCkDebugOverlay_InputProcessor : public IInputProcessor
{
public:
    // --- IInputProcessor (observe only; never consume) ---
    virtual void Tick(const float /*InDeltaTime*/, FSlateApplication& /*InSlateApp*/, TSharedRef<ICursor> /*InCursor*/) override {}

    virtual bool HandleKeyDownEvent(FSlateApplication& /*InSlateApp*/, const FKeyEvent& InKeyEvent) override
    {
        if (InKeyEvent.IsRepeat() == false)
        { _PressedThisPoll.Add(InKeyEvent.GetKey()); }
        // Consume (swallow) keys the overlay fully owns so Slate/OS defaults don't fire — e.g.
        // a lone Alt press otherwise activates the Windows menu chrome, which shows the cursor
        // and defocuses the game viewport. The press is still recorded above for our detection.
        return _ConsumeKeys.Contains(InKeyEvent.GetKey());
    }

    virtual bool HandleKeyUpEvent(FSlateApplication& /*InSlateApp*/, const FKeyEvent& InKeyEvent) override
    {
        // Swallow the matching key-up too (the Alt menu activates on release).
        return _ConsumeKeys.Contains(InKeyEvent.GetKey());
    }

    virtual const TCHAR* GetDebugName() const override { return TEXT("CkDebugOverlay"); }

    // --- Polling (game thread, from DoTick) ---

    // Edge-triggered: true exactly once per physical key press (removes it from the set).
    auto Consume_WasJustPressed(const FKey& InKey) -> bool
    {
        return InKey.IsValid() && _PressedThisPoll.Remove(InKey) > 0;
    }

    // Drop any presses not consumed this tick so they don't leak into the next tick.
    auto Clear() -> void { _PressedThisPoll.Reset(); }

    // Keys the overlay fully owns (swallowed from the game/Slate). Set by the subsystem to the
    // gesture keys that would otherwise trigger Slate/OS side-effects (the cycle key — Alt).
    auto Set_ConsumeKeys(const TSet<FKey>& InKeys) -> void { _ConsumeKeys = InKeys; }

private:
    TSet<FKey> _PressedThisPoll;
    TSet<FKey> _ConsumeKeys;
};
