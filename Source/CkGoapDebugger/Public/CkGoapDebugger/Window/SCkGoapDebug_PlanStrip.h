#pragma once

#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "Widgets/SCompoundWidget.h"

class SHorizontalBox;
class STextBlock;

// ====================================================================================================================

DECLARE_DELEGATE_OneParam(FOnCkGoapDebug_PlanStepClicked, FString /*ClassName*/);

// ====================================================================================================================
// The hero band at the bottom of the graph viewport:
//   [Plan · N steps · cost C · X/N done] ……………… Goal: <Name>
//   (step pill) → (step pill) → … → (goal pill)
//
// Subscribes to the ViewModel. Rebuilds only when the plan contents change.
// ====================================================================================================================

class CKGOAPDEBUGGER_API SCkGoapDebug_PlanStrip : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGoapDebug_PlanStrip) {}
		SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
		SLATE_EVENT(FOnCkGoapDebug_PlanStepClicked, OnStepClicked)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual auto Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
	auto RebuildStrip() -> void;
	auto ComputeHash() const -> uint32;
	auto BuildStepPill(int32 InStepIdx, const FString& InActionName, float InCost, bool bDone, bool bActive) -> TSharedRef<SWidget>;
	auto BuildArrow(bool bDone, bool bActive) -> TSharedRef<SWidget>;
	auto BuildGoalPill(const FCkGoapDebugger_GoalInfo& InGoal) -> TSharedRef<SWidget>;

private:
	TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;
	FOnCkGoapDebug_PlanStepClicked _OnStepClicked;

	TSharedPtr<SHorizontalBox> _FlowBox;
	TSharedPtr<STextBlock> _MetaText;
	TSharedPtr<STextBlock> _GoalNameText;

	uint32 _LastHash = 0;
};

// ====================================================================================================================
