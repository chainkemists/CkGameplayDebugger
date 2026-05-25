#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateColor.h"

// ====================================================================================================================
// A bold value paired with a small uppercase label — a "stat" tile element.
//
// Use for compact metric displays in toolbars / dashboard tiles / inspector
// rails. Examples: "$999  cost", "1  attempt", "12.4 ms  budget", "3  children".
//
// Layout:
//
//     [bold value]   [UPPERCASE LABEL]
//
// Both attributes are TAttribute-bound so live data can drive them without
// rebuilding the widget. ValueColor defaults to the project palette's
// "primary text" tone — override per-stat (e.g. amber for "cost", red for
// "errors") when the visual call-out matters.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_StatPair : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_StatPair)
        : _Value(FText::GetEmpty())
        , _Label(FText::GetEmpty())
        , _ValueFontSize(14)
        , _LabelFontSize(8)
        {}
        SLATE_ATTRIBUTE(FText, Value)
        SLATE_ATTRIBUTE(FText, Label)
        SLATE_ATTRIBUTE(FSlateColor, ValueColor)
        SLATE_ARGUMENT(int32, ValueFontSize)
        SLATE_ARGUMENT(int32, LabelFontSize)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
};

// ====================================================================================================================
