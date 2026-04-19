#pragma once

#include "Widgets/SCompoundWidget.h"

class SWidgetSwitcher;

// ====================================================================================================================
// A vertical column with:
//   - a clickable header (title, optional pill, chevron)
//   - an Expanded body (caller-provided content)
//   - a Collapsed summary (caller-provided content, e.g. "12 hidden")
//
// The column owns its expanded/collapsed state. Consumers can observe via
// OnExpansionChanged.
// ====================================================================================================================

DECLARE_DELEGATE_OneParam(FOnCkDebugColumnExpansionChanged, bool /*bIsExpanded*/);

class CKDEBUGGERCOMMON_API SCkDebug_ExpandableColumn : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkDebug_ExpandableColumn)
		: _Title(FText::GetEmpty())
		, _PillText(FText::GetEmpty())
		, _StartExpanded(true)
		, _TitleColor(FLinearColor(0.88f, 0.88f, 0.92f))
		, _BorderColor(FLinearColor(0.15f, 0.17f, 0.22f))
		, _BackgroundColor(FLinearColor(0.075f, 0.09f, 0.12f))
		, _HeaderBackgroundColor(FLinearColor(0.05f, 0.07f, 0.10f))
	{}
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(FText, PillText)
		SLATE_ARGUMENT(bool, StartExpanded)
		SLATE_ARGUMENT(FLinearColor, TitleColor)
		SLATE_ARGUMENT(FLinearColor, BorderColor)
		SLATE_ARGUMENT(FLinearColor, BackgroundColor)
		SLATE_ARGUMENT(FLinearColor, HeaderBackgroundColor)
		SLATE_EVENT(FOnCkDebugColumnExpansionChanged, OnExpansionChanged)

		SLATE_NAMED_SLOT(FArguments, ExpandedBody)
		SLATE_NAMED_SLOT(FArguments, CollapsedSummary)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

	auto Set_Expanded(bool InExpanded) -> void;
	auto Is_Expanded() const -> bool { return _IsExpanded; }

private:
	auto OnHeaderClicked() -> FReply;

private:
	bool _IsExpanded = true;
	TSharedPtr<SWidgetSwitcher> _BodySwitcher;
	TSharedPtr<class STextBlock> _ChevronText;
	FOnCkDebugColumnExpansionChanged _OnExpansionChanged;
};

// ====================================================================================================================
