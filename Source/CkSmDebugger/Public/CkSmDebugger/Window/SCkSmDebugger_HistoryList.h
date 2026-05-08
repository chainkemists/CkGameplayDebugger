#pragma once

#include "CkSmDebugger/ViewModel/CkSmDebugger_ViewModel.h"
#include "CkSmDebugger/Graph/CkSmDebugGraph.h"

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

    auto Construct(const FArguments& InArgs, TSharedPtr<FCkSmDebugger_ViewModel> InViewModel, UCkSmDebugGraph* InGraph = nullptr) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
    using FHistoryItemPtr = TSharedPtr<FCkSmDebugger_HistoryEntry>;

    auto GenerateRow(FHistoryItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;
    auto BuildTaskChips(const TArray<FCkSmDebugger_HistoryTaskSnapshot>& InSnapshots, bool InShortNames) -> TSharedRef<SWidget>;
    auto OnSelectionChanged(FHistoryItemPtr InItem, ESelectInfo::Type InSelectInfo) -> void;
    auto RebuildList() -> void;

private:
    TSharedPtr<FCkSmDebugger_ViewModel> _ViewModel;
    UCkSmDebugGraph* _Graph = nullptr;
    TSharedPtr<SListView<FHistoryItemPtr>> _ListView;
    TArray<FHistoryItemPtr> _Items;
    FDelegateHandle _SmDataRefreshedHandle;
    FDelegateHandle _SmListChangedHandle;
    int32 _LastHistoryCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------
