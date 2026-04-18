#pragma once

#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Failure Analysis Panel — per-goal-condition breakdown of why planning failed
//
// Shows the active (or best) goal's conditions and, for each one:
//   - Current world-state value and whether it already satisfies the condition
//   - Which registered actions have effects that can progress this condition
//   - A red "UNREACHABLE" flag when no action addresses an unsatisfied condition
//     (the typical root cause of PlanFailed in a freshly-built action set)
// ====================================================================================================================

class SCkGoapDebugger_FailureAnalysisPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGoapDebugger_FailureAnalysisPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual auto Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
	auto RebuildContent() -> void;

private:
	TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;
	TSharedPtr<SVerticalBox> _ContentBox;
	uint32 _LastContentHash = 0;
};

// ====================================================================================================================
