#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

/**
 * Shared frame for standalone CK debugger tabs.
 *
 * The frame owns only debugger-wide chrome: a stable title and debugger switcher,
 * an optional tool-specific toolbar, the tool body, and a persistent status strip.
 * Feature modules continue to own all tool-specific state and controls.
 */
class CKDEBUGGERCOMMON_API SCkDebug_WindowChrome : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_WindowChrome)
        : _WindowId(NAME_None)
        , _ToolTabId(NAME_None)
        , _DisplayName(FText::GetEmpty())
        , _StatusText(FText::GetEmpty())
    {}
        SLATE_ARGUMENT(FName, WindowId)
        SLATE_ARGUMENT(FName, ToolTabId)
        SLATE_ATTRIBUTE(FText, DisplayName)
        SLATE_ATTRIBUTE(FText, StatusText)
        SLATE_NAMED_SLOT(FArguments, ToolbarContent)
        SLATE_NAMED_SLOT(FArguments, Content)
        SLATE_NAMED_SLOT(FArguments, StatusContent)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

private:
    auto Build_DebuggerMenu() const -> TSharedRef<SWidget>;
    auto OnOpenDebugger(FName InTabId) const -> FReply;
    auto Get_DefaultStatusText() const -> FText;

    FName _WindowId;
    FName _ToolTabId;
    TAttribute<FText> _DisplayName;
    TAttribute<FText> _StatusText;
};

// --------------------------------------------------------------------------------------------------------------------
