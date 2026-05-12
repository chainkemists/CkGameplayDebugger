#pragma once

#include "CkEqsDebugger/Data/CkEqsDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FCkEqsDebugger_ViewModel;
template<typename T> class SListView;
class ITableRow;
class STableViewBase;
class SCkDebug_SelectableLabel;

// --------------------------------------------------------------------------------------------------------------------
// Candidate panel — one row per candidate in the currently selected query (sorted by score, post-Finalize order).
// Selecting a row drives the test-breakdown panel. Right-click for "Copy ..." menu via CkDebuggerCommon utils.
// --------------------------------------------------------------------------------------------------------------------

class SCkEqsDebugger_CandidatePanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkEqsDebugger_CandidatePanel) {}
        SLATE_ARGUMENT(TSharedPtr<FCkEqsDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkEqsDebugger_CandidatePanel();

private:
    auto OnQueryListChanged(const TArray<FCkEqsDebugger_QueryInfo>& InQueries) -> void;
    auto OnSelectedQueryChanged(FCk_Handle_EqsQuery InHandle) -> void;
    auto Refresh_FromCurrentQuery() -> void;

    auto OnGenerateRow(TSharedPtr<FCkEqsDebugger_CandidateInfo> InItem, const TSharedRef<STableViewBase>& InOwner) -> TSharedRef<ITableRow>;
    auto OnSelectionChanged(TSharedPtr<FCkEqsDebugger_CandidateInfo> InItem, ESelectInfo::Type InSelectInfo) -> void;
    auto OnContextMenuOpening() -> TSharedPtr<SWidget>;

    auto BuildCopyTextForItem(const FCkEqsDebugger_CandidateInfo& InItem) const -> FString;

    TSharedPtr<FCkEqsDebugger_ViewModel> _ViewModel;

    TArray<TSharedPtr<FCkEqsDebugger_CandidateInfo>> _Items;
    TSharedPtr<SListView<TSharedPtr<FCkEqsDebugger_CandidateInfo>>> _ListView;

    TSharedPtr<SCkDebug_SelectableLabel> _HeaderLabel;

    FDelegateHandle _OnQueryListChangedHandle;
    FDelegateHandle _OnSelectedQueryChangedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
