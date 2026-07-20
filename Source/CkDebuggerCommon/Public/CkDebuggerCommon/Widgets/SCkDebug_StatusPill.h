#pragma once

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// A small pill: dot + label, tone-colored — rounded fill on the tone's dim
// surface with a tone border (the Mission Control status pill).
// Use for status badges like "Plan Found", "LIVE", "Failed" — anywhere a
// single-word state needs to stand out in a toolbar or panel header.
//
// Text and Tone are attribute-bound so live state (LIVE ↔ PAUSED, plan status)
// re-renders without a rebuild. The ECk_Tone enum lives with the style tokens
// in CkEditorTools/Style/CkStyle.h.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_StatusPill : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkDebug_StatusPill)
		: _Text(FText::GetEmpty())
		, _Tone(ECk_Tone::Neutral)
		, _ShowDot(true)
	{}
		SLATE_ATTRIBUTE(FText, Text)
		SLATE_ATTRIBUTE(ECk_Tone, Tone)
		SLATE_ARGUMENT(bool, ShowDot)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
};

// ====================================================================================================================
