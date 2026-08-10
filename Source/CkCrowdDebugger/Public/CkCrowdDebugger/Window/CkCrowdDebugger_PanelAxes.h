#pragma once

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Layout/Margin.h"
#include "Widgets/SWidget.h"

// --------------------------------------------------------------------------------------------------------------------
// The Layer-B axis floor for CkCrowdDebugger's panel surfaces. Every panel in this module composes
// its pane heading and its container padding through here so a single axis flip moves all of them —
// AgentList, AgentDetail, Stats, NavmeshStatus, EventLog, and the window's popover sections.
//
// Deliberately NOT used by the viewport / 3d-viewport paint paths: those draw a world, not a
// document, and have no rows or headers for the axes to describe.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::crowd_debugger_axes
{
	/** The panel's uppercase pane heading, routed through the SectionHeaderStyle axis. */
	inline auto Make_PaneHeading(const FString& InLabel) -> TSharedRef<SWidget>
	{
		return ck::debug_axes::Make_SectionHeader(
			UCkDebuggerStyleSettings::Get_Selection(), FText::FromString(InLabel), ECk_Tone::Neutral);
	}

	/**
	 * RowDensity applied as a DELTA on a surface's own base padding, never as an absolute — these
	 * panels deliberately disagree on absolute spacing, and only the offset between density options
	 * is the axis' business. Comfortable is the axis default, so under Classic the delta is zero and
	 * every panel renders exactly as it shipped. Clamped so Compact can't produce negative margins.
	 */
	inline auto Apply_RowDensity(const FMargin& InBase) -> FMargin
	{
		const auto Baseline = ck::debug_axes::Get_RowPadding(FCkDebuggerStyleSelection{});
		const auto Current  = ck::debug_axes::Get_RowPadding(UCkDebuggerStyleSettings::Get_Selection());

		const auto DeltaX = Current.Left - Baseline.Left;
		const auto DeltaY = Current.Top  - Baseline.Top;

		return FMargin
		{
			FMath::Max(0.0f, InBase.Left   + DeltaX),
			FMath::Max(0.0f, InBase.Top    + DeltaY),
			FMath::Max(0.0f, InBase.Right  + DeltaX),
			FMath::Max(0.0f, InBase.Bottom + DeltaY)
		};
	}
}

// --------------------------------------------------------------------------------------------------------------------
