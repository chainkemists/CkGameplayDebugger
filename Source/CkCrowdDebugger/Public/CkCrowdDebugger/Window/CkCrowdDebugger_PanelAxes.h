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
	/** The panel's uppercase pane heading, routed through the SectionHeaderStyle axis. */
	inline auto Make_PaneHeading(const FString& InLabel) -> TSharedRef<SWidget>
	{
		return ck::debug_axes::Make_SectionHeader(
			UCkDebuggerStyleSettings::Get_Selection(), FText::FromString(InLabel), ECk_Tone::Neutral);
	}
}

// --------------------------------------------------------------------------------------------------------------------
