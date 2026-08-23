#pragma once

#include "CoreMinimal.h"

#include "Framework/Application/IInputProcessor.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkDebug_ViewportComponentPicker;

// --------------------------------------------------------------------------------------------------------------------

/** Passive Slate pre-input bridge for the runtime component picker. */
class CKDEBUGGERCOMMON_API FCkDebug_ViewportComponentPickerInputProcessor : public IInputProcessor
{
public:
    explicit
    FCkDebug_ViewportComponentPickerInputProcessor(
        TWeakPtr<FCkDebug_ViewportComponentPicker> InPickerWeak);

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
    TWeakPtr<FCkDebug_ViewportComponentPicker> _PickerWeak;
    bool                                       _ConsumedDown = false;
};
