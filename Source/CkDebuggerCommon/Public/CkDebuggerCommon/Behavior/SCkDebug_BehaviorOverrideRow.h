#pragma once

#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

/** Common presentation for one registered, session-only behavior suppression. */
class CKDEBUGGERCOMMON_API SCkDebug_BehaviorOverrideRow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_BehaviorOverrideRow)
        : _OverrideId(NAME_None)
    {}
        SLATE_ARGUMENT(FName, OverrideId)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

private:
    auto Get_Label() const -> FText;
    auto Get_Description() const -> FText;
    auto Get_Tooltip() const -> FText;
    auto Get_IsAvailable() const -> bool;
    auto Get_IsActive() const -> bool;
    auto HandleStateChanged(bool InShouldActivate) -> void;

    FName _OverrideId;
    FText _LastFailure;
};

// --------------------------------------------------------------------------------------------------------------------
