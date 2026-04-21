#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Compact toolbar widget: two dropdowns letting the user override refresh
// mode + rate cap for a specific debugger window. Reads/writes
// UCkDebuggerWindowSettings via GetMutableDefault + SaveConfig.
//
// The dropdowns show "Global (resolved: X)" for the UseGlobal entry so it's
// always obvious what the current effective value is.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebugger_RefreshControls : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkDebugger_RefreshControls) {}
		SLATE_ARGUMENT(FName, WindowId)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

private:
	auto Build_ModeDropdown() -> TSharedRef<SWidget>;
	auto Build_RateDropdown() -> TSharedRef<SWidget>;

	FName _WindowId;
};

// ====================================================================================================================
