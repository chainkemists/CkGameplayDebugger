#pragma once

#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Action Detail Panel — shows full context for the selected action node
// ====================================================================================================================

class UCkGoapDebugNode_Action;

class SCkGoapDebugger_StatsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGoapDebugger_StatsPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual auto Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

	auto SetSelectedAction(const FCkGoapDebugger_ActionInfo* InAction, int32 InPlanStepIndex) -> void;
	auto ClearSelection() -> void;

private:
	auto RebuildContent() -> void;
	auto BuildNoSelectionContent() -> TSharedRef<SWidget>;
	auto BuildActionContent(const FCkGoapDebugger_ActionInfo& InAction) -> TSharedRef<SWidget>;

private:
	TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;
	TSharedPtr<SVerticalBox> _ContentBox;

	// Store the action's class name (stable across data-collector rebuilds)
	// rather than a raw pointer (the DataCollector resets _GoapEntities every
	// tick, which would dangle any FCkGoapDebugger_ActionInfo* captured here).
	FString _SelectedActionName;
	int32 _PlanStepIndex = -1;

	// Content hash of the last rendered action info; re-renders only when
	// world state / plan index / action data changes.
	uint32 _LastContentHash = 0;
};

// ====================================================================================================================
