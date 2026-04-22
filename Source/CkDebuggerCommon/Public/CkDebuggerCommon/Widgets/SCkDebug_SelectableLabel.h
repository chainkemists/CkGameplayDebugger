#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"

class SEditableText;

// --------------------------------------------------------------------------------------------------------------------
// Drop-in replacement for STextBlock that lets the user select and copy the text.
// Built on a read-only SEditableText, so users get drag-select, Ctrl+C, and a
// built-in right-click → Copy / Select All context menu for free.
//
// Limitations vs STextBlock:
//   - Single line only (no AutoWrapText, no TextOverflowPolicy).
//   - No TransformPolicy (e.g. ToUpper). For uppercase headers, keep STextBlock.
//
// Use this anywhere the displayed text carries information the user would
// reasonably want to copy (names, IDs, paths, expressions, numbers).
// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API SCkDebug_SelectableLabel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_SelectableLabel)
        : _Text(FText::GetEmpty())
        , _ColorAndOpacity(FSlateColor::UseForeground())
        {}
        SLATE_ATTRIBUTE(FText, Text)
        SLATE_ATTRIBUTE(FSlateFontInfo, Font)
        SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // Imperative setter for callers that update the displayed text after
    // construction (e.g. live stat-panel values). Mirrors STextBlock::SetText
    // so swapping at the call site is a one-token rename.
    auto SetText(const FText& InText) -> void;

private:
    TSharedPtr<SEditableText> _Editable;
};
