#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Subsection header inside an inspector body. Uppercase label + optional count.
// Smaller + quieter than the panel-level header in SCkDebug_InspectorPanel.
//
// Panel-header mode (the mockup's ".ph" row): set Underline(true) and it gains
// a bottom hairline; SubText renders a muted lowercase clarifier after the
// label ("what the agent believes"); RightContent hosts trailing controls
// (a Sandbox switch, an Edit button, a live count badge).
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_SectionHeader : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkDebug_SectionHeader)
		: _Label(FText::GetEmpty())
		, _CountText(FText::GetEmpty())
		, _SubText(FText::GetEmpty())
		, _Underline(false)
	{}
		SLATE_ARGUMENT(FText, Label)
		SLATE_ARGUMENT(FText, CountText)
		// Muted, non-uppercase clarifier after the label.
		SLATE_ARGUMENT(FText, SubText)
		// Widget clarifier after the label — takes precedence over SubText.
		// Host an SCkDebug_NameLabel here when the clarifier is a class/tag name.
		SLATE_NAMED_SLOT(FArguments, SubContent)
		// true → panel-header treatment: padding + bottom hairline.
		SLATE_ARGUMENT(bool, Underline)
		// Right-aligned trailing controls (switches, buttons, badges).
		SLATE_NAMED_SLOT(FArguments, RightContent)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
};

// ====================================================================================================================
