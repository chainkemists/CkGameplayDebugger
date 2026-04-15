#pragma once

#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

class SCkGoapDebugger_GoalPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGoapDebugger_GoalPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual auto Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
	auto RebuildGoalList() -> void;

private:
	TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;
	TSharedPtr<SVerticalBox> _GoalListBox;
	int32 _LastGoalCount = -1;
};

// ====================================================================================================================
