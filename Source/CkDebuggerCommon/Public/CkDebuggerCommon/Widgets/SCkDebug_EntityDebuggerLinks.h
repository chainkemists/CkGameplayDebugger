#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SHorizontalBox;

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API SCkDebug_EntityDebuggerLinks : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_EntityDebuggerLinks)
        : _Entity(FCk_Handle{})
    {}
        SLATE_ATTRIBUTE(FCk_Handle, Entity)
        SLATE_ARGUMENT(FName, ExcludeTabId)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkDebug_EntityDebuggerLinks() override;

private:
    auto Rebuild() -> void;
    auto OnOpenInClicked(FName InTabId) -> FReply;

    TAttribute<FCk_Handle> _Entity;
    FName _ExcludeTabId;
    TSharedPtr<SHorizontalBox> _Links;
    FDelegateHandle _RouteChangedHandle;
    FDelegateHandle _ToolChangedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
