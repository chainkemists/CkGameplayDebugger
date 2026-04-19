#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// One goal card: name + priority badge + goal conditions text.
// Compact — usable in a horizontal strip at the bottom of the macro panel.
// ====================================================================================================================

class CKGOAPDEBUGGER_API SCkGoapDebug_GoalCard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGoapDebug_GoalCard) {}
		SLATE_ARGUMENT(FCkGoapDebugger_GoalInfo, Goal)
		// Optional display-name override; falls back to Goal.ClassName when empty.
		SLATE_ARGUMENT(FString, DisplayName)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
};

// ====================================================================================================================
