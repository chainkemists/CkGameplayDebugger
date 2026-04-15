#pragma once

#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

class SCkGoapDebugger_PlanView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGoapDebugger_PlanView) {}
		SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual auto Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
	auto RebuildPlanList() -> void;

private:
	TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;
	TSharedPtr<SVerticalBox> _PlanListBox;
	TSharedPtr<STextBlock> _PlanCostText;
	int32 _LastPlanLength = -1;
	ECk_GoapPlanStatus _LastStatus = ECk_GoapPlanStatus::Idle;
};

// ====================================================================================================================
