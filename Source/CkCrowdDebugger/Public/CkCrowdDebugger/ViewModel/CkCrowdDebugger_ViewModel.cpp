#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_ViewModel::
	Tick(UWorld* InWorld, float InDeltaTime)
	-> void
{
	if (_IsPaused)
	{ return; }

	_DataCollector.Collect(InWorld);

	const auto Count = _DataCollector.Get_AgentCount();
	if (Count != _LastAgentCount)
	{
		_LastAgentCount = Count;
		OnAgentListChanged.Broadcast(_DataCollector.Get_AllAgents());
	}
	else
	{
		// Same count — but content may have changed (a new tag, different handle id,
		// agent destroyed and another created in the same frame). For Gate 0 we
		// always broadcast the latest list; per-tick refresh of the ListView is
		// cheap when the data is already a copyable struct array. Subsequent gates
		// can hash this and skip the broadcast on unchanged frames.
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
