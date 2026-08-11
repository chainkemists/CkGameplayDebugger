#pragma once

#include "CoreMinimal.h"

#include "Framework/Application/IInputProcessor.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkDebug_ViewportPicker;

// --------------------------------------------------------------------------------------------------------------------

/**
 * Slate pre-input processor that captures mouse and keyboard events while the debugger's
 * viewport picker mode is active. Forwards relevant events to the owning picker.
 *
 * Holds only a TWeakPtr back to the picker so that destruction of the picker cannot
 * leave a dangling pointer inside FSlateApplication.
 */
class CKDEBUGGERCOMMON_API FCkDebug_ViewportPickerInputProcessor : public IInputProcessor
{
public:
    explicit
    FCkDebug_ViewportPickerInputProcessor(
        TWeakPtr<FCkDebug_ViewportPicker> InPickerWeak);

    // ---- IInputProcessor interface (engine signatures, no trailing return types) ----

    virtual void Tick(
        const float InDeltaTime,
        FSlateApplication& InSlateApp,
        TSharedRef<ICursor> InCursor) override;

    virtual bool HandleMouseMoveEvent(
        FSlateApplication& InSlateApp,
        const FPointerEvent& InMouseEvent) override;

    virtual bool HandleMouseButtonDownEvent(
        FSlateApplication& InSlateApp,
        const FPointerEvent& InMouseEvent) override;

    virtual bool HandleMouseButtonUpEvent(
        FSlateApplication& InSlateApp,
        const FPointerEvent& InMouseEvent) override;

    virtual bool HandleKeyDownEvent(
        FSlateApplication& InSlateApp,
        const FKeyEvent& InKeyEvent) override;

    virtual const TCHAR* GetDebugName() const override;

private:
    TWeakPtr<FCkDebug_ViewportPicker> _PickerWeak;
    bool                              _ConsumedDown = false;
};
