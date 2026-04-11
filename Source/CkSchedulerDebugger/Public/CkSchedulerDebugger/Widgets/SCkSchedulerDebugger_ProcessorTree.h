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

private:
	TSharedPtr<FCkSchedulerDebugger_ViewModel> _ViewModel;
	TArray<TSharedPtr<FCkSchedulerDebugger_TreeNode>> _DisplayRoots;
	TSharedPtr<STreeView<TSharedPtr<FCkSchedulerDebugger_TreeNode>>> _TreeView;

	FString _FilterString;
	FString _BreakdownFilterString;
	ECkSchedulerDebugger_SortMode _SortMode = ECkSchedulerDebugger_SortMode::ExecutionOrder;
	TSharedPtr<SBox> _PumpContainer;

	FDelegateHandle _DataRefreshedHandle;
	uint32 _LastPumpDataHash = 0;
};

// --------------------------------------------------------------------------------------------------------------------
