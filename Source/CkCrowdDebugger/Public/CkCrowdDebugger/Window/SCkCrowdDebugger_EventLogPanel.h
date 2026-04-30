#pragma once

#include "Widgets/SCompoundWidget.h"

class FCkCrowdDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------

class SCkCrowdDebugger_EventLogPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkCrowdDebugger_EventLogPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FCkCrowdDebugger_ViewModel>, ViewModel)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

private:
	TSharedPtr<FCkCrowdDebugger_ViewModel> _ViewModel;
};

// --------------------------------------------------------------------------------------------------------------------
