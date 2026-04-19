#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// A small pill: dot + label, tone-colored.
// Use for status badges like "Plan Found", "Simulating", "Failed" — anywhere
// a single-word state needs to stand out in a toolbar or panel header.
// ====================================================================================================================

enum class ECkDebug_Tone : uint8
{
	Neutral,
	Info,
	Ok,
	Warn,
	Err,
	Accent,
};

class CKDEBUGGERCOMMON_API SCkDebug_StatusPill : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkDebug_StatusPill)
		: _Text(FText::GetEmpty())
		, _Tone(ECkDebug_Tone::Neutral)
		, _ShowDot(true)
	{}
		SLATE_ARGUMENT(FText, Text)
		SLATE_ARGUMENT(ECkDebug_Tone, Tone)
		SLATE_ARGUMENT(bool, ShowDot)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
};

// ====================================================================================================================
