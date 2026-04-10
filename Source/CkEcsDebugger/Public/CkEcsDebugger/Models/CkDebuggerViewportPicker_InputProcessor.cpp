#include "CkDebuggerViewportPicker_InputProcessor.h"

#include "CkEcsDebugger/Models/CkDebuggerModel_ViewportPicker.h"

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"

// --------------------------------------------------------------------------------------------------------------------

FCkDebuggerViewportPicker_InputProcessor::FCkDebuggerViewportPicker_InputProcessor(
    TWeakPtr<FCkDebuggerModel_ViewportPicker> InModelWeak)
    : _ModelWeak(MoveTemp(InModelWeak))
{
}

// --------------------------------------------------------------------------------------------------------------------

void
    FCkDebuggerViewportPicker_InputProcessor::
    Tick(
        const float InDeltaTime,
        FSlateApplication& InSlateApp,
        TSharedRef<ICursor> InCursor)
{
    // No per-tick work required — the picker model ticks itself from the main window.
}

// --------------------------------------------------------------------------------------------------------------------

bool
    FCkDebuggerViewportPicker_InputProcessor::
    HandleMouseMoveEvent(
        FSlateApplication& InSlateApp,
        const FPointerEvent& InMouseEvent)
{
    const auto Model = _ModelWeak.Pin();
    if (NOT Model.IsValid())
    { return false; }

    Model->OnMouseMoved(InMouseEvent.GetScreenSpacePosition());

    // Never consume mouse-move events — let the editor continue updating cursor state and tooltips.
    return false;
}

// --------------------------------------------------------------------------------------------------------------------

bool
    FCkDebuggerViewportPicker_InputProcessor::
    HandleMouseButtonDownEvent(
        FSlateApplication& InSlateApp,
        const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    { return false; }

    const auto Model = _ModelWeak.Pin();
    if (NOT Model.IsValid())
    { return false; }

    const auto Consumed = Model->OnMouseClicked(InMouseEvent.GetScreenSpacePosition());

    if (Consumed)
    {
        _ConsumedDown = true;
        return true;
    }

    return false;
}

// --------------------------------------------------------------------------------------------------------------------

bool
    FCkDebuggerViewportPicker_InputProcessor::
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
    FCkDebuggerViewportPicker_InputProcessor::
    HandleKeyDownEvent(
        FSlateApplication& InSlateApp,
        const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() != EKeys::Escape)
    { return false; }

    const auto Model = _ModelWeak.Pin();
    if (NOT Model.IsValid())
    { return false; }

    return Model->OnEscapePressed();
}

// --------------------------------------------------------------------------------------------------------------------

const TCHAR*
    FCkDebuggerViewportPicker_InputProcessor::
    GetDebugName() const
{
    return TEXT("CkDebuggerViewportPicker");
}
