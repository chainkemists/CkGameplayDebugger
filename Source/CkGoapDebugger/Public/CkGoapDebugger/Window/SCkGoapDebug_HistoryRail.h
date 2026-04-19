#pragma once

#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "Widgets/SCompoundWidget.h"

class SCkDebug_RailContainer;
class UCkGoapDebugGraph;

// ====================================================================================================================
// Plan history left-rail. Subscribes to the ViewModel; each history entry
// becomes an SCkDebug_HistoryRow. Selecting a row enters scrub mode.
// ====================================================================================================================

DECLARE_DELEGATE(FOnCkGoapDebug_HistoryScrubChanged);

class CKGOAPDEBUGGER_API SCkGoapDebug_HistoryRail : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGoapDebug_HistoryRail)
		: _Graph(nullptr)
	{}
		SLATE_ARGUMENT(TSharedPtr<FCkGoapDebugger_ViewModel>, ViewModel)
		SLATE_ARGUMENT(UCkGoapDebugGraph*, Graph)
		SLATE_EVENT(FOnCkGoapDebug_HistoryScrubChanged, OnScrubChanged)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual auto Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
	auto RebuildRows() -> void;

private:
	TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;
	TWeakObjectPtr<UCkGoapDebugGraph> _Graph;
	FOnCkGoapDebug_HistoryScrubChanged _OnScrubChanged;

	TSharedPtr<SCkDebug_RailContainer> _Rail;
	uint32 _LastHash = 0;
};

// ====================================================================================================================
