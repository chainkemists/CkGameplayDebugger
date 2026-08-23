#pragma once

#include "CoreMinimal.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CommandBar.h"
#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

/**
 * Shared frame for standalone CK debugger tabs.
 *
 * The frame owns only debugger-wide chrome: semantic command groups, a trailing
 * status/refresh/tool cluster, and the tool body. The tab owns debugger identity.
 * Feature modules continue to own all tool-specific state and controls.
 */
class CKDEBUGGERCOMMON_API SCkDebug_WindowChrome : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_WindowChrome)
        : _WindowId(NAME_None)
        , _ToolTabId(NAME_None)
        , _StatusText(FText::GetEmpty())
        , _ShowRefreshControls(false)
    {}
        SLATE_ARGUMENT(FName, WindowId)
        SLATE_ARGUMENT(FName, ToolTabId)
        SLATE_ATTRIBUTE(FText, StatusText)
        SLATE_ARGUMENT(TArray<FCkDebug_CommandGroup>, CommandGroups)
        SLATE_ARGUMENT(bool, ShowRefreshControls)
        SLATE_NAMED_SLOT(FArguments, CommonActionsContent)
        SLATE_NAMED_SLOT(FArguments, MenuActionsContent)
        SLATE_NAMED_SLOT(FArguments, ToolbarContent)
        SLATE_NAMED_SLOT(FArguments, Content)
        SLATE_NAMED_SLOT(FArguments, StatusContent)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

private:
    auto OnOpenLauncher() const -> FReply;
    auto Get_DefaultStatusText() const -> FText;

    FName _WindowId;
    FName _ToolTabId;
    TAttribute<FText> _StatusText;
};

// --------------------------------------------------------------------------------------------------------------------
