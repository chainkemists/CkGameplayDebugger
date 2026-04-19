#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Subsection header inside an inspector body. Uppercase label + optional count.
// Smaller + quieter than the panel-level header in SCkDebug_InspectorPanel.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_SectionHeader : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkDebug_SectionHeader)
		: _Label(FText::GetEmpty())
		, _CountText(FText::GetEmpty())
	{}
		SLATE_ARGUMENT(FText, Label)
		SLATE_ARGUMENT(FText, CountText)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
};

// ====================================================================================================================
