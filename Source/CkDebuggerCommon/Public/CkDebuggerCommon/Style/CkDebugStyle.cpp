#include "CkDebugStyle.h"

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
		return FAppStyle::GetBrush(TEXT("RoundedSelection_16x"));
	}
}

// ====================================================================================================================
