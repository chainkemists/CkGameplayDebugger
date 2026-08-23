#pragma once

#include "CoreMinimal.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FCkDebug_ViewportComponentPicker;

/** Shared toolbar surface for the generic component picker. */
class CKDEBUGGERCOMMON_API SCkDebug_ViewportComponentPickerControls : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_ViewportComponentPickerControls)
        {}
        SLATE_ARGUMENT(TSharedPtr<FCkDebug_ViewportComponentPicker>, Picker)
        SLATE_ARGUMENT(FText, PickTooltip)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

private:
    TSharedPtr<FCkDebug_ViewportComponentPicker> _Picker;
    FText _PickTooltip;
};
