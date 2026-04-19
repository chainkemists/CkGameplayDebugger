#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Small rounded pill badge with a numeric/text value and an optional suffix label.
// Examples: "5 pre", "$3", "priority 10". No data binding — pass finished strings.
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
		SLATE_ARGUMENT(FText, ValueText)
		SLATE_ARGUMENT(FText, SuffixText)
		SLATE_ARGUMENT(FLinearColor, ValueColor)
		SLATE_ARGUMENT(FLinearColor, SuffixColor)
		SLATE_ARGUMENT(FLinearColor, BackgroundColor)
		SLATE_ARGUMENT(FLinearColor, BorderColor)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
};

// ====================================================================================================================
