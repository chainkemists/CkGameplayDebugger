#pragma once

#include "CkEqsDebugger/Data/CkEqsDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FCkEqsDebugger_ViewModel;
template<typename T> class SListView;
class ITableRow;
class STableViewBase;
class SCkDebug_DualSearchBar;
class FMenuBuilder;

// --------------------------------------------------------------------------------------------------------------------
// Query list panel — one row per in-flight / completed query in the current PIE world.
// Selecting a row calls ViewModel::Set_SelectedQueryHandle.
//
// Includes a CkDebuggerCommon dual search bar:
//   - Filter   → narrows visible rows (hide non-matches)
//   - Highlight → among visible rows, dim non-matches so matches stand out
// --------------------------------------------------------------------------------------------------------------------

class SCkEqsDebugger_QueryList : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkEqsDebugger_QueryList) {}
        SLATE_ARGUMENT(TSharedPtr<FCkEqsDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkEqsDebugger_QueryList();

private:
    // Per-row item bundles the snapshot info with the panel's transient match flags. Match flags are set during
    // ApplyFilterPipeline; rebuilding _Items from the source query list also re-runs the pipeline.
    struct FRowItem
    {
        FCkEqsDebugger_QueryInfo Info;
        bool                     IsHighlightMatch = true;
    };

    auto OnQueryListChanged(const TArray<FCkEqsDebugger_QueryInfo>& InQueries) -> void;
    auto ApplyFilterPipeline(const TArray<FCkEqsDebugger_QueryInfo>& InQueries) -> void;

    auto OnGenerateRow(TSharedPtr<FRowItem> InItem, const TSharedRef<STableViewBase>& InOwner) -> TSharedRef<ITableRow>;
    auto OnSelectionChanged(TSharedPtr<FRowItem> InItem, ESelectInfo::Type InSelectInfo) -> void;
    auto OnContextMenuOpening() -> TSharedPtr<SWidget>;

    auto BuildCopyTextForItem(const FCkEqsDebugger_QueryInfo& InInfo) const -> FString;

    TSharedPtr<FCkEqsDebugger_ViewModel> _ViewModel;

    TArray<TSharedPtr<FRowItem>> _Items;
    TSharedPtr<SListView<TSharedPtr<FRowItem>>> _ListView;
    TSharedPtr<SCkDebug_DualSearchBar> _SearchBar;

    // Cached source list (so ApplyFilterPipeline can re-run when only the highlight string changes without us
    // having to wait for the next OnQueryListChanged broadcast).
    TArray<FCkEqsDebugger_QueryInfo> _LastQueries;

    FString _FilterString;
    FString _HighlightString;

    FDelegateHandle _OnQueryListChangedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
