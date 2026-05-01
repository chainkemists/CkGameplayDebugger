#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_ViewModel::
	Tick(UWorld* InWorld, float InDeltaTime)
	-> void
{
	_LastWorld = InWorld;

	if (_IsPaused)
	{ return; }

	_DataCollector.Collect(InWorld);

	// Only broadcast when the agent list shape actually changes. Broadcasting every
	// frame causes the AgentListPanel to rebuild its TSharedPtr<AgentSnapshot> items,
	// which invalidates SListView selection (SListView keys selection by TSharedPtr
	// identity, not by snapshot content). For Gate 0, count-change is sufficient
	// because there's no per-frame agent data. Gates 2+ will need a smarter dirty-bit
	// (e.g. hash of {position, velocity, status}) so per-agent data can refresh
	// without resetting selection — paired with selection restoration in the panel.
	const auto Count = _DataCollector.Get_AgentCount();
	if (Count != _LastAgentCount)
	{
		_LastAgentCount = Count;
		OnAgentListChanged.Broadcast(_DataCollector.Get_AllAgents());
	}

	if (ck::IsValid(_SelectedHandle))
	{
		const auto* Selected = Get_SelectedSnapshot();
		OnAgentDataRefreshed.Broadcast(Selected);
	}
	else
	{
		OnAgentDataRefreshed.Broadcast(nullptr);
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_ViewModel::
	Run_HealthCheckProbe()
	-> void
{
	_DataCollector.Run_HealthCheckProbe(_LastWorld.Get());
	// No specific delegate for health-check yet; the panel reads the status via
	// TAttribute bindings on every paint, so the next frame picks up the result.
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_ViewModel::
	Set_SelectedHandle(FCk_Handle InHandle)
	-> void
{
	if (_SelectedHandle == InHandle)
	{ return; }

	_SelectedHandle = InHandle;
	OnSelectedAgentChanged.Broadcast(_SelectedHandle);
	OnAgentDataRefreshed.Broadcast(Get_SelectedSnapshot());
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_ViewModel::
	Get_SelectedSnapshot() const
	-> const FCkCrowdDebugger_AgentSnapshot*
{
	if (NOT ck::IsValid(_SelectedHandle))
	{ return nullptr; }

	const auto& Agents = _DataCollector.Get_AllAgents();
	for (const auto& Agent : Agents)
	{
		if (Agent.Handle == _SelectedHandle)
		{ return &Agent; }
	}

	return nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_ViewModel::
	Set_Paused(bool InPaused)
	-> void
{
	if (_IsPaused == InPaused)
	{ return; }

	_IsPaused = InPaused;
	OnPausedChanged.Broadcast(_IsPaused);
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_ViewModel::
	Set_ViewMode(ECk_CrowdDebugger_ViewMode InMode)
	-> void
{
	if (_ViewMode == InMode)
	{ return; }

	_ViewMode = InMode;
	OnViewModeChanged.Broadcast(_ViewMode);
}

// --------------------------------------------------------------------------------------------------------------------
