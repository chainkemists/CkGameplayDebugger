#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// TRUE / FALSE value pill — the world-state panel's value display.
//
//   TRUE   ok tone (green dim bg, green mono text)
//   FALSE  neutral inset (dim bg, muted mono text — false is a value, not an error)
//
// `Editable` (attribute) turns on the affordance: hand cursor + accent border
// on hover + OnToggled fires with the flipped value on click. The debugger
// binds Editable to its Sandbox switch so pills become interactive only in
// sandbox mode.
//
// Click-trap: an editable pill consumes left-clicks — not row-safe inside
// SListView (see CkDebuggerCommon/CLAUDE.md §"List / tree rows").
// ====================================================================================================================

DECLARE_DELEGATE_OneParam(FOnCkDebug_ValuePillToggled, bool /* NewValue */);

class CKDEBUGGERCOMMON_API SCkDebug_ValuePill : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_ValuePill)
        : _Value(false)
        , _Editable(false)
        , _MinWidth(46.0f)
        , _TrueText(NSLOCTEXT("CkDebug", "ValuePill_True", "TRUE"))
        , _FalseText(NSLOCTEXT("CkDebug", "ValuePill_False", "FALSE"))
    {}
        SLATE_ATTRIBUTE(bool, Value)
        SLATE_ATTRIBUTE(bool, Editable)
        SLATE_ARGUMENT(float, MinWidth)
        SLATE_ARGUMENT(FText, TrueText)
        SLATE_ARGUMENT(FText, FalseText)
        SLATE_EVENT(FOnCkDebug_ValuePillToggled, OnToggled)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    virtual auto OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;

private:
    TAttribute<bool> _Value;
    TAttribute<bool> _Editable;
    FOnCkDebug_ValuePillToggled _OnToggled;
};

// ====================================================================================================================
