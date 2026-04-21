#pragma once

// --------------------------------------------------------------------------------------------------------------------
// Shared helpers for gallery section .cpp files. These live in a header (with
// `inline`) rather than per-file anonymous namespaces because UBT's unity
// build concatenates section .cpp files into one TU — anonymous-namespace
// helpers with the same name then collide as duplicate symbols.
// --------------------------------------------------------------------------------------------------------------------

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace ck::gallery
{
	/** Italic muted caption for explanatory text above each example. */
	inline auto Caption(const FString& InText) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.Text(FText::FromString(InText))
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", CkDebugStyle::FontSizeSmall()))
			.ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute()));
	}
}

// --------------------------------------------------------------------------------------------------------------------
