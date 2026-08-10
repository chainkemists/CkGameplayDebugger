#pragma once

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Layout/Margin.h"
#include "Widgets/SWidget.h"

// --------------------------------------------------------------------------------------------------------------------
// The module's own axis-composed content: the pane heading every CkCrowdDebugger panel shares —
// AgentList, AgentDetail, Stats, NavmeshStatus, EventLog, and the window's popover sections.
// The metric deltas (row density, icon size, separators) are NOT here: they are suite-wide
// measurements and live in `ck::debug_axes` (CkDebuggerCommon/Styles/CkDebuggerAxes.h).
//
// Deliberately NOT used by the viewport / 3d-viewport paint paths: those draw a world, not a
// document, and have no rows or headers for the axes to describe.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::crowd_debugger_axes
{
	/**
	 * The panel's uppercase pane heading, routed through the SectionHeaderStyle axis.
	 *
	 * Nothing is cached here: `Make_SectionHeader` is live by construction (R1), so the widget this
	 * returns re-reads the axis at paint time and the `Get_Selection()` argument is only carried for
	 * call-site symmetry. A heading built at Construct therefore follows a Style Lab flip with no
	 * rebuild — which is why this stays a thin pass-through rather than growing a cache.
	 */
	inline auto Make_PaneHeading(const FString& InLabel) -> TSharedRef<SWidget>
	{
		return ck::debug_axes::Make_SectionHeader(
			UCkDebuggerStyleSettings::Get_Selection(), FText::FromString(InLabel), ECk_Tone::Neutral);
	}

	// ----- Typography ----------------------------------------------------------
	// Free, argument-less getters so call sites bind through `.Font_Static(&...)` instead of baking
	// an FSlateFontInfo — the panels here are built once and only regenerate rows when the agent SET
	// changes, so a construct-time size would freeze TextScale whenever the crowd is stable.

	/** The muted trailing column (owner name, secondary meta) — one step below body text. */
	inline auto Get_Font_Micro() -> FSlateFontInfo
	{ return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro()); }

	/** Emphasis inside a detail row (section value, header label). */
	inline auto Get_Font_Bold_Body() -> FSlateFontInfo
	{ return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeBody()); }

	/** Emphasis on a compact chip / badge. */
	inline auto Get_Font_Bold_Small() -> FSlateFontInfo
	{ return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeSmall()); }

	/** The viewport panel's own heading, which predates the shared pane heading. */
	inline auto Get_Font_PaneHeading() -> FSlateFontInfo
	{ return ck::debug_axes::ScaledFont("Bold", CkStyle::PaneHeadingFontSize()); }

	/** Body text on a viewport chrome row. */
	inline auto Get_Font_Small() -> FSlateFontInfo
	{ return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); }

	// ----- Row metrics ---------------------------------------------------------

	/** The agent list's own row padding, under RowDensity. */
	inline auto Get_AgentRowPadding() -> FMargin
	{ return ck::debug_axes::Apply_RowDensity(FMargin{8.0f, 3.0f}); }
}

// --------------------------------------------------------------------------------------------------------------------
