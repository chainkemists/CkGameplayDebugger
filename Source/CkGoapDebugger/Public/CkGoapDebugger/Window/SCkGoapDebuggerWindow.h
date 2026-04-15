#pragma once

#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

class SCkGoapDebugger_PlanView;
class SCkGoapDebugger_WorldStatePanel;
class SCkGoapDebugger_GoalPanel;
class SCkGoapDebugger_StatsPanel;

// ====================================================================================================================

class SCkGoapDebuggerWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGoapDebuggerWindow) {}
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual auto Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
	auto BuildToolbar() -> TSharedRef<SWidget>;
	auto RefreshEntitySelector() -> void;

private:
	TSharedPtr<FCkGoapDebugger_ViewModel> _ViewModel;

	TSharedPtr<SCkGoapDebugger_PlanView> _PlanView;
	TSharedPtr<SCkGoapDebugger_WorldStatePanel> _WorldStatePanel;
	TSharedPtr<SCkGoapDebugger_GoalPanel> _GoalPanel;
	TSharedPtr<SCkGoapDebugger_StatsPanel> _StatsPanel;

	TArray<TSharedPtr<FString>> _EntitySelectorItems;
	TArray<FCk_Handle> _EntitySelectorHandles;
	TSharedPtr<STextComboBox> _EntitySelector;
	TSharedPtr<STextBlock> _StatusBadge;

	TWeakObjectPtr<UWorld> _CachedWorld;
};

// ====================================================================================================================
