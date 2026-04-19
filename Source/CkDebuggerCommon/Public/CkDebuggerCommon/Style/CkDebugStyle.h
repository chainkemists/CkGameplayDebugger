#pragma once

#include "CoreMinimal.h"

// ====================================================================================================================
// Shared palette + spacing tokens for all Ck debugger UIs (GOAP, SM, future).
// Use these instead of hand-rolled FLinearColors — keeps the visual language
// consistent across debuggers and lets us retheme everything in one place.
// ====================================================================================================================

namespace CkDebugStyle
{
	// ----- Backgrounds -----
	inline const auto BgRoot       = FLinearColor(0.043f, 0.055f, 0.075f);   // #0b0e13 — outer shell
	inline const auto Bg1          = FLinearColor(0.067f, 0.083f, 0.110f);   // #11151c — panel base
	inline const auto Bg2          = FLinearColor(0.086f, 0.106f, 0.141f);   // #161b24 — sunken row / inset
	inline const auto Bg3          = FLinearColor(0.106f, 0.133f, 0.188f);   // #1b2230 — floating elements

	// ----- Borders / dividers -----
	inline const auto Border       = FLinearColor(0.137f, 0.165f, 0.220f);   // #232a38
	inline const auto BorderStrong = FLinearColor(0.196f, 0.224f, 0.290f);   // #32394a

	// ----- Text -----
	inline const auto Text         = FLinearColor(0.898f, 0.906f, 0.929f);   // #e5e7ed — primary
	inline const auto TextDim      = FLinearColor(0.541f, 0.572f, 0.643f);   // #8a92a4 — secondary
	inline const auto TextMute     = FLinearColor(0.353f, 0.384f, 0.467f);   // #5a6277 — placeholder / meta

	// ----- Semantic colors -----
	inline const auto Accent       = FLinearColor(0.961f, 0.784f, 0.259f);   // #f5c842 — primary focus / goal
	inline const auto Ok           = FLinearColor(0.333f, 0.769f, 0.478f);   // #55c47a — success / satisfied
	inline const auto Err          = FLinearColor(0.839f, 0.353f, 0.353f);   // #d65a5a — failure / unmet
	inline const auto Warn         = FLinearColor(0.902f, 0.647f, 0.271f);   // #e6a545 — caution / needed
	inline const auto Info         = FLinearColor(0.373f, 0.702f, 0.831f);   // #5fb3d4 — selection / current

	// ----- Translucent overlays (for banners, selected rows) -----
	inline auto OverlayOf(const FLinearColor& InBase, float InAlpha) -> FLinearColor
	{
		auto C = InBase; C.A = InAlpha; return C;
	}

	// ----- Category colors (shared enum so SM and other debuggers can reuse) -----
	inline const auto CategoryGather   = FLinearColor(0.31f, 0.66f, 0.31f);
	inline const auto CategoryBuild    = FLinearColor(0.76f, 0.54f, 0.16f);
	inline const auto CategoryResearch = FLinearColor(0.37f, 0.70f, 0.83f);
	inline const auto CategoryTrain    = FLinearColor(0.78f, 0.30f, 0.30f);
	inline const auto CategoryAge      = FLinearColor(0.71f, 0.44f, 0.82f);
	inline const auto CategoryTrade    = FLinearColor(0.83f, 0.69f, 0.37f);

	// ----- Spacing -----
	constexpr auto SpaceXS  = 2.0f;
	constexpr auto SpaceS   = 4.0f;
	constexpr auto SpaceM   = 8.0f;
	constexpr auto SpaceL   = 12.0f;
	constexpr auto SpaceXL  = 16.0f;
	constexpr auto SpaceXXL = 24.0f;

	// ----- Font sizes (pt) — keep a small vocabulary -----
	constexpr auto FontSizeH2   = 14;
	constexpr auto FontSizeH3   = 12;   // inspector panel header
	constexpr auto FontSizeH4   = 10;   // section header (uppercase)
	constexpr auto FontSizeBody = 11;
	constexpr auto FontSizeSmall= 9;
	constexpr auto FontSizeMicro= 8;

	// ----- Common brushes (cached lookups) -----
	CKDEBUGGERCOMMON_API auto GetFilledBrush() -> const FSlateBrush*; // solid white, tintable
	CKDEBUGGERCOMMON_API auto GetRoundedBrush() -> const FSlateBrush*; // rounded selection / pill
}

// ====================================================================================================================
