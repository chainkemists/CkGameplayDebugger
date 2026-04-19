#include "CkDebugStyle.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "Styling/AppStyle.h"

// ====================================================================================================================

namespace CkDebugStyle
{
	auto GetFilledBrush() -> const FSlateBrush*
	{
		return FAppStyle::GetBrush(TEXT("GenericWhiteBox"));
	}

	auto GetRoundedBrush() -> const FSlateBrush*
	{
		// RoundedSelection_16x is not reliably registered across UE builds —
		// when missing, Slate falls back to the "missing texture" checkerboard.
		// Square corners with GenericWhiteBox render reliably everywhere.
		return FAppStyle::GetBrush(TEXT("GenericWhiteBox"));
	}

	auto GetToneColor(ECkDebug_Tone InTone) -> FLinearColor
	{
		switch (InTone)
		{
			case ECkDebug_Tone::Info:   return Info;
			case ECkDebug_Tone::Ok:     return Ok;
			case ECkDebug_Tone::Warn:   return Warn;
			case ECkDebug_Tone::Err:    return Err;
			case ECkDebug_Tone::Accent: return Accent;
			default:                    return TextDim;
		}
	}
}

// ====================================================================================================================
