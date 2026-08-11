#pragma once

#include "CkSmDebugger/ViewModel/CkSmDebugger_ViewModel.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

// --------------------------------------------------------------------------------------------------------------------
// History list — shows transition history entries for the selected run.
// Clicking an entry scrubs the timeline to that transition.
// --------------------------------------------------------------------------------------------------------------------

DECLARE_DELEGATE_OneParam(FOnHistoryEntrySelected, TSharedPtr<FCkSmDebugger_HistoryEntry>);

class SCkSmDebugger_HistoryList : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkSmDebugger_HistoryList) {}
    SLATE_END_ARGS()

    FOnHistoryEntrySelected OnEntrySelected;

    auto Construct(const FArguments& InArgs, TSharedPtr<FCkSmDebugger_ViewModel> InViewModel, TAttribute<int32> InNameDepth = 1) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    /**
     * Drop the rebuild debounce and re-emit every row. The window calls this on a style-revision
     * bump: rows are attribute-bound for colour / font / padding, but a row's STRUCTURE (which
     * slots exist) is composed once at generation, so a structural axis needs the re-emit.
     */
    auto Invalidate_StyleCache() -> void;

private:
    using FHistoryItemPtr = TSharedPtr<FCkSmDebugger_HistoryEntry>;

    auto GenerateRow(FHistoryItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;
    auto BuildTaskChips(const TArray<FCkSmDebugger_HistoryTaskSnapshot>& InSnapshots, bool InShortNames) -> TSharedRef<SWidget>;
    auto OnSelectionChanged(FHistoryItemPtr InItem, ESelectInfo::Type InSelectInfo) -> void;
    auto RebuildList() -> void;

private:
    TSharedPtr<FCkSmDebugger_ViewModel> _ViewModel;
    TAttribute<int32> _NameDepth = 1;
    TSharedPtr<SListView<FHistoryItemPtr>> _ListView;
    TArray<FHistoryItemPtr> _Items;
    FDelegateHandle _SmDataRefreshedHandle;
    FDelegateHandle _SmListChangedHandle;
    int32 _LastHistoryCount = 0;
    int32 _LastScrubScrollIdx = -1;

    // Dual-search state. Filter narrows _Items; Highlight only dims rows that
    // don't match (no rebuild — applied via per-row tint lambda).
    FString _FilterString;
    FString _HighlightString;

    auto MatchesFilter(const FCkSmDebugger_HistoryEntry& InEntry, const FString& InText) const -> bool;
};

// --------------------------------------------------------------------------------------------------------------------
