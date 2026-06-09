#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Tiny single-line label placed at a world-projected screen position.
//
// The text is set imperatively via Set_Text(). Hit-test invisible.
// Positioned by the parent SCkDebugOverlay_Root via SConstraintCanvas.
// ====================================================================================================================

class CKENTITYDEBUGOVERLAY_API SCkDebugOverlay_WorldTag : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebugOverlay_WorldTag)
        : _Text(FText::GetEmpty())
    {}
        SLATE_ARGUMENT(FText, Text)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // Updates the displayed text. Safe to call every tick.
    auto Set_Text(const FText& InText) -> void;

private:
    TSharedPtr<STextBlock> _TextBlock;
};

// ====================================================================================================================
