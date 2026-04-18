#pragma once

#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

class UCkGoapDebugGraph;
class SCkGoapDebugger_WorldStatePanel;
class SCkGoapDebugger_StatsPanel;
class SCkGoapDebugger_FailureAnalysisPanel;
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
	auto RebuildPlanStrip() -> void;
	auto RebuildDiagnosticsBanner() -> void;

private:
	TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

	TObjectPtr<UCkGoapDebugGraph> _Graph;
	TSharedPtr<SGraphEditor> _GraphEditor;

	TSharedPtr<SCkGoapDebugger_WorldStatePanel> _WorldStatePanel;
	TSharedPtr<SCkGoapDebugger_StatsPanel> _ActionDetailPanel;
	TSharedPtr<SCkGoapDebugger_FailureAnalysisPanel> _FailureAnalysisPanel;

	TArray<TSharedPtr<FString>> _EntitySelectorItems;
	TArray<FCk_Handle> _EntitySelectorHandles;
	TSharedPtr<STextComboBox> _EntitySelector;
	TSharedPtr<STextBlock> _StatusBadge;

	TSharedPtr<SVerticalBox> _HistoryListBox;
	TSharedPtr<SHorizontalBox> _PlanStripBox;
	uint32 _LastPlanStripHash = 0;

	TSharedPtr<SVerticalBox> _DiagnosticsBanner;
	uint32 _LastDiagnosticsHash = 0;

	TWeakObjectPtr<UWorld> _CachedWorld;
	int32 _SelectedHistoryIndex = -1;
	int32 _LastHistoryCount = -1;
	uint32 _LastHistoryHash = 0;
};

// ====================================================================================================================
