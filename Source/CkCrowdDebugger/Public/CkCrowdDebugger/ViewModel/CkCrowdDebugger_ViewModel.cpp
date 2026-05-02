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

	// Gate 3 — broadcast every frame so the agent-list rows reflect live neighbor counts,
	// status, etc. The Gate 0 count-change-only gate left per-agent fields stale (n= column
	// stuck at 0 even after NeighborSync populated the cache). The AgentListPanel already
	// restores selection by handle after rebuild (see OnAgentListChanged), so per-frame
	// broadcast is safe; cost is rebuilding ~N TSharedPtr items per tick at typical N≤150.
	OnAgentListChanged.Broadcast(_DataCollector.Get_AllAgents());
	_LastAgentCount = _DataCollector.Get_AgentCount();

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
