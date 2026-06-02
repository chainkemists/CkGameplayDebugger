#pragma once

#include "CkSchedulerDebugger/ViewModel/CkSchedulerDebugger_ViewModel.h"
#include "CkSchedulerDebugger/Pages/ICkSchedulerDebuggerPage.h"

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"
#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"

#include "Widgets/Layout/SBox.h"

class SCkSchedulerDebugger_FrameHistoryBar;

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
	virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK Scheduler Debugger")); }

private:
	auto DoBuildTopBar() -> TSharedRef<SWidget>;
	auto DoBuildStatsBar() -> TSharedRef<SWidget>;
	auto DoBuildTabBar() -> TSharedRef<SWidget>;
	auto DoSwitchToPage(int32 InPageIndex) -> void;

	auto DoMakeStatItem(
		const FText& InLabel,
		TAttribute<FText> InValue,
		TAttribute<FSlateColor> InColor) -> TSharedRef<SWidget>;

	auto DoGetFrameHistoryMaxSize() const -> int32 { return _FrameHistoryMaxSize; }

private:
	TSharedPtr<FCkSchedulerDebugger_ViewModel> _ViewModel;
	TArray<TSharedPtr<ICkSchedulerDebuggerPage>> _Pages;
	int32 _ActivePageIndex = 0;
	TSharedPtr<SBox> _ContentContainer;
	TSharedPtr<SCkSchedulerDebugger_FrameHistoryBar> _FrameHistoryBar;

	TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;
	int32 _FrameHistoryMaxSize = 3000;
	TWeakObjectPtr<UWorld> _CurrentWorld;
};

// --------------------------------------------------------------------------------------------------------------------
