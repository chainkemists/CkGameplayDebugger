#include "CkDebuggerStyleSettings.h"

// ====================================================================================================================

namespace
{
	auto DoRgb(uint8 R, uint8 G, uint8 B) -> FLinearColor
	{
		return FLinearColor(FColor(R, G, B, 255));
	}
}

// ====================================================================================================================

UCkDebuggerStyleSettings::UCkDebuggerStyleSettings()
{
	// Backgrounds — near-black navy, each tier slightly lighter than the last.
	BgRoot = DoRgb(0x0b, 0x0e, 0x13);
	Bg1    = DoRgb(0x11, 0x15, 0x1c);
	Bg2    = DoRgb(0x16, 0x1b, 0x24);
	Bg3    = DoRgb(0x1b, 0x22, 0x30);

	// Borders
	Border       = DoRgb(0x23, 0x2a, 0x38);
	BorderStrong = DoRgb(0x32, 0x39, 0x4a);

	// Text
	Text     = DoRgb(0xe5, 0xe7, 0xed);
	TextDim  = DoRgb(0x8a, 0x92, 0xa4);
	TextMute = DoRgb(0x5a, 0x62, 0x77);

	// Semantic
	Accent = DoRgb(0xf5, 0xc8, 0x42);
	Ok     = DoRgb(0x55, 0xc4, 0x7a);
	Err    = DoRgb(0xd6, 0x5a, 0x5a);
	Warn   = DoRgb(0xe6, 0xa5, 0x45);
	Info   = DoRgb(0x5f, 0xb3, 0xd4);

	// Action categories
	CategoryGather   = DoRgb(0x4e, 0xa8, 0x4e);
	CategoryBuild    = DoRgb(0xc2, 0x8a, 0x2a);
	CategoryResearch = DoRgb(0x5f, 0xb3, 0xd4);
	CategoryTrain    = DoRgb(0xc7, 0x4c, 0x4c);
	CategoryAge      = DoRgb(0xb4, 0x6f, 0xd0);
	CategoryTrade    = DoRgb(0xd4, 0xb1, 0x5f);

	// Typography — intentionally smaller than the mockup's CSS pixel sizes
	// because UE Slate renders bolds that read larger at equivalent points.
	FontSizeH2    = 13;
	FontSizeH3    = 11;
	FontSizeH4    = 9;
	FontSizeBody  = 10;
	FontSizeSmall = 9;
	FontSizeMicro = 8;

	// Graph nodes — opaque fills give the "depth" look the grey nodes had in
	// the screenshot. Translucent tints read as flat.
	NodeFill_Inactive    = Bg2;
	NodeBorder_Inactive  = Border;
	NodeFill_InPlan      = DoRgb(0x14, 0x23, 0x35);   // opaque dark info blue
	NodeBorder_InPlan    = Info;
	NodeFill_Goal        = DoRgb(0x2a, 0x22, 0x0a);   // opaque dark amber
	NodeBorder_Goal      = Accent;
	NodeFill_GoalInactive= DoRgb(0x15, 0x13, 0x08);

	NodeBorderThickness = 1.5f;
	NodeInactiveOpacity = 0.55f;
}

// ====================================================================================================================
