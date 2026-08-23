#pragma once

#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

/** Common list surface for every registered session-only behavior override. */
class CKDEBUGGERCOMMON_API SCkDebug_BehaviorOverridePanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_BehaviorOverridePanel)
    {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkDebug_BehaviorOverridePanel() override;

private:
    auto RebuildRows() -> void;

    TSharedPtr<class SVerticalBox> _Rows;
    FDelegateHandle _RegistryChangedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
