#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API SCkDebug_UseEcsSelection : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_UseEcsSelection) {}
        SLATE_ARGUMENT(FName, TargetTabId)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

private:
    auto Get_Label() const -> FText;
    auto Get_Tooltip() const -> FText;
    auto Get_IsEnabled() const -> bool;
    auto OnClicked() -> FReply;

    FName _TargetTabId;
};

// --------------------------------------------------------------------------------------------------------------------
