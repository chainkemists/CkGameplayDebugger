#pragma once

#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

class SCkGoapDebugger_WorldStatePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGoapDebugger_WorldStatePanel) {}
		SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual auto Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
	auto RebuildWorldState() -> void;

private:
	TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;
	TSharedPtr<SVerticalBox> _StateListBox;
	int32 _LastWorldStateCount = -1;
};

// ====================================================================================================================
