#pragma once

#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

class UCkGoapDebugGraph;
class SCkGoapDebugger_PlanView;
class SCkGoapDebugger_WorldStatePanel;
class SCkGoapDebugger_StatsPanel;
class SGraphEditor;

// ====================================================================================================================

class SCkGoapDebuggerWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGoapDebuggerWindow) {}
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual auto Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

	~SCkGoapDebuggerWindow();

private:
	auto BuildToolbar() -> TSharedRef<SWidget>;
	auto RefreshEntitySelector() -> void;
	auto OnGraphSelectionChanged(const TSet<UObject*>& InSelection) -> void;
	auto BuildTimelineAndHistory() -> TSharedRef<SWidget>;
	auto BuildHistory() -> void;
	auto UpdateTimeline() -> void;

private:
	TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

	TObjectPtr<UCkGoapDebugGraph> _Graph;
	TSharedPtr<SGraphEditor> _GraphEditor;

	TSharedPtr<SCkGoapDebugger_PlanView> _PlanView;
	TSharedPtr<SCkGoapDebugger_WorldStatePanel> _WorldStatePanel;
	TSharedPtr<SCkGoapDebugger_StatsPanel> _ActionDetailPanel;

	TArray<TSharedPtr<FString>> _EntitySelectorItems;
	TArray<FCk_Handle> _EntitySelectorHandles;
	TSharedPtr<STextComboBox> _EntitySelector;
	TSharedPtr<STextBlock> _StatusBadge;

	TSharedPtr<SVerticalBox> _HistoryListBox;
	TSharedPtr<SBox> _TimelineBox;

	TWeakObjectPtr<UWorld> _CachedWorld;
	int32 _SelectedHistoryIndex = -1;
};

// ====================================================================================================================
