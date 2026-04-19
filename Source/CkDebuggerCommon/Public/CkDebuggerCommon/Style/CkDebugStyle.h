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

	CKDEBUGGERCOMMON_API auto Accent()       -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Ok()           -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Err()          -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Warn()         -> FLinearColor;
	CKDEBUGGERCOMMON_API auto Info()         -> FLinearColor;

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

	// ----- Graph node visuals -------------------------------------------------
	CKDEBUGGERCOMMON_API auto NodeFill_Inactive()     -> FLinearColor;
	CKDEBUGGERCOMMON_API auto NodeBorder_Inactive()   -> FLinearColor;
	CKDEBUGGERCOMMON_API auto NodeFill_InPlan()       -> FLinearColor;
	CKDEBUGGERCOMMON_API auto NodeBorder_InPlan()     -> FLinearColor;
	CKDEBUGGERCOMMON_API auto NodeFill_Goal()         -> FLinearColor;
	CKDEBUGGERCOMMON_API auto NodeBorder_Goal()       -> FLinearColor;
	CKDEBUGGERCOMMON_API auto NodeFill_GoalInactive() -> FLinearColor;
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
