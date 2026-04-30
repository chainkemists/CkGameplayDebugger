#pragma once

#include "Widgets/SCompoundWidget.h"

class FCkCrowdDebugger_ViewModel;
struct FCkCrowdDebugger_AgentSnapshot;

// --------------------------------------------------------------------------------------------------------------------

class SCkCrowdDebugger_AgentDetailPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkCrowdDebugger_AgentDetailPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FCkCrowdDebugger_ViewModel>, ViewModel)
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;
	virtual ~SCkCrowdDebugger_AgentDetailPanel();

private:
	auto OnAgentDataRefreshed(const FCkCrowdDebugger_AgentSnapshot* InSnapshot) -> void;
	auto Get_BodyText() const -> FText;

private:
	TSharedPtr<FCkCrowdDebugger_ViewModel> _ViewModel;
	FString _CachedBody;
	FDelegateHandle _OnRefreshedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
