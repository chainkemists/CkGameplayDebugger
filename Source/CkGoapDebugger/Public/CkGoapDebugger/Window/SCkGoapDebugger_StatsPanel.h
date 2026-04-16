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

	auto SetSelectedAction(const FCkGoapDebugger_ActionInfo* InAction, int32 InPlanStepIndex) -> void;
	auto ClearSelection() -> void;

private:
	auto RebuildContent() -> void;
	auto BuildNoSelectionContent() -> TSharedRef<SWidget>;
	auto BuildActionContent() -> TSharedRef<SWidget>;

private:
	TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;
	TSharedPtr<SVerticalBox> _ContentBox;

	const FCkGoapDebugger_ActionInfo* _SelectedAction = nullptr;
	int32 _PlanStepIndex = -1;
};

// ====================================================================================================================
