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

    virtual ~SCkDebug_WindowChrome() override;

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    /** The retained authored frame; exposed for focused consumer verification only. */
    TSharedPtr<class FCkUiView> Get_AuthoredFrame() const { return _AuthoredFrame; }

private:
    auto ActivateAuthoredFrame(const TSharedRef<SWidget>& InCommandBar, const TSharedRef<SWidget>& InContent) -> bool;
    auto ActivateNativeFallback(const TSharedRef<SWidget>& InCommandBar, const TSharedRef<SWidget>& InContent) -> void;
    auto OnOpenLauncher() const -> FReply;
    auto Get_DefaultStatusText() const -> FText;

    FName _WindowId;
    FName _ToolTabId;
    TAttribute<FText> _StatusText;
    TSharedPtr<class FCkUiView> _AuthoredFrame;
};

// --------------------------------------------------------------------------------------------------------------------
