#pragma once

#include "CkNavDebugger/Data/CkNavDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkNavDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------
// Rolling failure log. Click a row → select that agent (drives in-world overlay + inspector).
// SListView avoids the full-tree rebuild flicker that plagued the previous clear+append pattern.
// --------------------------------------------------------------------------------------------------------------------

class SCkNavDebugger_FailureLogPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkNavDebugger_FailureLogPanel) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, TSharedRef<FCkNavDebugger_ViewModel> InViewModel) -> void;

    auto Refresh() -> void;

private:
    auto OnGenerateRow(TSharedPtr<FCkNavDebugger_FailureLogEntry> InItem, const TSharedRef<STableViewBase>& InOwner)
        -> TSharedRef<ITableRow>;
    auto OnSelectionChanged(TSharedPtr<FCkNavDebugger_FailureLogEntry> InItem, ESelectInfo::Type InSelectInfo) -> void;

    TWeakPtr<FCkNavDebugger_ViewModel> _ViewModel;
    TArray<TSharedPtr<FCkNavDebugger_FailureLogEntry>> _Items;
    TSharedPtr<SListView<TSharedPtr<FCkNavDebugger_FailureLogEntry>>> _ListView;

    int32 _LastSeenLogCount = -1;
};

// --------------------------------------------------------------------------------------------------------------------
