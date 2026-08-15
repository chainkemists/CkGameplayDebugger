#pragma once

#include "CkJoltDebugger/Data/CkJoltDebugger_Types.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SCkDebug_DualSearchBar;

// --------------------------------------------------------------------------------------------------------------------

namespace ck_jolt_debugger_outliner_panel
{
    /*
     * What makes two rows the same row across refreshes. An entity can back more than one population at once
     * (a JoltBody entity that also owns a Probe), so the handle alone would collapse those rows into one and
     * churn the list every pass.
     *
     * It lives in the HEADER because the panel's multi-selection is stored as identities rather than as row
     * pointers: a pointer set would silently lose members every time the filter pipeline replaced an item.
     */
    struct FRowIdentity
    {
        FCk_Handle _Handle;
        ECkJoltDebugger_Population _Population = ECkJoltDebugger_Population::JoltBody;

        auto operator==(const FRowIdentity& InOther) const -> bool
        {
            return _Handle == InOther._Handle && _Population == InOther._Population;
        }

        friend auto GetTypeHash(const FRowIdentity& InIdentity) -> uint32
        {
            return HashCombine(GetTypeHash(InIdentity._Handle), GetTypeHash(static_cast<uint8>(InIdentity._Population)));
        }
    };

    inline auto Get_RowIdentity(
        const FCkJoltDebugger_BodySnapshot& InSnapshot) -> FRowIdentity
    {
        return FRowIdentity{InSnapshot.Handle, InSnapshot.Population};
    }
}

// --------------------------------------------------------------------------------------------------------------------

/*
 * A user-driven selection change. The FIRST parameter is the PRIMARY — the row the user last acted on, the
 * one the detail panel and the facility's sample follow; the second is the whole selected set, in list order.
 */
DECLARE_DELEGATE_TwoParams(
    FOnCkJoltDebugger_RowSelected,
    TOptional<FCkJoltDebugger_BodySnapshot>,
    TArray<FCkJoltDebugger_BodySnapshot>);

// --------------------------------------------------------------------------------------------------------------------

/*
 * The outliner: every body-backing entity of the selected world, one row each.
 *
 * Row items keep TSharedPtr identity across refreshes (SListView keys selection by pointer), so Refresh
 * mutates the existing snapshot in place and only asks for a list refresh when the visible SET changes.
 * OnRowSelected fires for USER-driven selection only — the external selectors apply with
 * ESelectInfo::Direct, which the selection handler ignores, so a programmatic apply never echoes back.
 *
 * MULTI-SELECT (P7-D53). The panel is the window's selection STORE: it owns the selected identities and the
 * primary, and the view is driven from them rather than the other way round. That inversion is forced by the
 * engine — `SListView::Private_SignalSelectionChanged` hands `OnSelectionChanged` an ARBITRARY element of its
 * selection TSet (the first iterator), not the row the user just clicked, so "primary = last clicked" cannot
 * be read off the delegate parameter. It is derived from the set DELTA instead: whatever was added is the new
 * primary; if the primary was removed, the last survivor takes over.
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
     * Both REPLACE the selection; Add_ToSelection is the additive twin.
     */
    auto SelectByHandle(const FCk_Handle& InHandle) -> TOptional<FCkJoltDebugger_BodySnapshot>;
    auto SelectByEntity(FCk_Entity InEntity) -> TOptional<FCkJoltDebugger_BodySnapshot>;

    /*
     * The Ctrl+click path, from the outliner's own rows or from a Ctrl+click in the viewport: the row joins
     * the selection and becomes the PRIMARY, because it is the one the user last acted on. A row already in
     * the selection is promoted to primary rather than added twice.
     */
    auto Add_ToSelection(const FCk_Handle& InHandle) -> TOptional<FCkJoltDebugger_BodySnapshot>;

    auto ClearSelection() -> void;

    /** The PRIMARY selected row — the one the detail panel and the facility's sample follow. */
    auto Get_Selection() const -> TOptional<FCkJoltDebugger_BodySnapshot>;

    /** Every selected row, primary FIRST (the facility samples the first highlighted key). */
    auto Get_SelectedAll() const -> TArray<FCkJoltDebugger_BodySnapshot>;

    auto Get_NumSelectedRows() const -> int32;

    /*
     * Rows surviving the filter, PLUS every selected row when the filter would otherwise hide it — a selection
     * the user cannot see is indistinguishable from no selection, so selected rows are pinned and dimmed
     * rather than dropped.
     */
    auto Get_NumVisibleRows() const -> int32;

    /** Whether a listed row renders muted: it lost the highlight query, or it is a pinned selection. */
    auto Get_IsRowDimmed(const FCkJoltDebugger_BodySnapshot& InBody) const -> bool;

    /** Drive the filter query directly — the same path a keystroke in the search box takes. */
    auto Set_FilterQuery(const FString& InQuery) -> void;

    /** Regenerate every row widget after a style revision. Item identity is untouched. */
    auto Rebuild_ForStyleChange() -> void;

private:
    using ItemPtr = TSharedPtr<FCkJoltDebugger_BodySnapshot>;
    using FRowIdentity = ck_jolt_debugger_outliner_panel::FRowIdentity;

    auto OnGenerateRow(ItemPtr InItem, const TSharedRef<STableViewBase>& InTable) -> TSharedRef<ITableRow>;
    auto OnSelectionChanged(ItemPtr InItem, ESelectInfo::Type InSelectInfo) -> void;
    auto OnContextMenuOpening() -> TSharedPtr<SWidget>;

    auto ApplyFilterPipeline() -> void;
    auto DoSelectItem(ItemPtr InItem, bool InIsAdditive) -> TOptional<FCkJoltDebugger_BodySnapshot>;
    auto DoSelectMatching(TFunctionRef<bool(const FCkJoltDebugger_BodySnapshot&)> InPredicate, bool InIsAdditive)
        -> TOptional<FCkJoltDebugger_BodySnapshot>;
    auto TryFind_Item(const FCkJoltDebugger_BodySnapshot& InBody) const -> ItemPtr;
    auto TryFind_ItemByIdentity(const FRowIdentity& InIdentity) const -> ItemPtr;

    /** Push _SelectedIdentities / _Primary onto the view, without echoing back through OnSelectionChanged. */
    auto DoPushSelectionToView() -> void;

    /** The whole selected set as snapshots, primary first. Reads the collected bodies, not the visible rows. */
    auto Get_SnapshotsForSelection() const -> TArray<FCkJoltDebugger_BodySnapshot>;

    auto DoBroadcastSelection() -> void;

    TArray<FCkJoltDebugger_BodySnapshot> _Bodies;
    TArray<ItemPtr>                      _ItemSource;

    TSharedPtr<SListView<ItemPtr>>  _ListView;
    TSharedPtr<SCkDebug_DualSearchBar> _SearchBar;

    FOnCkJoltDebugger_RowSelected _OnRowSelected;

    // The selection model, owned here rather than read back off the view. In list order, minus the primary,
    // which is tracked separately because the view cannot report which row was clicked last.
    TArray<FRowIdentity>    _SelectedIdentities;
    TOptional<FRowIdentity> _Primary;

    // Raised while this panel is stamping the view. Every programmatic apply signals a selection change back
    // through OnSelectionChanged, and letting that rewrite the model from the view's half-applied state is
    // how a multi-selection loses members mid-apply.
    bool _IsApplyingSelection = false;

    // Filter hides rows; Highlight dims the ones that survive the filter.
    FString _FilterString;
    FString _HighlightString;
};

// --------------------------------------------------------------------------------------------------------------------
