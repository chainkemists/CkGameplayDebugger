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

	// F — frame the selected agent in the ejected editor viewport.
	virtual auto OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) -> FReply override;
	virtual auto SupportsKeyboardFocus() const -> bool override { return true; }
	// Applies an already-resolved crowd-agent target and frames its row.
	auto SelectEntityExternal(FCk_Entity InEntity) -> void;

private:
	using ItemPtr = TSharedPtr<FCkCrowdDebugger_AgentSnapshot>;

	auto OnAgentListChanged(const TArray<FCkCrowdDebugger_AgentSnapshot>& InAgents) -> void;

	// Filter (hide) + highlight (dim) pass over the ViewModel's agent list.
	// Deliberately takes the source list by reference rather than caching a copy:
	// a cached TArray<FCkCrowdDebugger_AgentSnapshot> would be another container of
	// FCk_Handle to clear on EndPIE, and the ViewModel already owns this data.
	auto ApplyFilterPipeline(const TArray<FCkCrowdDebugger_AgentSnapshot>& InAgents) -> void;

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
	TSharedPtr<class SCkDebug_DualSearchBar> _SearchBar;
	FDelegateHandle _OnListChangedHandle;
	FDelegateHandle _OnSelectionSyncHandle;

	// Dual search state. Filter hides rows; Highlight dims the ones that survive
	// the filter (row text colour reads these through a weak-panel lambda).
	FString _FilterString;
	FString _HighlightString;

	// Sync-received selection flash — the row's background tint fades out so the
	// user notices WHICH agent a cross-debugger selection resolved to.
	TWeakPtr<FCkCrowdDebugger_AgentSnapshot> _SyncFlashItem;
	double _SyncFlashEndSeconds = 0.0;
};

// --------------------------------------------------------------------------------------------------------------------
