#pragma once

#include "CkEditorTools/Style/CkIcons_Generated.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

/**
 * Canonical icon-only debugger action.
 *
 * The typed icon and explanation stay together, so compact common actions do
 * not reintroduce bare, unlabelled glyph buttons.
 */
class CKDEBUGGERCOMMON_API SCkDebug_IconButton : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_IconButton)
        : _IconId(ECk_Icon::None)
        , _IsEnabled(true)
    {}
        SLATE_ARGUMENT(ECk_Icon, IconId)
        SLATE_ATTRIBUTE(FText, Label)
        SLATE_EVENT(FOnClicked, OnClicked)
        SLATE_ATTRIBUTE(bool, IsEnabled)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
};

// --------------------------------------------------------------------------------------------------------------------
