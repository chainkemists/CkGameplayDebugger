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
// Per-test breakdown panel — one row per test, showing raw / normalized / weighted / passed for the currently
// selected candidate. Driven by the ViewModel's selected-candidate state. Each row uses SCkDebug_HistoryRow with
// a tone-colored marker (green = passed, red = filter-failed, accent = score-only); right-click opens a copy menu.
// --------------------------------------------------------------------------------------------------------------------

class SCkEqsDebugger_TestBreakdownPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkEqsDebugger_TestBreakdownPanel) {}
        SLATE_ARGUMENT(TSharedPtr<FCkEqsDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkEqsDebugger_TestBreakdownPanel();

    /** Regenerate every row widget after a Layer-B style revision. Data is untouched. */
    auto Rebuild_ForStyleChange() -> void;

private:
    auto OnQueryListChanged(const TArray<FCkEqsDebugger_QueryInfo>& InQueries) -> void;
    auto OnSelectedCandidateChanged(int32 InIndex) -> void;
    auto Refresh_FromCurrentCandidate() -> void;

    auto OnGenerateRow(TSharedPtr<FCkEqsDebugger_PerTestInfo> InItem, const TSharedRef<STableViewBase>& InOwner) -> TSharedRef<ITableRow>;
    auto OnContextMenuOpening() -> TSharedPtr<SWidget>;

    auto BuildCopyTextForItem(const FCkEqsDebugger_PerTestInfo& InT) const -> FString;

    TSharedPtr<FCkEqsDebugger_ViewModel> _ViewModel;

    TArray<TSharedPtr<FCkEqsDebugger_PerTestInfo>> _Items;
    TSharedPtr<SListView<TSharedPtr<FCkEqsDebugger_PerTestInfo>>> _ListView;

    TSharedPtr<SCkDebug_SelectableLabel> _HeaderLabel;

    FDelegateHandle _OnQueryListChangedHandle;
    FDelegateHandle _OnSelectedCandidateChangedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
