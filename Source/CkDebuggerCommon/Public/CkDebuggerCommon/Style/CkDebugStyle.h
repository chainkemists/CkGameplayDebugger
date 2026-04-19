#pragma once

#include "CoreMinimal.h"

// ====================================================================================================================
// Shared palette + spacing tokens for all Ck debugger UIs (GOAP, SM, future).
// Use these instead of hand-rolled FLinearColors — keeps the visual language
// consistent across debuggers and lets us retheme everything in one place.
// ====================================================================================================================

namespace CkDebugStyle
{
	// Palette color helper — the FLinearColor(FColor) ctor performs the
	// sRGB → linear conversion Slate expects. Writing raw 0..1 linear values
	// by hand makes them display way too bright, so every palette entry
	// goes through FColor byte components that match the HTML mockup hexes.
	inline auto Rgb(uint8 R, uint8 G, uint8 B) -> FLinearColor
	{
		return FLinearColor(FColor(R, G, B, 255));
	}

	// ----- Backgrounds -----
	inline const auto BgRoot       = Rgb(0x0b, 0x0e, 0x13);   // outer shell
	inline const auto Bg1          = Rgb(0x11, 0x15, 0x1c);   // panel base
	inline const auto Bg2          = Rgb(0x16, 0x1b, 0x24);   // sunken row / inset
	inline const auto Bg3          = Rgb(0x1b, 0x22, 0x30);   // floating elements

	// ----- Borders / dividers -----
	inline const auto Border       = Rgb(0x23, 0x2a, 0x38);
	inline const auto BorderStrong = Rgb(0x32, 0x39, 0x4a);

	// ----- Text -----
	inline const auto Text         = Rgb(0xe5, 0xe7, 0xed);
	inline const auto TextDim      = Rgb(0x8a, 0x92, 0xa4);
	inline const auto TextMute     = Rgb(0x5a, 0x62, 0x77);

	// ----- Semantic colors -----
	inline const auto Accent       = Rgb(0xf5, 0xc8, 0x42);   // primary focus / goal
	inline const auto Ok           = Rgb(0x55, 0xc4, 0x7a);   // success / satisfied
	inline const auto Err          = Rgb(0xd6, 0x5a, 0x5a);   // failure / unmet
	inline const auto Warn         = Rgb(0xe6, 0xa5, 0x45);   // caution / needed
	inline const auto Info         = Rgb(0x5f, 0xb3, 0xd4);   // selection / current

	// ----- Translucent overlays (for banners, selected rows) -----
	inline auto OverlayOf(const FLinearColor& InBase, float InAlpha) -> FLinearColor
	{
		auto C = InBase; C.A = InAlpha; return C;
	}

	// ----- Category colors (shared enum so SM and other debuggers can reuse) -----
	inline const auto CategoryGather   = Rgb(0x4e, 0xa8, 0x4e);
	inline const auto CategoryBuild    = Rgb(0xc2, 0x8a, 0x2a);
	inline const auto CategoryResearch = Rgb(0x5f, 0xb3, 0xd4);
	inline const auto CategoryTrain    = Rgb(0xc7, 0x4c, 0x4c);
	inline const auto CategoryAge      = Rgb(0xb4, 0x6f, 0xd0);
	inline const auto CategoryTrade    = Rgb(0xd4, 0xb1, 0x5f);

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

// ----- Tone → color helper (forward-declared in each widget header that needs it) -----
enum class ECkDebug_Tone : uint8;

namespace CkDebugStyle
{
	CKDEBUGGERCOMMON_API auto GetToneColor(ECkDebug_Tone InTone) -> FLinearColor;
}

// ====================================================================================================================
