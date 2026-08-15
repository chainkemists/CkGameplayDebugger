#pragma once

#include "CkJoltDebugger/Data/CkJoltDebugger_Types.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SCkDebug_DualSearchBar;

// --------------------------------------------------------------------------------------------------------------------

DECLARE_DELEGATE_OneParam(FOnCkJoltDebugger_RowSelected, TOptional<FCkJoltDebugger_BodySnapshot>);

// --------------------------------------------------------------------------------------------------------------------

/*
 * The outliner: every body-backing entity of the selected world, one row each.
 *
 * Row items keep TSharedPtr identity across refreshes (SListView keys selection by pointer), so Refresh
 * mutates the existing snapshot in place and only asks for a list refresh when the visible SET changes.
 * OnRowSelected fires for USER-driven selection only — the external selectors apply with
 * ESelectInfo::Direct, which the selection handler ignores, so a programmatic apply never echoes back.
 */
class SCkJoltDebugger_OutlinerPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkJoltDebugger_OutlinerPanel) {}
        SLATE_EVENT(FOnCkJoltDebugger_RowSelected, OnRowSelected)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    /** Reconcile the visible rows against a fresh collector pass. */
    auto Refresh(const TArray<FCkJoltDebugger_BodySnapshot>& InBodies) -> void;

    /** Drop every row and its handle. Called from the window's single session-invalidated path. */
    auto Clear() -> void;

    /*
     * Both selectors search the UNFILTERED set and reveal the match: a row the user's filter currently hides is
     * still a row this window owns, and an external selector that silently did nothing would read as a broken
     * "Open In". Revealing clears the FILTER query only — the highlight query dims rows, it never hides them.
     */
    auto SelectByHandle(const FCk_Handle& InHandle) -> TOptional<FCkJoltDebugger_BodySnapshot>;
    auto SelectByEntity(FCk_Entity InEntity) -> TOptional<FCkJoltDebugger_BodySnapshot>;
    auto ClearSelection() -> void;

    /** The row the view has selected, if any. */
    auto Get_Selection() const -> TOptional<FCkJoltDebugger_BodySnapshot>;

    /*
     * Rows surviving the filter, PLUS the selected row when the filter would otherwise hide it — a selection
     * the user cannot see is indistinguishable from no selection, so the selected row is pinned and dimmed
     * rather than dropped.
     */
    auto Get_NumVisibleRows() const -> int32;

    /** Whether a listed row renders muted: it lost the highlight query, or it is the pinned selection. */
    auto Get_IsRowDimmed(const FCkJoltDebugger_BodySnapshot& InBody) const -> bool;

    /** Drive the filter query directly — the same path a keystroke in the search box takes. */
    auto Set_FilterQuery(const FString& InQuery) -> void;

    /** Regenerate every row widget after a style revision. Item identity is untouched. */
    auto Rebuild_ForStyleChange() -> void;

private:
    using ItemPtr = TSharedPtr<FCkJoltDebugger_BodySnapshot>;

    auto OnGenerateRow(ItemPtr InItem, const TSharedRef<STableViewBase>& InTable) -> TSharedRef<ITableRow>;
    auto OnSelectionChanged(ItemPtr InItem, ESelectInfo::Type InSelectInfo) -> void;
    auto OnContextMenuOpening() -> TSharedPtr<SWidget>;

    auto ApplyFilterPipeline() -> void;
    auto DoSelectItem(ItemPtr InItem) -> TOptional<FCkJoltDebugger_BodySnapshot>;
    auto DoSelectMatching(TFunctionRef<bool(const FCkJoltDebugger_BodySnapshot&)> InPredicate)
        -> TOptional<FCkJoltDebugger_BodySnapshot>;
    auto TryFind_Item(const FCkJoltDebugger_BodySnapshot& InBody) const -> ItemPtr;

    TArray<FCkJoltDebugger_BodySnapshot> _Bodies;
    TArray<ItemPtr>                      _ItemSource;

    TSharedPtr<SListView<ItemPtr>>  _ListView;
    TSharedPtr<SCkDebug_DualSearchBar> _SearchBar;

    FOnCkJoltDebugger_RowSelected _OnRowSelected;

    // Filter hides rows; Highlight dims the ones that survive the filter.
    FString _FilterString;
    FString _HighlightString;
};

// --------------------------------------------------------------------------------------------------------------------
