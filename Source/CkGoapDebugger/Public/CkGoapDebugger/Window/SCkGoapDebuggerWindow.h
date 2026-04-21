#pragma once

#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

// ====================================================================================================================

class UCkGoapDebugGraph;
class SCkGoapDebugger_WorldStatePanel;
class SCkGoapDebugger_StatsPanel;
class SCkGoapDebugger_FailureAnalysisPanel;
class SCkGoapDebug_MacroNodesPanel;
class SCkGoapDebug_PlanStrip;
class SCkGoapDebug_HistoryRail;
class SGraphEditor;
class SWidgetSwitcher;
class STextComboBox;
class STextBlock;
class SVerticalBox;

// ====================================================================================================================

class SCkGoapDebuggerWindow : public SCkDebugger_WindowBase
{
public:
	static const FName WindowId;

	SLATE_BEGIN_ARGS(SCkGoapDebuggerWindow) {}
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual auto Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

	virtual auto Get_WindowId() const -> FName override { return WindowId; }
	virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK GOAP Debugger")); }

	~SCkGoapDebuggerWindow();

private:
	auto BuildToolbar() -> TSharedRef<SWidget>;
	auto BuildTopTabs() -> TSharedRef<SWidget>;
	auto BuildRightInspectorStack() -> TSharedRef<SWidget>;
	auto RefreshEntitySelector() -> void;
	auto OnGraphSelectionChanged(const TSet<UObject*>& InSelection) -> void;
	auto OnMacroActionClicked(FString InClassName) -> void;
	auto OnPlanStepClicked(FString InClassName) -> void;
	auto RebuildDiagnosticsBanner() -> void;

private:
	TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

	TObjectPtr<UCkGoapDebugGraph> _Graph;
	TSharedPtr<SGraphEditor> _GraphEditor;

	TSharedPtr<SCkGoapDebugger_WorldStatePanel> _WorldStatePanel;
	TSharedPtr<SCkGoapDebugger_StatsPanel> _ActionDetailPanel;
	TSharedPtr<SCkGoapDebugger_FailureAnalysisPanel> _FailureAnalysisPanel;
	TSharedPtr<SCkGoapDebug_MacroNodesPanel> _MacroNodesPanel;
	TSharedPtr<SCkGoapDebug_PlanStrip> _PlanStrip;
	TSharedPtr<SCkGoapDebug_HistoryRail> _HistoryRail;

	TArray<TSharedPtr<FString>> _EntitySelectorItems;
	TArray<FCk_Handle> _EntitySelectorHandles;
	TSharedPtr<STextComboBox> _EntitySelector;
	TSharedPtr<STextBlock> _StatusBadge;

	TSharedPtr<SVerticalBox> _DiagnosticsBanner;
	uint32 _LastDiagnosticsHash = 0;

	TSharedPtr<SWidgetSwitcher> _TopTabSwitcher;
	int32 _TopTabIndex = 0;

	TWeakObjectPtr<UWorld> _CachedWorld;
};

// ====================================================================================================================
