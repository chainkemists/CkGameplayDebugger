#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

DECLARE_DELEGATE_OneParam(FOnCkDebug_ToggleSurfaceChanged, bool /* NewState */);

/**
 * Common content-bearing toggle surface for contextual filters, cards, and
 * dynamic feature chips that cannot be represented by a fixed icon id.
 *
 * The feature owns state and content; Common owns selection, hover, keyboard,
 * accessibility, and tooltip presentation. Use SCkDebug_IconToggle when a
 * fixed suite icon communicates the option more clearly.
 */
class CKDEBUGGERCOMMON_API SCkDebug_ToggleSurface : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_ToggleSurface)
        : _IsOn(false)
        , _IsEnabled(true)
        , _AccessibleText(FText::GetEmpty())
        , _ToolTipText(FText::GetEmpty())
    {}
        SLATE_ATTRIBUTE(bool, IsOn)
        SLATE_EVENT(FOnCkDebug_ToggleSurfaceChanged, OnStateChanged)
        SLATE_ATTRIBUTE(bool, IsEnabled)
        SLATE_ATTRIBUTE(FText, AccessibleText)
        SLATE_ATTRIBUTE(FText, ToolTipText)
        SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
};

// ====================================================================================================================
