#pragma once

#include "CkJoltDebugger/Data/CkJoltDebugger_Types.h"

#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

/*
 * By CONST REFERENCE, not by value: every row's value attribute pulls the selection on every paint, and a
 * by-value delegate copied the whole snapshot — handle, two strings and a vector — once per row per frame.
 * The referent is the window's own selection member, which outlives every row it feeds.
 */
DECLARE_DELEGATE_RetVal(const TOptional<FCkJoltDebugger_BodySnapshot>&, FOnCkJoltDebugger_GetSelection);

// --------------------------------------------------------------------------------------------------------------------

/*
 * The selected body's facts, as key/value rows. Built ONCE — every value is a TAttribute lambda pulling the
 * window's current selection through GetSelection, so a live selection change (or a live velocity) lands
 * without a Slate rebuild. Rows that do not apply to the selected population read "--" rather than
 * disappearing, so the panel's height never jumps as the selection moves between populations.
 */
class SCkJoltDebugger_DetailPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkJoltDebugger_DetailPanel) {}
        SLATE_EVENT(FOnCkJoltDebugger_GetSelection, GetSelection)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    /** What the named row renders right now — the row's own bound value, evaluated. Empty for an unknown key. */
    auto Get_RowValueText(const FString& InKey) const -> FText;

private:
    auto Get_Selection() const -> const TOptional<FCkJoltDebugger_BodySnapshot>&;

    auto MakeRow(
        const FString& InKey,
        TAttribute<FText> InValue) -> TSharedRef<SWidget>;

    auto MakeEntityRow() -> TSharedRef<SWidget>;

    FOnCkJoltDebugger_GetSelection _GetSelection;

    // The bound value of every key/value row, by key: one lookup surface for the row values, so what a caller
    // reads back is the same attribute the widget paints rather than a re-derivation of it.
    TMap<FString, TAttribute<FText>> _RowValues;
};

// --------------------------------------------------------------------------------------------------------------------
