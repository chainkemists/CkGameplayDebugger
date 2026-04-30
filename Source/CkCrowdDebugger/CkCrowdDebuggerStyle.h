#pragma once

#include "Math/Color.h"

// --------------------------------------------------------------------------------------------------------------------
// Slate-side style tokens for the Crowd Debugger. Approximate the static UX
// mockup colors documented in the PLAN.md mockup folder. Inline constants only —
// no FSlateStyleSet, matching the CkGoapDebugger pattern.
// --------------------------------------------------------------------------------------------------------------------

namespace CkCrowdDebuggerStyle
{
	// Status colors
	inline const auto StatusOk           = FLinearColor{0.31f, 0.79f, 0.49f, 1.0f};   // green — Walking / OK
	inline const auto StatusWarn         = FLinearColor{0.90f, 0.73f, 0.29f, 1.0f};   // yellow — Replan
	inline const auto StatusError        = FLinearColor{0.89f, 0.39f, 0.39f, 1.0f};   // red — Failed
	inline const auto StatusInfo         = FLinearColor{0.42f, 0.71f, 0.89f, 1.0f};   // cyan — Live (proxy)
	inline const auto StatusAsleep       = FLinearColor{0.56f, 0.56f, 0.56f, 1.0f};   // grey — Asleep / dim
	inline const auto AccentSelected     = FLinearColor{1.00f, 0.61f, 0.25f, 1.0f};   // orange — selection
	inline const auto AccentSelectedDim  = FLinearColor{0.71f, 0.43f, 0.17f, 1.0f};

	// Layout
	constexpr auto PanelPadding   = 12.0f;
	constexpr auto SectionSpacing = 16.0f;
	constexpr auto RowHeight      = 18.0f;
}

// --------------------------------------------------------------------------------------------------------------------
