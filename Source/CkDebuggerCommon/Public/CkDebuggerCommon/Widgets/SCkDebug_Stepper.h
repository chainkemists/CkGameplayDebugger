#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Compact −/+ stepper pair — the decision/catalog cost editor. The widget is
// buttons-only; render the value itself next to it (mono text, StatPair, …).
// Not row-safe inside SListView (buttons consume clicks).
// ====================================================================================================================

DECLARE_DELEGATE_OneParam(FOnCkDebug_StepperDelta, int32 /* +1 or -1 */);

class CKDEBUGGERCOMMON_API SCkDebug_Stepper : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_Stepper)
        : _ButtonSize(17.0f)
    {}
        SLATE_ARGUMENT(float, ButtonSize)
        SLATE_EVENT(FOnCkDebug_StepperDelta, OnDelta)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

private:
    auto DoMakeButton(const FText& InGlyph, int32 InDelta, float InSize) -> TSharedRef<SWidget>;

    FOnCkDebug_StepperDelta _OnDelta;
};

// ====================================================================================================================
