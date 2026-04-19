#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// One shared visual widget for every "rectangular action node" in the GOAP
// debugger — the center body of the graph action node AND the plan-strip
// step pills. Keeping them in one widget means border/fill/badge/typography
// tokens flow from a single render path; tuning one style updates both.
//
// Variant chooses which palette token bundle (NodeFill_* vs PlanStep_Fill_*)
// drives fill / border / badge colors. Step badge is rendered when
// StepIndex >= 0. BodyContent is an optional caller-provided row below the
// header — plan strip puts the state text there, graph nodes put pre/eff
// rows.
// ====================================================================================================================

enum class ECkGoapDebug_ActionPillVariant : uint8
{
	Inactive,  // graph: registered but not in current plan  (Node_Inactive)
	InPlan,    // graph: part of current plan, not running   (Node_InPlan)
	Pending,   // plan strip: step not yet active            (PlanStep_Pending)
	Active,    // plan strip: currently executing step       (PlanStep_Active)
	Done,      // plan strip: completed step                 (PlanStep_Done)
};

DECLARE_DELEGATE(FOnCkGoapDebug_ActionPillClicked);

class CKGOAPDEBUGGER_API SCkGoapDebug_ActionPill : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGoapDebug_ActionPill)
		: _Variant(ECkGoapDebug_ActionPillVariant::Inactive)
		, _StepIndex(-1)
		, _Title(FText::GetEmpty())
		, _CostValue(0.0f)
		, _ShowCost(true)
		, _OpacityOverride(-1.0f)
		, _MinDesiredWidth(0.0f)
	{}
		SLATE_ARGUMENT(ECkGoapDebug_ActionPillVariant, Variant)
		SLATE_ARGUMENT(int32, StepIndex)        // -1 = no badge
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(float, CostValue)
		SLATE_ARGUMENT(bool, ShowCost)
		// < 0 means "use variant default" (inactive dims, others are opaque)
		SLATE_ARGUMENT(float, OpacityOverride)
		SLATE_ARGUMENT(float, MinDesiredWidth)
		SLATE_EVENT(FOnCkGoapDebug_ActionPillClicked, OnClicked)
		SLATE_NAMED_SLOT(FArguments, BodyContent)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

private:
	auto OnButtonClicked() -> FReply;

	FOnCkGoapDebug_ActionPillClicked _OnClicked;
};

// ====================================================================================================================
