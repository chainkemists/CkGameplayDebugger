#pragma once

#include "CkCrowdDebugger/Data/CkCrowdDebugger_Types.h"

class UWorld;

// --------------------------------------------------------------------------------------------------------------------
// Plain C++ class (not UObject). Per-frame Collect(UWorld*) walks the ECS
// registry, builds an array of AgentSnapshot copies, samples the navmesh
// status. The ViewModel owns one of these and ticks it.
// --------------------------------------------------------------------------------------------------------------------

class FCkCrowdDebugger_DataCollector
{
public:
	auto Collect(UWorld* InWorld) -> void;

	auto Get_AllAgents() const -> const TArray<FCkCrowdDebugger_AgentSnapshot>& { return _Agents; }
	auto Get_AgentCount() const -> int32 { return _Agents.Num(); }
	auto Get_NavmeshStatus() const -> const FCkCrowdDebugger_NavmeshStatus& { return _NavmeshStatus; }

private:
	auto SampleAgent(FCk_Handle InHandle) -> void;

private:
	TArray<FCkCrowdDebugger_AgentSnapshot> _Agents;
	FCkCrowdDebugger_NavmeshStatus _NavmeshStatus;
};

// --------------------------------------------------------------------------------------------------------------------
