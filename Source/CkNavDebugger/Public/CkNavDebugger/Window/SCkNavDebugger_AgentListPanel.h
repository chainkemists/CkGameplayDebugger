#pragma once

#include "CkNavDebugger/Data/CkNavDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkNavDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------
// Scrollable list of all nav agents in the registry. Click a row → select agent (drives the
// in-world overlay + the inspector panel + the SelectedEntityId cvar).
// --------------------------------------------------------------------------------------------------------------------

class SCkNavDebugger_AgentListPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkNavDebugger_AgentListPanel) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, TSharedRef<FCkNavDebugger_ViewModel> InViewModel) -> void;

    auto Refresh() -> void;

private:
    auto OnGenerateRow(TSharedPtr<FCkNavDebugger_AgentInfo> InItem, const TSharedRef<STableViewBase>& InOwner)
        -> TSharedRef<ITableRow>;
    auto OnSelectionChanged(TSharedPtr<FCkNavDebugger_AgentInfo> InItem, ESelectInfo::Type InSelectInfo) -> void;

    TWeakPtr<FCkNavDebugger_ViewModel> _ViewModel;
    TArray<TSharedPtr<FCkNavDebugger_AgentInfo>> _Items;
    TSharedPtr<SListView<TSharedPtr<FCkNavDebugger_AgentInfo>>> _ListView;
};

// --------------------------------------------------------------------------------------------------------------------
