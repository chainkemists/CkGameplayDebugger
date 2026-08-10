#pragma once

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Layout/Margin.h"

// --------------------------------------------------------------------------------------------------------------------
// The module's axis-composed TYPOGRAPHY and row metrics, in the one form the Scheduler debugger can
// actually consume: free functions with no arguments, so every call site binds them through
// `.Font_Static(&...)` / `TAttribute<FMargin>::CreateStatic(&...)` instead of baking a value.
//
// Why that matters here more than anywhere else in the suite: the processor tree and the inspector
// build hundreds of text blocks inside row generators that only re-run when the DATA changes. A
// construct-time `ScaledFont(...)` in those paths would freeze TextScale until the next scheduler
// pump changed the tree, which is exactly the "freezes at rest" failure this campaign is closing.
//
// Size mapping from the literals these replaced: 8 -> FontSizeMicro, 9 -> FontSizeBody,
// 10 -> FontSizeH3, 12 -> FontSizeH2. The three sizes with no matching role (11 / 14 / 16) stay
// literal INSIDE ScaledFont and say so at their call site — they still follow the axis, they just
// do not claim a role they do not have.
//
// Colors are deliberately absent: they resolve from CkStyle:: roles at the call site and were never
// baked in the first place.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::scheduler_debugger_axes
{
	// ----- Typography ----------------------------------------------------------

	inline auto Get_Font_Regular_Micro() -> FSlateFontInfo
	{ return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro()); }

	inline auto Get_Font_Regular_Body() -> FSlateFontInfo
	{ return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeBody()); }

	inline auto Get_Font_Regular_H3() -> FSlateFontInfo
	{ return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeH3()); }

	inline auto Get_Font_Bold_Micro() -> FSlateFontInfo
	{ return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeMicro()); }

	inline auto Get_Font_Bold_Body() -> FSlateFontInfo
	{ return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeBody()); }

	inline auto Get_Font_Bold_H3() -> FSlateFontInfo
	{ return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeH3()); }

	inline auto Get_Font_Bold_H2() -> FSlateFontInfo
	{ return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeH2()); }

	inline auto Get_Font_Italic_Micro() -> FSlateFontInfo
	{ return ck::debug_axes::ScaledFont("Italic", CkStyle::FontSizeMicro()); }

	inline auto Get_Font_Italic_Body() -> FSlateFontInfo
	{ return ck::debug_axes::ScaledFont("Italic", CkStyle::FontSizeBody()); }

	/** Deliberate outlier: the empty-state / "no selection" italic is one step ABOVE FontSizeH3 and
	 *  below FontSizeH2, and reading it as either changes the panel's balance. Kept literal. */
	inline auto Get_Font_Italic_EmptyState() -> FSlateFontInfo
	{ return ck::debug_axes::ScaledFont("Italic", 11); }

	/** Deliberate outlier: the window's own title, larger than any FontSize* role. */
	inline auto Get_Font_WindowTitle() -> FSlateFontInfo
	{ return ck::debug_axes::ScaledFont("Bold", 14); }

	/** Deliberate outlier: the "Coming Soon" placeholder banner, sized to fill a whole page. */
	inline auto Get_Font_PlaceholderBanner() -> FSlateFontInfo
	{ return ck::debug_axes::ScaledFont("Bold", 16); }

	// ----- Row metrics ---------------------------------------------------------
	// RowDensity in attribute form: an STableRow's Padding is a TAttribute, and the inspector's
	// key/value grid slots take one too, so the axis lands on rows generated long before the flip.

	inline auto Get_TreeRowPadding() -> FMargin
	{ return ck::debug_axes::Apply_RowDensity(FMargin{0.0f, 1.0f}); }

	inline auto Get_InspectorLabelPadding() -> FMargin
	{ return ck::debug_axes::Apply_RowDensity(FMargin{0.0f, 2.0f}); }
}

// --------------------------------------------------------------------------------------------------------------------
