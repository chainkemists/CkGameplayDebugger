#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Shared visual for every "rectangular debugger node" — the center body of
// graph-style debug nodes AND plan-strip step pills. Keeping them in one
// widget means border/fill/badge/typography tokens flow from a single render
// path; tuning one style updates both.
//
// Variant chooses which palette token bundle (NodeFill_* vs PlanStep_Fill_*)
// drives fill / border / badge colors. Step badge is rendered when
// StepIndex >= 0. BodyContent is an optional caller-provided row below the
// header.
//
// Two optional overrides let specific debuggers add semantic overlays without
// forking the widget:
//   - AccentColor          — when set, draws a colored vertical strip at the
//                             left edge of the pill, between the fill and the
//                             first content column. Used by the ECS debugger
//                             to colour-code entity categories. Transparent by
//                             default (no strip).
//   - BorderColorOverride  — when set to a non-transparent color, replaces
//                             the variant's border color. Used by the ECS
//                             debugger to colour-code edge-type (relationship
//                             vs reference vs transform) while keeping the
//                             variant's fill/badge palette.
// ====================================================================================================================

enum class ECkDebug_NodePillVariant : uint8
{
	Inactive,  // graph: registered but not in current plan  (NodeFill/Border_Inactive)
	InPlan,    // graph: part of current plan                 (NodeFill/Border_InPlan)
	Pending,   // plan strip: step not yet active             (PlanStep_Pending)
	Active,    // plan strip: currently executing step        (PlanStep_Active)
	Done,      // plan strip: completed step                  (PlanStep_Done)
};

DECLARE_DELEGATE(FOnCkDebug_NodePillClicked);

class CKDEBUGGERCOMMON_API SCkDebug_NodePill : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkDebug_NodePill)
		: _Variant(ECkDebug_NodePillVariant::Inactive)
		, _StepIndex(-1)
		, _Title(FText::GetEmpty())
		, _CostValue(0.0f)
		, _ShowCost(true)
		, _OpacityOverride(-1.0f)
		, _MinDesiredWidth(0.0f)
		, _AccentColor(FLinearColor::Transparent)
		, _BorderColorOverride(FLinearColor::Transparent)
		, _Selected(false)
	{}
		SLATE_ARGUMENT(ECkDebug_NodePillVariant, Variant)
		SLATE_ARGUMENT(int32, StepIndex)        // -1 = no badge
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(float, CostValue)
		SLATE_ARGUMENT(bool, ShowCost)
		// < 0 means "use variant default" (inactive dims, others are opaque)
		SLATE_ARGUMENT(float, OpacityOverride)
		SLATE_ARGUMENT(float, MinDesiredWidth)
		// Alpha==0 → no strip drawn. Non-transparent → left-edge colored strip.
		SLATE_ARGUMENT(FLinearColor, AccentColor)
		// Alpha==0 → use variant border color. Non-transparent → override.
		SLATE_ARGUMENT(FLinearColor, BorderColorOverride)
		// Draws an extra selection-highlight ring around the pill when true.
		SLATE_ARGUMENT(bool, Selected)
		// When non-empty, right-click on the pill shows a "Copy Text" menu that
		// puts this string on the clipboard. Default off.
		SLATE_ARGUMENT(FString, CopyText)
		SLATE_EVENT(FOnCkDebug_NodePillClicked, OnClicked)
		SLATE_NAMED_SLOT(FArguments, BodyContent)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

	auto Set_Selected(bool InSelected) -> void;

private:
	auto OnButtonClicked() -> FReply;

	FOnCkDebug_NodePillClicked _OnClicked;
	TSharedPtr<class SBorder> _SelectionRing;
	bool _Selected = false;
};

// ====================================================================================================================
