#pragma once

#include "CkSchedulerDebugger/ViewModel/CkSchedulerDebugger_ViewModel.h"
#include "CkSchedulerDebugger/Pages/ICkSchedulerDebuggerPage.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBox.h"

// --------------------------------------------------------------------------------------------------------------------

class SCkSchedulerDebuggerWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkSchedulerDebuggerWindow) {}
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual auto Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
	auto DoBuildTopBar() -> TSharedRef<SWidget>;
	auto DoBuildStatsBar() -> TSharedRef<SWidget>;
	auto DoBuildTabBar() -> TSharedRef<SWidget>;
	auto DoSwitchToPage(int32 InPageIndex) -> void;
	auto DoFindBestWorld() const -> UWorld*;

	auto DoMakeStatItem(
		const FText& InLabel,
		TAttribute<FText> InValue,
		TAttribute<FSlateColor> InColor) -> TSharedRef<SWidget>;

private:
	TSharedPtr<FCkSchedulerDebugger_ViewModel> _ViewModel;
	TArray<TSharedPtr<ICkSchedulerDebuggerPage>> _Pages;
	int32 _ActivePageIndex = 0;
	TSharedPtr<SBox> _ContentContainer;

	TArray<TSharedPtr<FString>> _WorldOptions;
	int32 _SelectedWorldIndex = 0;
};

// --------------------------------------------------------------------------------------------------------------------
