#pragma once

#include "CoreMinimal.h"

// ====================================================================================================================
// Color, font-size, and node-visual tokens shared by GOAP, SM, and future Ck
// debugger UIs. All non-constexpr values route through UCkDebuggerStyleSettings
// so users can tune the palette from Project Settings → CkFoundation → GOAP.
// The function accessors are lazy-resolved so there are no static init order
// surprises.
// ====================================================================================================================

struct FSlateBrush;

// Forward-declared tone enum (defined in SCkDebug_StatusPill.h) so callers
// that only need GetToneColor don't have to drag the widget header in.
enum class ECkDebug_Tone : uint8;

namespace CkDebugStyle
{
	// ----- Helpers ------------------------------------------------------------
	inline auto Rgb(uint8 R, uint8 G, uint8 B) -> FLinearColor
	{
		return FLinearColor(FColor(R, G, B, 255));
	}

	inline auto OverlayOf(const FLinearColor& InBase, float InAlpha) -> FLinearColor
	{
		auto C = InBase; C.A = InAlpha; return C;
	}

	// ----- Palette ------------------------------------------------------------
	CKDEBUGGERCOMMON_API auto BgRoot()       -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Bg1()          -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Bg2()          -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Bg3()          -> FLinearColor;

	CKDEBUGGERCOMMON_API auto Border()       -> FLinearColor;
	CKDEBUGGERCOMMON_API auto BorderStrong() -> FLinearColor;

	CKDEBUGGERCOMMON_API auto Text()         -> FLinearColor;
	CKDEBUGGERCOMMON_API auto TextDim()      -> FLinearColor;
	CKDEBUGGERCOMMON_API auto TextMute()     -> FLinearColor;
	CKDEBUGGERCOMMON_API auto TextStrong()   -> FLinearColor;

	CKDEBUGGERCOMMON_API auto Accent()       -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Ok()           -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Err()          -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Warn()         -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Info()         -> FLinearColor;

	// ----- Selection & Hover --------------------------------------------------
	CKDEBUGGERCOMMON_API auto Selection()         -> FLinearColor;
	CKDEBUGGERCOMMON_API auto SelectionInactive() -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Hover()             -> FLinearColor;

	// ----- Domain (ECS / component semantics) --------------------------------
	CKDEBUGGERCOMMON_API auto None()               -> FLinearColor;
	CKDEBUGGERCOMMON_API auto EntityId()           -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Transform()          -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Network()            -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Relationship()       -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Attribute()          -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Reference()          -> FLinearColor;
	CKDEBUGGERCOMMON_API auto PickMarker_Default() -> FLinearColor;
	CKDEBUGGERCOMMON_API auto PickMarker_Hover()   -> FLinearColor;

	// ----- Value-type colors -------------------------------------------------
	CKDEBUGGERCOMMON_API auto Value_Bool_True()  -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Value_Bool_False() -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Value_Numeric()    -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Value_String()     -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Value_Math()       -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Value_Tag()        -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Value_Enum()       -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Value_Object()     -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Value_Handle()     -> FLinearColor;

	// ----- State colors ------------------------------------------------------
	CKDEBUGGERCOMMON_API auto State_Enabled()     -> FLinearColor;
	CKDEBUGGERCOMMON_API auto State_Disabled()    -> FLinearColor;
	CKDEBUGGERCOMMON_API auto State_Overlapping() -> FLinearColor;
	CKDEBUGGERCOMMON_API auto State_Config()      -> FLinearColor;

	// ----- Status colors (objective / task progress) -------------------------
	CKDEBUGGERCOMMON_API auto Status_NotStarted() -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Status_Active()     -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Status_Completed()  -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Status_Failed()     -> FLinearColor;

	// ----- Graph canvas colors -----------------------------------------------
	CKDEBUGGERCOMMON_API auto Graph_Background()         -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Graph_Edge()               -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Graph_Node_Center()        -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Graph_Node_Default()       -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Graph_Node_Border_Default()-> FLinearColor;
	CKDEBUGGERCOMMON_API auto Graph_Node_Border_Center() -> FLinearColor;

	CKDEBUGGERCOMMON_API auto CategoryGather()   -> FLinearColor;
	CKDEBUGGERCOMMON_API auto CategoryBuild()    -> FLinearColor;
	CKDEBUGGERCOMMON_API auto CategoryResearch() -> FLinearColor;
	CKDEBUGGERCOMMON_API auto CategoryTrain()    -> FLinearColor;
	CKDEBUGGERCOMMON_API auto CategoryAge()      -> FLinearColor;
	CKDEBUGGERCOMMON_API auto CategoryTrade()    -> FLinearColor;

	// ----- Typography ---------------------------------------------------------
	CKDEBUGGERCOMMON_API auto FontSizeH2()    -> int32;
	CKDEBUGGERCOMMON_API auto FontSizeH3()    -> int32;
	CKDEBUGGERCOMMON_API auto FontSizeH4()    -> int32;
	CKDEBUGGERCOMMON_API auto FontSizeBody()  -> int32;
	CKDEBUGGERCOMMON_API auto FontSizeSmall() -> int32;
	CKDEBUGGERCOMMON_API auto FontSizeMicro() -> int32;

	// ----- Pane headings ------------------------------------------------------
	CKDEBUGGERCOMMON_API auto PaneHeadingFontSize() -> int32;
	CKDEBUGGERCOMMON_API auto PaneHeadingColor()    -> FLinearColor;

	// ----- Plan strip ---------------------------------------------------------
	CKDEBUGGERCOMMON_API auto PlanStrip_TitleFontSize()     -> int32;
	CKDEBUGGERCOMMON_API auto PlanStrip_MetaFontSize()      -> int32;
	CKDEBUGGERCOMMON_API auto PlanStrip_GoalLabelFontSize() -> int32;
	CKDEBUGGERCOMMON_API auto PlanStrip_GoalNameFontSize()  -> int32;
	CKDEBUGGERCOMMON_API auto PlanStrip_StepNameFontSize()  -> int32;
	CKDEBUGGERCOMMON_API auto PlanStrip_StepCostFontSize()  -> int32;
	CKDEBUGGERCOMMON_API auto PlanStrip_StepStateFontSize() -> int32;

	CKDEBUGGERCOMMON_API auto PlanStep_Fill_Pending()   -> FLinearColor;
	CKDEBUGGERCOMMON_API auto PlanStep_Border_Pending() -> FLinearColor;
	CKDEBUGGERCOMMON_API auto PlanStep_Badge_Pending()  -> FLinearColor;
	CKDEBUGGERCOMMON_API auto PlanStep_Fill_Active()    -> FLinearColor;
	CKDEBUGGERCOMMON_API auto PlanStep_Border_Active()  -> FLinearColor;
	CKDEBUGGERCOMMON_API auto PlanStep_Badge_Active()   -> FLinearColor;
	CKDEBUGGERCOMMON_API auto PlanStep_Fill_Done()      -> FLinearColor;
	CKDEBUGGERCOMMON_API auto PlanStep_Border_Done()    -> FLinearColor;
	CKDEBUGGERCOMMON_API auto PlanStep_Badge_Done()     -> FLinearColor;
	CKDEBUGGERCOMMON_API auto PlanStrip_GoalFill()      -> FLinearColor;
	CKDEBUGGERCOMMON_API auto PlanStrip_GoalBorder()    -> FLinearColor;

	// ----- Graph node visuals -------------------------------------------------
	CKDEBUGGERCOMMON_API auto NodeFill_Inactive()     -> FLinearColor;
	CKDEBUGGERCOMMON_API auto NodeBorder_Inactive()   -> FLinearColor;
	CKDEBUGGERCOMMON_API auto NodeFill_InPlan()       -> FLinearColor;
	CKDEBUGGERCOMMON_API auto NodeBorder_InPlan()     -> FLinearColor;
	CKDEBUGGERCOMMON_API auto NodeFill_Goal()         -> FLinearColor;
	CKDEBUGGERCOMMON_API auto NodeBorder_Goal()       -> FLinearColor;
	CKDEBUGGERCOMMON_API auto NodeFill_GoalInactive() -> FLinearColor;
	CKDEBUGGERCOMMON_API auto NodeTitleFontSize()     -> int32;
	CKDEBUGGERCOMMON_API auto NodeCostFontSize()      -> int32;
	CKDEBUGGERCOMMON_API auto NodeMetaFontSize()      -> int32;
	CKDEBUGGERCOMMON_API auto NodeBorderThickness()   -> float;
	CKDEBUGGERCOMMON_API auto NodeInactiveOpacity()   -> float;

	// ----- Spacing (compile-time constants — no need to tune) -----------------
	constexpr auto SpaceXS  = 2.0f;
	constexpr auto SpaceS   = 4.0f;
	constexpr auto SpaceM   = 8.0f;
	constexpr auto SpaceL   = 12.0f;
	constexpr auto SpaceXL  = 16.0f;
	constexpr auto SpaceXXL = 24.0f;

	// ----- Brushes / helpers --------------------------------------------------
	CKDEBUGGERCOMMON_API auto GetFilledBrush()  -> const FSlateBrush*;
	CKDEBUGGERCOMMON_API auto GetRoundedBrush() -> const FSlateBrush*;
	CKDEBUGGERCOMMON_API auto GetToneColor(ECkDebug_Tone InTone) -> FLinearColor;
}

// ====================================================================================================================
