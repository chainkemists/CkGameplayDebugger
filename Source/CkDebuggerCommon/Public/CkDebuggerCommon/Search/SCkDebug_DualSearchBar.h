#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_OneParam(FCkDebug_OnDualSearchTextChanged, const FString&);

// --------------------------------------------------------------------------------------------------------------------
// Side-by-side pair of search inputs giving the user two independent text
// queries:
//   - "Filter"    → narrows the visible entries (hide non-matches)
//   - "Highlight" → among the entries that survive the filter, dim non-matches
//                   so matches stand out without losing surrounding context
//
// Either can be empty. The two are composable: type into Filter to narrow the
// list, then type into Highlight to spot specific items inside the narrowed set.
//
// Consumers wire two delegates and run their filter pipeline twice — once for
// hide-visibility (filter string), once for the dim flag (highlight string).
// See CkDebuggerCommon/CLAUDE.md for the canonical pipeline pattern.
// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API SCkDebug_DualSearchBar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_DualSearchBar)
        : _FilterHintText(FText::FromString(TEXT("Filter…")))
        , _HighlightHintText(FText::FromString(TEXT("Highlight…")))
        {}
        SLATE_EVENT(FCkDebug_OnDualSearchTextChanged, OnFilterTextChanged)
        SLATE_EVENT(FCkDebug_OnDualSearchTextChanged, OnHighlightTextChanged)
        SLATE_ARGUMENT(FText, FilterHintText)
        SLATE_ARGUMENT(FText, HighlightHintText)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    auto Get_FilterText() const -> FString;
    auto Get_HighlightText() const -> FString;

    /** Programmatic filter (e.g. Overview card click-through) — fires OnFilterTextChanged. */
    auto Set_FilterText(const FString& InText) -> void;

private:
    FCkDebug_OnDualSearchTextChanged _OnFilterChanged;
    FCkDebug_OnDualSearchTextChanged _OnHighlightChanged;

    TSharedPtr<class SSearchBox> _FilterBox;
    TSharedPtr<class SSearchBox> _HighlightBox;
};
