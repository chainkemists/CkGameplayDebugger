#pragma once

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"
#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"

#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkCrowdDebugger_ViewModel;

class SCkCrowdDebugger_NavmeshStatusPanel;
class SCkCrowdDebugger_AgentListPanel;
class SCkCrowdDebugger_AgentDetailPanel;
class SCkCrowdDebugger_StatsPanel;
class SCkCrowdDebugger_EventLogPanel;
class SCkCrowdDebugger_ViewportPanel;

// --------------------------------------------------------------------------------------------------------------------

class CKCROWDDEBUGGER_API SCkCrowdDebuggerWindow : public SCkDebugger_WindowBase
{
public:
	static const FName WindowId;

	SLATE_BEGIN_ARGS(SCkCrowdDebuggerWindow) {}
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void;

	virtual auto Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

	virtual auto Get_WindowId() const -> FName override { return WindowId; }
	virtual auto Get_WindowDisplayName() const -> FText override
	{ return FText::FromString(TEXT("CK Crowd Debugger")); }

	virtual ~SCkCrowdDebuggerWindow();

private:
	auto BuildToolbar() -> TSharedRef<SWidget>;

private:
	TSharedPtr<FCkCrowdDebugger_ViewModel> _ViewModel;
	TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;

	TSharedPtr<SCkCrowdDebugger_NavmeshStatusPanel> _NavmeshStatusPanel;
	TSharedPtr<SCkCrowdDebugger_AgentListPanel>     _AgentListPanel;
	TSharedPtr<SCkCrowdDebugger_AgentDetailPanel>   _AgentDetailPanel;
	TSharedPtr<SCkCrowdDebugger_StatsPanel>         _StatsPanel;
	TSharedPtr<SCkCrowdDebugger_EventLogPanel>      _EventLogPanel;
	TSharedPtr<SCkCrowdDebugger_ViewportPanel>      _ViewportPanel;
};

// --------------------------------------------------------------------------------------------------------------------
