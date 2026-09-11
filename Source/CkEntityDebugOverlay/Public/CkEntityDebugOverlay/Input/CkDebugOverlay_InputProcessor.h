#pragma once

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "HAL/PlatformTime.h"
#include "Math/NumericLimits.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Templates/Function.h"

#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"

// ====================================================================================================================
// Global Slate input pre-processor for the on-screen entity debug overlay.
//
// PC->WasInputKeyJustPressed dies when the user ejects from PIE (F8): keyboard input then
// routes to the editor viewport, not the game PlayerController, so the overlay's double-tap
// gestures stop registering. This pre-processor observes key-down events from
// FSlateApplication directly (regardless of game-vs-editor viewport focus). Legacy gestures
// observe their presses; focused selection bindings capture and consume their paired key events
// so selection input cannot leak into the game viewport.
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

enum class ECkDebugOverlaySelectionInputAction : uint8
{
    Select,
    ToggleSelectionLock,
    Next,
    Previous,
    FamilyPressed,
    FamilyReleased,
    Settings,
};

struct FCkDebugOverlaySelectionInputBindings
{
    FKey SelectKey;
    FKey PreviousKey;
    FKey NextKey;
    FKey FamilyKey;
    FKey SettingsKey;
    bool SettingsRequireControl = true;
    double HoldSelectSeconds = 0.4;
};

class FCkDebugOverlay_InputProcessor : public IInputProcessor
{
public:
    explicit FCkDebugOverlay_InputProcessor(TFunction<bool()> InCanHandleSelectionInput = {})
        : _CanHandleSelectionInput(MoveTemp(InCanHandleSelectionInput))
    {}

    auto SetCanHandleSelectionInput(TFunction<bool()> InCanHandleSelectionInput) -> void
    { _CanHandleSelectionInput = MoveTemp(InCanHandleSelectionInput); }

    auto SetSelectionBindings(const UCk_DebugOverlay_InputSettings& InSettings) -> void
    {
        _SelectionBindings.SelectKey = InSettings.SelectKey;
        _SelectionBindings.PreviousKey = InSettings.PreviousKey;
        _SelectionBindings.NextKey = InSettings.NextKey;
        _SelectionBindings.FamilyKey = InSettings.FamilyKey;
        _SelectionBindings.SettingsKey = InSettings.SettingsKey;
        _SelectionBindings.SettingsRequireControl = InSettings.SettingsRequireControl;
        _SelectionBindings.HoldSelectSeconds = NormalizeSelectionHoldSeconds(static_cast<double>(InSettings.HoldSelectSeconds));
    }

    auto SetSelectionBindings(const FCkDebugOverlaySelectionInputBindings& InBindings) -> void
    {
        _SelectionBindings = InBindings;
        _SelectionBindings.HoldSelectSeconds = NormalizeSelectionHoldSeconds(_SelectionBindings.HoldSelectSeconds);
    }

    // --- IInputProcessor ---
    virtual void Tick(const float /*InDeltaTime*/, FSlateApplication& /*InSlateApp*/, TSharedRef<ICursor> /*InCursor*/) override
    {
        if (CanHandleSelectionInput())
        {
            _SelectionFocusCleared = false;
            AdvanceSelectionHold(FPlatformTime::Seconds());
        }
        else if (NOT _SelectionFocusCleared)
        {
            ClearSelectionForFocusLoss();
        }
    }

    virtual bool HandleKeyDownEvent(FSlateApplication& /*InSlateApp*/, const FKeyEvent& InKeyEvent) override
    {
        const auto bCanHandle = CanHandleSelectionInput();
        if (NOT bCanHandle)
        { ClearSelectionForFocusLoss(); }
        if (RouteSelectionKeyDown(
            InKeyEvent.GetKey(), InKeyEvent.IsRepeat(), InKeyEvent.IsControlDown(), InKeyEvent.IsAltDown(),
            InKeyEvent.IsShiftDown(), InKeyEvent.IsCommandDown(), bCanHandle, FPlatformTime::Seconds()))
        { return true; }

        if (NOT bCanHandle)
        { return false; }
        _SelectionFocusCleared = false;
        _PressedThisPoll.Record_KeyDown(
            InKeyEvent.GetKey(), InKeyEvent.IsRepeat(), FPlatformTime::Seconds());
        return false; // observe only — never consume (consuming the key-up broke the next key-down)
    }

    virtual bool HandleKeyUpEvent(FSlateApplication& /*InSlateApp*/, const FKeyEvent& InKeyEvent) override
    {
        if (NOT CanHandleSelectionInput())
        { ClearSelectionForFocusLoss(); }
        return RouteSelectionKeyUp(InKeyEvent.GetKey(), FPlatformTime::Seconds());
    }

    virtual const TCHAR* GetDebugName() const override { return TEXT("CkDebugOverlay"); }

    // --- Polling (game thread, from DoTick) ---

    // Removes and returns every physical press for this key during the current poll, in order.
    auto Consume_PressTimes(const FKey& InKey) -> TArray<double>
    { return _PressedThisPoll.Consume_PressTimes(InKey); }

    auto ConsumeSelectionActions() -> TArray<ECkDebugOverlaySelectionInputAction>
    {
        auto Result = MoveTemp(_SelectionActions);
        _SelectionActions.Reset();
        return Result;
    }

    /** Clears focus-owned input and releases a held family scope exactly once. */
    auto ClearSelectionState() -> void
    {
        _SelectionActions.Reset();
        if (NOT _HeldFamilyKeys.IsEmpty())
        { _SelectionActions.Add(ECkDebugOverlaySelectionInputAction::FamilyReleased); }
        _HeldFamilyKeys.Reset();
        _SelectHold = {};
        _SelectionFocusCleared = true;
        _PressedThisPoll.Clear();
    }

    // A focus transition must preserve its one FamilyReleased action until the subsystem drains it.
    auto ClearSelectionForFocusLoss() -> void
    {
        if (NOT _SelectionFocusCleared)
        { ClearSelectionState(); }
    }

    // Value-only routing seam for automation tests and focused UI ownership code.
    auto RouteSelectionKeyDown(
        const FKey& InKey, bool bIsRepeat, bool bControl, bool bAlt, bool bShift, bool bCommand,
        bool bCanHandle, const double InTimeSeconds = FPlatformTime::Seconds()) -> bool
    {
        if (NOT InKey.IsValid())
        { return false; }
        if (bIsRepeat)
        { return _CapturedKeys.Contains(InKey); }
        if (NOT bCanHandle)
        { return false; }
        _SelectionFocusCleared = false;

        const auto bUnmodified = NOT bControl && NOT bAlt && NOT bShift && NOT bCommand;
        if (bUnmodified && InKey == _SelectionBindings.SelectKey && InKey.IsValid())
        {
            _CapturedKeys.Add(InKey);
            _SelectHold = { InKey, InTimeSeconds, false, FMath::IsFinite(InTimeSeconds) };
            return true;
        }
        if (bUnmodified && InKey == _SelectionBindings.NextKey && InKey.IsValid())
        { _CapturedKeys.Add(InKey); _SelectionActions.Add(ECkDebugOverlaySelectionInputAction::Next); return true; }
        if (bUnmodified && InKey == _SelectionBindings.PreviousKey && InKey.IsValid())
        { _CapturedKeys.Add(InKey); _SelectionActions.Add(ECkDebugOverlaySelectionInputAction::Previous); return true; }
        if (bUnmodified && InKey == _SelectionBindings.FamilyKey && InKey.IsValid())
        {
            _CapturedKeys.Add(InKey);
            _HeldFamilyKeys.Add(InKey);
            _SelectionActions.Add(ECkDebugOverlaySelectionInputAction::FamilyPressed);
            return true;
        }
        const auto bSettingsChord = _SelectionBindings.SettingsRequireControl
            ? bControl && NOT bAlt && NOT bShift && NOT bCommand
            : bUnmodified;
        if (bSettingsChord && InKey == _SelectionBindings.SettingsKey && InKey.IsValid())
        { _CapturedKeys.Add(InKey); _SelectionActions.Add(ECkDebugOverlaySelectionInputAction::Settings); return true; }
        return false;
    }

    auto RouteSelectionKeyUp(const FKey& InKey, const double InTimeSeconds = FPlatformTime::Seconds()) -> bool
    {
        if (NOT _CapturedKeys.Contains(InKey))
        { return false; }
        _CapturedKeys.Remove(InKey);
        if (_SelectHold.Key == InKey)
        {
            AdvanceSelectionHold(InTimeSeconds);
            if (_SelectHold.TimestampValid && NOT _SelectHold.ToggleEmitted)
            { _SelectionActions.Add(ECkDebugOverlaySelectionInputAction::Select); }
            _SelectHold = {};
            return true;
        }
        if (NOT _HeldFamilyKeys.Contains(InKey))
        { return true; }
        _HeldFamilyKeys.Remove(InKey);
        _SelectionActions.Add(ECkDebugOverlaySelectionInputAction::FamilyReleased);
        return true;
    }

    // Explicit timestamp seam: tests and the Slate tick share the same hold semantics.
    auto AdvanceSelectionHold(const double InTimeSeconds) -> void
    {
        if (NOT _SelectHold.Key.IsValid() || _SelectHold.ToggleEmitted || NOT _SelectHold.TimestampValid)
        { return; }
        if (NOT FMath::IsFinite(InTimeSeconds) || InTimeSeconds < _SelectHold.PressTime)
        {
            _SelectHold.TimestampValid = false;
            return;
        }
        if (InTimeSeconds - _SelectHold.PressTime >= _SelectionBindings.HoldSelectSeconds)
        {
            _SelectionActions.Add(ECkDebugOverlaySelectionInputAction::ToggleSelectionLock);
            _SelectHold.ToggleEmitted = true;
        }
    }

    // Drop any presses not consumed this tick so they don't leak into the next tick.
    auto Clear() -> void { _PressedThisPoll.Clear(); }

private:
    struct FSelectHold
    {
        FKey Key;
        double PressTime = 0.0;
        bool ToggleEmitted = false;
        bool TimestampValid = true;
    };

    // Persisted malformed values fail closed to the documented default instead of toggling immediately.
    static auto NormalizeSelectionHoldSeconds(const double InSeconds) -> double
    { return FMath::IsFinite(InSeconds) ? FMath::Clamp(InSeconds, 0.05, 3.0) : 0.4; }

    auto CanHandleSelectionInput() const -> bool
    { return _CanHandleSelectionInput && _CanHandleSelectionInput(); }

    FCkDebugOverlay_InputPressBuffer _PressedThisPoll;
    FCkDebugOverlaySelectionInputBindings _SelectionBindings;
    TFunction<bool()> _CanHandleSelectionInput;
    TSet<FKey> _CapturedKeys;
    TSet<FKey> _HeldFamilyKeys;
    FSelectHold _SelectHold;
    bool _SelectionFocusCleared = false;
    TArray<ECkDebugOverlaySelectionInputAction> _SelectionActions;
};
