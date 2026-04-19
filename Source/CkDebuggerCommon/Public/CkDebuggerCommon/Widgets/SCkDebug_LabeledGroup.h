#pragma once

#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

// ====================================================================================================================
// Vertical stack with a header row (optional dot + label + right-aligned count)
// and a vertical body.
// Body children are added after Construct via AddSlot() / SetSlots().
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_LabeledGroup : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkDebug_LabeledGroup)
		: _Label(FText::GetEmpty())
		, _CountText(FText::GetEmpty())
		, _DotColor(FLinearColor::Transparent)
		, _LabelColor(FLinearColor(0.54f, 0.57f, 0.65f))
		, _BorderColor(FLinearColor(0.13f, 0.16f, 0.22f))
		, _BackgroundColor(FLinearColor(0.06f, 0.08f, 0.11f))
		, _HeaderBackgroundColor(FLinearColor(0.04f, 0.05f, 0.08f))
	{}
		SLATE_ARGUMENT(FText, Label)
		SLATE_ARGUMENT(FText, CountText)
		SLATE_ARGUMENT(FLinearColor, DotColor)
		SLATE_ARGUMENT(FLinearColor, LabelColor)
		SLATE_ARGUMENT(FLinearColor, BorderColor)
		SLATE_ARGUMENT(FLinearColor, BackgroundColor)
		SLATE_ARGUMENT(FLinearColor, HeaderBackgroundColor)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

	// Add a child widget to the body.
	auto AddChild(TSharedRef<SWidget> InWidget) -> void;

	// Remove all body children.
	auto ClearChildren() -> void;

private:
	TSharedPtr<SVerticalBox> _Body;
};

// ====================================================================================================================
