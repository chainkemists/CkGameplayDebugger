#include "CkDebug_ViewportPickerInputProcessor.h"

#include "CkDebuggerCommon/Picker/CkDebug_ViewportPicker.h"

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"

// --------------------------------------------------------------------------------------------------------------------

FCkDebug_ViewportPickerInputProcessor::FCkDebug_ViewportPickerInputProcessor(
    TWeakPtr<FCkDebug_ViewportPicker> InPickerWeak)
    : _PickerWeak(MoveTemp(InPickerWeak))
{
}

// --------------------------------------------------------------------------------------------------------------------

void
    FCkDebug_ViewportPickerInputProcessor::
    Tick(
        const float InDeltaTime,
        FSlateApplication& InSlateApp,
        TSharedRef<ICursor> InCursor)
{
    // No per-tick work required — the picker ticks itself from its host window.
}

// --------------------------------------------------------------------------------------------------------------------

bool
    FCkDebug_ViewportPickerInputProcessor::
    HandleMouseMoveEvent(
        FSlateApplication& InSlateApp,
        const FPointerEvent& InMouseEvent)
{
    const auto Picker = _PickerWeak.Pin();
    if (NOT Picker.IsValid())
    { return false; }

    Picker->OnMouseMoved(InMouseEvent.GetScreenSpacePosition());

    // Never consume mouse-move events — let the editor continue updating cursor state and tooltips.
    return false;
}

// --------------------------------------------------------------------------------------------------------------------

bool
    FCkDebug_ViewportPickerInputProcessor::
    HandleMouseButtonDownEvent(
        FSlateApplication& InSlateApp,
        const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    { return false; }

    const auto Picker = _PickerWeak.Pin();
    if (NOT Picker.IsValid())
    { return false; }

    const auto Consumed = Picker->OnMouseClicked(InMouseEvent.GetScreenSpacePosition());

    if (Consumed)
    {
        _ConsumedDown = true;
        return true;
    }

    return false;
}

// --------------------------------------------------------------------------------------------------------------------

bool
    FCkDebug_ViewportPickerInputProcessor::
    HandleMouseButtonUpEvent(
        FSlateApplication& InSlateApp,
        const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    { return false; }

    if (NOT _ConsumedDown)
    { return false; }

    // Swallow the matching mouse-up so the game viewport does not receive a dangling release event.
    _ConsumedDown = false;
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

bool
    FCkDebug_ViewportPickerInputProcessor::
    HandleKeyDownEvent(
        FSlateApplication& InSlateApp,
        const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() != EKeys::Escape)
    { return false; }

    const auto Picker = _PickerWeak.Pin();
    if (NOT Picker.IsValid())
    { return false; }

    return Picker->OnEscapePressed();
}

// --------------------------------------------------------------------------------------------------------------------

const TCHAR*
    FCkDebug_ViewportPickerInputProcessor::
    GetDebugName() const
{
    return TEXT("CkDebugViewportPicker");
}
