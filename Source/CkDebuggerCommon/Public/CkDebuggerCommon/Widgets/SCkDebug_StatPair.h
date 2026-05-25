#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateColor.h"

class SCkDebug_SelectableLabel;

// ====================================================================================================================
// A bold value paired with a small uppercase label — a "stat" tile element.
//
// Use for compact metric displays in toolbars / dashboard tiles / inspector
// rails. Examples: "$999  cost", "Cost  $999", or stacked card "1234 / ITERS".
//
// Three layouts are supported via the Layout argument:
//
//   Inline_ValueFirst  (default — call-out then context)
//       [bold value]   [UPPERCASE LABEL]
//       e.g.  "$999  COST"   for GOAP cost / attempts / children stats.
//
//   Inline_LabelFirst  (table-row style: label, then aligned value)
//       [label]   [bold value]
//       e.g.  "Cost  $999"   for AStar/Scheduler stat rows.
//
//   Stacked_ValueOnTop (centered card; big value above small uppercase label)
//                                ┌─────────┐
//                                │  1234   │
//                                │  ITERS  │
//                                └─────────┘
//       e.g. AStar mini-stat 2x2 grid (ITERATIONS / OPEN / CLOSED / PATH).
//
// Both Value and Label are TAttribute-bound so live data can drive them
// without rebuilding the widget. For panels that prefer imperative updates,
// keep a TSharedPtr<SCkDebug_StatPair> handle and call SetValue / SetLabel.
//
// ValueColor defaults to the project palette's "primary text" tone — override
// per-stat (e.g. amber for "cost", red for "errors") when the visual call-out
// matters.
// ====================================================================================================================

enum class ECkDebug_StatPairLayout : uint8
{
    Inline_ValueFirst,
    Inline_LabelFirst,
    Stacked_ValueOnTop,
};

class CKDEBUGGERCOMMON_API SCkDebug_StatPair : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_StatPair)
        : _Value(FText::GetEmpty())
        , _Label(FText::GetEmpty())
        , _Layout(ECkDebug_StatPairLayout::Inline_ValueFirst)
        , _ValueFontSize(0)   // 0 = layout default
        , _LabelFontSize(0)   // 0 = layout default
        {}
        SLATE_ATTRIBUTE(FText, Value)
        SLATE_ATTRIBUTE(FText, Label)
        SLATE_ATTRIBUTE(FSlateColor, ValueColor)
        SLATE_ARGUMENT(ECkDebug_StatPairLayout, Layout)
        SLATE_ARGUMENT(int32, ValueFontSize)
        SLATE_ARGUMENT(int32, LabelFontSize)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // Imperative setters — for panels that update their stats from a Tick
    // refresh handler rather than via TAttribute binding. Pokes the inner
    // SelectableLabel widgets directly.
    auto SetValue(const FText& InText) -> void;
    auto SetLabel(const FText& InText) -> void;
    auto SetValueColor(const FSlateColor& InColor) -> void;

private:
    TSharedPtr<SCkDebug_SelectableLabel> _ValueText;
    TSharedPtr<SCkDebug_SelectableLabel> _LabelText;
};

// ====================================================================================================================
