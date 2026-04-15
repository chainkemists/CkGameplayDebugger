#pragma once

#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

class SCkGoapDebugger_StatsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGoapDebugger_StatsPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual auto Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
	auto RefreshFromGoapInfo(const FCkGoapDebugger_GoapInfo* InInfo) -> void;

private:
	TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

	TSharedPtr<STextBlock> _IterationsText;
	TSharedPtr<STextBlock> _OpenSetText;
	TSharedPtr<STextBlock> _ClosedSetText;
	TSharedPtr<STextBlock> _PlanLengthText;
	TSharedPtr<STextBlock> _BudgetText;
	TSharedPtr<SProgressBar> _BudgetBar;
	TSharedPtr<STextBlock> _ActionsCountText;
	TSharedPtr<STextBlock> _GoalsCountText;
	TSharedPtr<STextBlock> _WorldStateCountText;
	TSharedPtr<SVerticalBox> _HistoryBox;

	int32 _LastHistoryCount = -1;
};

// ====================================================================================================================
