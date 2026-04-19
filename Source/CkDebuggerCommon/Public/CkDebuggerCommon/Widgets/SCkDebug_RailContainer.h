#pragma once

#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class STextBlock;

// ====================================================================================================================
// A sidebar container widget: styled top header (label + count) with a
// scrollable vertical body beneath. Children appended via AddChild.
// Used for plan history rail, and anywhere a "rail of rows" is needed.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_RailContainer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkDebug_RailContainer)
		: _Title(FText::GetEmpty())
		, _CountText(FText::GetEmpty())
	{}
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(FText, CountText)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

	auto AddChild(TSharedRef<SWidget> InWidget) -> void;
	auto ClearChildren() -> void;
	auto Set_CountText(const FText& InText) -> void;

private:
	TSharedPtr<SVerticalBox> _Body;
	TSharedPtr<STextBlock> _CountBadge;
};

// ====================================================================================================================
