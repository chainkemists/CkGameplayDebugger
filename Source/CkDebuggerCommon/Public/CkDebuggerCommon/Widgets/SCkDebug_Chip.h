#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Condition / effect chip — the debugger's atom for "(key, required value)"
// facts: preconditions, effects, goal conditions, world-state references.
//
//   Satisfied    green dot + green tint      condition currently met
//   Unsatisfied  red dot + red tint          condition currently violated
//   Effect       accent "→" glyph            an effect ("sets this key")
//   Neutral      dim dot + inset tint        non-semantic fact ("no preconditions")
//
// Kind is attribute-bound so live world-state flips recolor the chip without
// a rebuild. `Highlighted` draws a tight accent glow ring — the cross-panel
// key-trace affordance.
//
// The box treatment follows the ChipStyle axis (Tint / Solid / Outline /
// TextOnly) through attribute lambdas, so a Style Lab flip moves chips that
// were built long before it. Tint is the default and is this widget's shipped
// look untouched.
//
// Click-trap warning: when OnClicked is bound the chip consumes left-clicks —
// do NOT place clickable chips inside SListView/STreeView rows (see
// CkDebuggerCommon/CLAUDE.md §"List / tree rows"). Unbound chips are inert
// and row-safe.
// ====================================================================================================================

enum class ECkDebug_ChipKind : uint8
{
    Neutral,
    Satisfied,
    Unsatisfied,
    Effect,
};

DECLARE_DELEGATE(FOnCkDebug_ChipClicked);

class CKDEBUGGERCOMMON_API SCkDebug_Chip : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_Chip)
        : _Text(FText::GetEmpty())
        , _Kind(ECkDebug_ChipKind::Neutral)
        , _Highlighted(false)
        , _ShowDot(true)
    {}
        SLATE_ATTRIBUTE(FText, Text)
        SLATE_ATTRIBUTE(ECkDebug_ChipKind, Kind)
        // true → tight accent glow ring (key-trace highlight).
        SLATE_ATTRIBUTE(bool, Highlighted)
        SLATE_ARGUMENT(bool, ShowDot)
        // Right-click → Copy payload. Empty = no copy menu.
        SLATE_ARGUMENT(FString, CopyText)
        // Bound → chip is clickable (consumes left-click; not row-safe).
        SLATE_EVENT(FOnCkDebug_ChipClicked, OnClicked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    virtual auto OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;

private:
    TAttribute<ECkDebug_ChipKind> _Kind;
    FOnCkDebug_ChipClicked _OnClicked;
    FString _CopyText;
};

// ====================================================================================================================
