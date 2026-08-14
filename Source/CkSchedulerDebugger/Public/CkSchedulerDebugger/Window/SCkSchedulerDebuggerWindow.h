#pragma once

#include "CkSchedulerDebugger/ViewModel/CkSchedulerDebugger_ViewModel.h"
#include "CkSchedulerDebugger/Pages/ICkSchedulerDebuggerPage.h"

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"
#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CommandBar.h"

#include "Widgets/Layout/SBox.h"

class SCkDebug_FrameStrip;

// --------------------------------------------------------------------------------------------------------------------

class SCkSchedulerDebuggerWindow : public SCkDebugger_WindowBase
{
public:
	static const FName WindowId;

	SLATE_BEGIN_ARGS(SCkSchedulerDebuggerWindow) {}
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual auto Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

	virtual auto Get_WindowId() const -> FName override { return WindowId; }
	virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("Scheduler")); }

protected:
	virtual auto OnStyleRevisionChanged() -> void override;

private:
	auto DoBuildCommandGroups() -> TArray<FCkDebug_CommandGroup>;
	auto DoBuildStatsBar() -> TSharedRef<SWidget>;
	auto DoBuildTabBar() -> TSharedRef<SWidget>;
	auto DoSwitchToPage(int32 InPageIndex) -> void;

	auto DoMakeStatItem(
		const FText& InLabel,
		TAttribute<FText> InValue,
		TAttribute<FSlateColor> InColor) -> TSharedRef<SWidget>;

	auto DoGetFrameHistoryMaxSize() const -> int32 { return _FrameHistoryMaxSize; }

	// Rebuilds the frame strip's sample array from the collector. The strip is immediate-mode and
	// owns no data, so this is the whole feed. InForce bypasses the unchanged-history early-out —
	// used when the highlight filter changes but the frame set did not.
	auto DoPushFrameSamples(bool InForce) -> void;

	/** One clipboard line describing the frame the strip currently has selected. */
	auto DoComposeSelectedFrameText() const -> FString;

private:
	TSharedPtr<FCkSchedulerDebugger_ViewModel> _ViewModel;
	TArray<TSharedPtr<ICkSchedulerDebuggerPage>> _Pages;
	int32 _ActivePageIndex = 0;
	TSharedPtr<SBox> _ContentContainer;
	TSharedPtr<SCkDebug_FrameStrip> _FrameStrip;

	// Highlight-filter state moved from the retired FrameHistoryBar: the strip renders the verdict,
	// the window owns the query and answers it.
	FString _HighlightFilter;

	// A frame's "did it run a matching processor" answer never changes once taken, so surviving
	// frames keep theirs across pushes instead of re-scanning every snapshot's processor list.
	TMap<uint64, bool> _HighlightVerdictByFrame;
	int32 _LastPushedSampleCount = INDEX_NONE;
	uint64 _LastPushedNewestFrame = 0;

	TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;
	int32 _FrameHistoryMaxSize = 3000;
	TWeakObjectPtr<UWorld> _CurrentWorld;
};

// --------------------------------------------------------------------------------------------------------------------
