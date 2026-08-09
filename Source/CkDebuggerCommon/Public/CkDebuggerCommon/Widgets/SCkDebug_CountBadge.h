#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Small rounded pill badge with a numeric/text value and an optional suffix label.
// Examples: "5 pre", "$3", "priority 10". The value is attribute-bound, so a live count follows
// its source without a rebuild; the suffix is a fixed label chosen when the badge is composed.
//
// The BadgeStyle axis drives the chrome, live via attributes: Solid (default) is ring + fill
// as shipped, Hollow keeps the ring and drops the fill, CountOnly drops both and leaves the
// bare toned value/suffix. The colors stay caller-supplied — the axis is colorless.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_CountBadge : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkDebug_CountBadge)
		: _ValueText(FText::GetEmpty())
		, _SuffixText(FText::GetEmpty())
		, _ValueColor(FLinearColor(0.9f, 0.9f, 0.9f))
		, _SuffixColor(FLinearColor(0.55f, 0.55f, 0.6f))
		, _BackgroundColor(FLinearColor(0.05f, 0.07f, 0.1f))
		, _BorderColor(FLinearColor(0.18f, 0.20f, 0.24f))
	{}
		SLATE_ATTRIBUTE(FText, ValueText)
		SLATE_ARGUMENT(FText, SuffixText)
		SLATE_ARGUMENT(FLinearColor, ValueColor)
		SLATE_ARGUMENT(FLinearColor, SuffixColor)
		SLATE_ARGUMENT(FLinearColor, BackgroundColor)
		SLATE_ARGUMENT(FLinearColor, BorderColor)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
};

// ====================================================================================================================
