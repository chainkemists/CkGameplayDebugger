#pragma once

#include "CkEditorTools/Style/CkStyle.h"

#include "Math/Color.h"

// --------------------------------------------------------------------------------------------------------------------
// Slate-side style entry points for the Crowd Debugger.
//
// These used to be seven hardcoded FLinearColor constants approximating the PLAN.md mockup. They
// now resolve from `CkStyle::` roles, so an Editor Preferences -> Ck -> Style edit moves them with
// the rest of the suite. Accessors, not constants: the roles read the settings CDO, and a
// namespace-scope constant would resolve at static-init time.
//
// NOTE for whoever picks this up next: as of the common-widget consolidation this namespace has
// ZERO call sites in CkCrowdDebugger — every live panel already reads `CkStyle::` directly. It is
// left in place (rather than deleted unprompted) purely so the mapping above is on record.
// --------------------------------------------------------------------------------------------------------------------

namespace CkCrowdDebuggerStyle
{
	// Status colors
	inline auto StatusOk()     -> FLinearColor { return CkStyle::Ok(); }        // Walking / OK
	inline auto StatusWarn()   -> FLinearColor { return CkStyle::Warn(); }      // Replan
	inline auto StatusError()  -> FLinearColor { return CkStyle::Err(); }       // Failed
	inline auto StatusInfo()   -> FLinearColor { return CkStyle::Info(); }      // Live (proxy)
	inline auto StatusAsleep() -> FLinearColor { return CkStyle::TextMute(); }  // Asleep / dim

	// Selection accent. The dim variant is the same role at reduced weight rather than a second hue,
	// which is what "dim" meant in the mockup.
	inline auto AccentSelected()    -> FLinearColor { return CkStyle::Accent(); }
	inline auto AccentSelectedDim() -> FLinearColor { return CkStyle::AccentDim(); }

	// Layout — aliases onto the shared spacing scale.
	inline constexpr auto PanelPadding   = CkStyle::SpaceL;    // 12
	inline constexpr auto SectionSpacing = CkStyle::SpaceXL;   // 16
	inline constexpr auto RowHeight      = 18.0f;
}

// --------------------------------------------------------------------------------------------------------------------
