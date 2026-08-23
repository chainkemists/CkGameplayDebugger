#include "CkDebug_ViewportComponentPickerInputProcessor.h"

#include "CkDebuggerCommon/Picker/CkDebug_ViewportComponentPicker.h"

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"

// --------------------------------------------------------------------------------------------------------------------
FCkDebug_ViewportComponentPickerInputProcessor::FCkDebug_ViewportComponentPickerInputProcessor(
    TWeakPtr<FCkDebug_ViewportComponentPicker> InPickerWeak)
    : _PickerWeak(MoveTemp(InPickerWeak))
{
}

// --------------------------------------------------------------------------------------------------------------------

void
    FCkDebug_ViewportComponentPickerInputProcessor::
    Tick(
        const float InDeltaTime,
        FSlateApplication& InSlateApp,
        TSharedRef<ICursor> InCursor)
{
}

// --------------------------------------------------------------------------------------------------------------------

bool
    FCkDebug_ViewportComponentPickerInputProcessor::
    HandleMouseMoveEvent(
        FSlateApplication& InSlateApp,
        const FPointerEvent& InMouseEvent)
{
    const auto Picker = _PickerWeak.Pin();
    if (NOT Picker.IsValid())
    { return false; }

    Picker->OnMouseMoved(InMouseEvent.GetScreenSpacePosition());
    return false;
}

// --------------------------------------------------------------------------------------------------------------------

bool
    FCkDebug_ViewportComponentPickerInputProcessor::
    HandleMouseButtonDownEvent(
        FSlateApplication& InSlateApp,
        const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    { return false; }

    const auto Picker = _PickerWeak.Pin();
    if (NOT Picker.IsValid())
    { return false; }

    _ConsumedDown = Picker->OnMouseClicked(InMouseEvent.GetScreenSpacePosition());
    return _ConsumedDown;
}

// --------------------------------------------------------------------------------------------------------------------

bool
    FCkDebug_ViewportComponentPickerInputProcessor::
    HandleMouseButtonUpEvent(
        FSlateApplication& InSlateApp,
        const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || NOT _ConsumedDown)
    { return false; }

    _ConsumedDown = false;
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

bool
    FCkDebug_ViewportComponentPickerInputProcessor::
    HandleKeyDownEvent(
        FSlateApplication& InSlateApp,
        const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() != EKeys::Escape)
    { return false; }

    const auto Picker = _PickerWeak.Pin();
    return Picker.IsValid() && Picker->OnEscapePressed();
}

// --------------------------------------------------------------------------------------------------------------------

const TCHAR*
    FCkDebug_ViewportComponentPickerInputProcessor::
    GetDebugName() const
{
    return TEXT("CkDebugViewportComponentPicker");
}

// --------------------------------------------------------------------------------------------------------------------
