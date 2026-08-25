#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SBorder;
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

    // The bound entity is an attribute the host re-evaluates as its selection moves. Nothing
    // broadcasts that change, so the button row is rebuilt here when the resolved entity differs
    // from the one the current row was built for.
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
    auto Rebuild() -> void;
    auto OnOpenInClicked(FName InTabId) -> FReply;

    TAttribute<FCk_Handle> _Entity;
    FCk_Handle _BuiltForEntity;
    FName _ExcludeTabId;
    // Self stays Visible so Slate keeps ticking this widget when it has nothing to show;
    // an arranged-out (Collapsed) widget is never ticked, and the entity attribute would
    // then never be re-read. The empty state collapses the ROOT instead, which still
    // yields a zero desired size through SCompoundWidget::ComputeDesiredSize.
    TSharedPtr<SBorder> _Root;
    TSharedPtr<SHorizontalBox> _Links;
    FDelegateHandle _RouteChangedHandle;
    FDelegateHandle _ToolChangedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
