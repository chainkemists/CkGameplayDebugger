#include "CkDebugStyle.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "Styling/AppStyle.h"

// ====================================================================================================================

namespace
{
	auto DoSettings() -> const UCkDebuggerStyleSettings*
	{
		return GetDefault<UCkDebuggerStyleSettings>();
	}
}

#define CK_STYLE_GETTER(Name) \
	auto CkDebugStyle::Name() -> decltype(UCkDebuggerStyleSettings::Name) \
	{ \
		return DoSettings()->Name; \
	}

// ====================================================================================================================

CK_STYLE_GETTER(BgRoot)
CK_STYLE_GETTER(Bg1)
CK_STYLE_GETTER(Bg2)
CK_STYLE_GETTER(Bg3)
CK_STYLE_GETTER(Border)
CK_STYLE_GETTER(BorderStrong)
CK_STYLE_GETTER(Text)
CK_STYLE_GETTER(TextDim)
CK_STYLE_GETTER(TextMute)
CK_STYLE_GETTER(Accent)
CK_STYLE_GETTER(Ok)
CK_STYLE_GETTER(Err)
CK_STYLE_GETTER(Warn)
CK_STYLE_GETTER(Info)

CK_STYLE_GETTER(CategoryGather)
CK_STYLE_GETTER(CategoryBuild)
CK_STYLE_GETTER(CategoryResearch)
CK_STYLE_GETTER(CategoryTrain)
CK_STYLE_GETTER(CategoryAge)
CK_STYLE_GETTER(CategoryTrade)

CK_STYLE_GETTER(FontSizeH2)
CK_STYLE_GETTER(FontSizeH3)
CK_STYLE_GETTER(FontSizeH4)
CK_STYLE_GETTER(FontSizeBody)
CK_STYLE_GETTER(FontSizeSmall)
CK_STYLE_GETTER(FontSizeMicro)

CK_STYLE_GETTER(PaneHeadingFontSize)
CK_STYLE_GETTER(PaneHeadingColor)

CK_STYLE_GETTER(PlanStrip_TitleFontSize)
CK_STYLE_GETTER(PlanStrip_MetaFontSize)
CK_STYLE_GETTER(PlanStrip_GoalLabelFontSize)
CK_STYLE_GETTER(PlanStrip_GoalNameFontSize)
CK_STYLE_GETTER(PlanStrip_StepNameFontSize)
CK_STYLE_GETTER(PlanStrip_StepCostFontSize)
CK_STYLE_GETTER(PlanStrip_StepStateFontSize)

CK_STYLE_GETTER(PlanStep_Fill_Pending)
CK_STYLE_GETTER(PlanStep_Border_Pending)
CK_STYLE_GETTER(PlanStep_Badge_Pending)
CK_STYLE_GETTER(PlanStep_Fill_Active)
CK_STYLE_GETTER(PlanStep_Border_Active)
CK_STYLE_GETTER(PlanStep_Badge_Active)
CK_STYLE_GETTER(PlanStep_Fill_Done)
CK_STYLE_GETTER(PlanStep_Border_Done)
CK_STYLE_GETTER(PlanStep_Badge_Done)
CK_STYLE_GETTER(PlanStrip_GoalFill)
CK_STYLE_GETTER(PlanStrip_GoalBorder)

CK_STYLE_GETTER(NodeFill_Inactive)
CK_STYLE_GETTER(NodeBorder_Inactive)
CK_STYLE_GETTER(NodeFill_InPlan)
CK_STYLE_GETTER(NodeBorder_InPlan)
CK_STYLE_GETTER(NodeFill_Goal)
CK_STYLE_GETTER(NodeBorder_Goal)
CK_STYLE_GETTER(NodeFill_GoalInactive)
CK_STYLE_GETTER(NodeTitleFontSize)
CK_STYLE_GETTER(NodeCostFontSize)
CK_STYLE_GETTER(NodeMetaFontSize)
CK_STYLE_GETTER(NodeBorderThickness)
CK_STYLE_GETTER(NodeInactiveOpacity)

#undef CK_STYLE_GETTER

// ====================================================================================================================

auto
	CkDebugStyle::
	GetFilledBrush()
	-> const FSlateBrush*
{
	return FAppStyle::GetBrush(TEXT("GenericWhiteBox"));
}

auto
	CkDebugStyle::
	GetRoundedBrush()
	-> const FSlateBrush*
{
	// RoundedSelection_16x is not reliably registered across UE builds — when
	// missing, Slate falls back to the "missing texture" checkerboard. Square
	// corners with GenericWhiteBox render reliably everywhere.
	return FAppStyle::GetBrush(TEXT("GenericWhiteBox"));
}

auto
	CkDebugStyle::
	GetToneColor(ECkDebug_Tone InTone)
	-> FLinearColor
{
	switch (InTone)
	{
		case ECkDebug_Tone::Info:   return Info();
		case ECkDebug_Tone::Ok:     return Ok();
		case ECkDebug_Tone::Warn:   return Warn();
		case ECkDebug_Tone::Err:    return Err();
		case ECkDebug_Tone::Accent: return Accent();
		default:                    return TextDim();
	}
}

// ====================================================================================================================
