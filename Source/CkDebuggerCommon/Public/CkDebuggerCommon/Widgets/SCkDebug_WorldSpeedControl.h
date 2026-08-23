#pragma once

#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

/** Common authority-world speed control shown by every standalone CK debugger. */
class CKDEBUGGERCOMMON_API SCkDebug_WorldSpeedControl : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_WorldSpeedControl)
    {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

private:
    auto Get_Value() const -> float;
    auto Get_Tooltip() const -> FText;
    auto Get_IsEnabled() const -> bool;
    auto HandleValueChanged(float InMultiplier) -> void;
};

// --------------------------------------------------------------------------------------------------------------------
