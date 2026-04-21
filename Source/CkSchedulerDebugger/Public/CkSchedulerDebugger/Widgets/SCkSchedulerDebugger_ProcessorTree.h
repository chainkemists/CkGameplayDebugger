#pragma once

#include "CkSchedulerDebugger/Data/CkSchedulerDebugger_Types.h"
#include "CkSchedulerDebugger/ViewModel/CkSchedulerDebugger_ViewModel.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Input/SSearchBox.h"

// --------------------------------------------------------------------------------------------------------------------

enum class ECkSchedulerDebugger_SortMode : uint8
{
	ExecutionOrder,
	Name,
	Timing
};

// --------------------------------------------------------------------------------------------------------------------

class SCkSchedulerDebugger_ProcessorTree : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkSchedulerDebugger_ProcessorTree) {}
		SLATE_ARGUMENT(TSharedPtr<FCkSchedulerDebugger_ViewModel>, ViewModel)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	~SCkSchedulerDebugger_ProcessorTree();

private:
	auto DoOnDataRefreshed() -> void;
	auto DoRebuildFlattenedTree() -> void;
	auto DoApplyFilter(const FText& InFilterText) -> void;
	auto DoApplySort() -> void;
	auto DoMatchesFilter(const FCkSchedulerDebugger_TreeNode& InNode, const FString& InFilter) const -> bool;

	auto DoGenerateRow(
		TSharedPtr<FCkSchedulerDebugger_TreeNode> InItem,
		const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;

	auto DoGetChildren(
		TSharedPtr<FCkSchedulerDebugger_TreeNode> InItem,
		TArray<TSharedPtr<FCkSchedulerDebugger_TreeNode>>& OutChildren) -> void;

	auto DoOnSelectionChanged(
		TSharedPtr<FCkSchedulerDebugger_TreeNode> InItem,
		ESelectInfo::Type InSelectInfo) -> void;

	auto DoBuildPumpSection() -> TSharedRef<SWidget>;

	// Stable-tree builder for the pump/frame breakdown. Called only on
	// structural hash change (topology grew, filter/scrub changed). Produces
	// a pre-allocated widget tree where visibility and text for every row
	// bind to TAttribute lambdas reading live from the data collector — so
	// per-frame pump toggles, timing updates and entity-count changes flow
	// through without ANY structural widget mutation. Kills scrunch.
	auto DoBuildPumpBreakdown_StableTree() -> TSharedRef<SWidget>;

private:
	TSharedPtr<FCkSchedulerDebugger_ViewModel> _ViewModel;
	TArray<TSharedPtr<FCkSchedulerDebugger_TreeNode>> _DisplayRoots;
	TSharedPtr<STreeView<TSharedPtr<FCkSchedulerDebugger_TreeNode>>> _TreeView;

	FString _FilterString;
	FString _BreakdownFilterString;
	bool _BreakdownHideIdle = false;
	ECkSchedulerDebugger_SortMode _SortMode = ECkSchedulerDebugger_SortMode::ExecutionOrder;
	TSharedPtr<SBox> _PumpContainer;

	FDelegateHandle _DataRefreshedHandle;
	uint32 _LastPumpDataHash = 0;

	// Grow-only counter tracking the highest pump count we've ever observed.
	// The pump-breakdown structure pre-allocates Pass sections up to this
	// number and toggles their Visibility per-frame based on live PumpCount.
	// Only growing the counter triggers a structural rebuild — per-frame
	// variance in how many passes actually ran does not.
	int32 _MaxObservedPumpCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------
