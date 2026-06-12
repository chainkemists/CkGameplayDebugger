#pragma once

#include "CkCrowdDebugger/Data/CkCrowdDebugger_Types.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class FCkCrowdDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------

class SCkCrowdDebugger_AgentListPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkCrowdDebugger_AgentListPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FCkCrowdDebugger_ViewModel>, ViewModel)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual ~SCkCrowdDebugger_AgentListPanel();

private:
	using ItemPtr = TSharedPtr<FCkCrowdDebugger_AgentSnapshot>;

	auto OnAgentListChanged(const TArray<FCkCrowdDebugger_AgentSnapshot>& InAgents) -> void;
	auto OnGenerateRow(ItemPtr InItem, const TSharedRef<STableViewBase>& InTable) -> TSharedRef<ITableRow>;
	auto OnSelectionChanged(ItemPtr InItem, ESelectInfo::Type InSelectInfo) -> void;
	auto OnContextMenuOpening() -> TSharedPtr<SWidget>;

	// Cross-debugger selection sync (CkDebug_SelectionSync) — selects the
	// agent whose lineage contains/is-contained-by the broadcast entity.
	auto OnGlobalSelectionSync(const FCk_Handle& InSelected, FName InSource) -> void;

private:
	TSharedPtr<FCkCrowdDebugger_ViewModel> _ViewModel;
	TArray<ItemPtr> _ItemSource;
	TSharedPtr<SListView<ItemPtr>> _ListView;
	FDelegateHandle _OnListChangedHandle;
	FDelegateHandle _OnSelectionSyncHandle;
};

// --------------------------------------------------------------------------------------------------------------------
