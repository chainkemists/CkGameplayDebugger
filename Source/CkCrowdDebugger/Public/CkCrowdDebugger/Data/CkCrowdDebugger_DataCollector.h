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
	// InSelectedAgent decides which agent's planned-path POINTS are carried; every agent still
	// reports its point count. Pass an invalid handle to carry none.
	auto Collect(UWorld* InWorld, const FCk_Handle& InSelectedAgent) -> void;
	auto Reset_ForWorldChange() -> void;

	// Run a synthetic FindPathSync probe (origin → origin+200 in world space) and
	// stash the result on _NavmeshStatus. Called by the ViewModel from the toolbar
	// button. Bypasses the request/processor pipeline entirely so a green probe
	// proves the nav stack works in isolation from any gym wiring.
	auto Run_HealthCheckProbe(UWorld* InWorld) -> void;

	auto Get_AllAgents() const -> const TArray<FCkCrowdDebugger_AgentSnapshot>& { return _Agents; }
	auto Get_AgentCount() const -> int32 { return _Agents.Num(); }
	auto Get_NavmeshStatus() const -> const FCkCrowdDebugger_NavmeshStatus& { return _NavmeshStatus; }

	// Flat triangle soup (3 world-space verts per triangle) of the walkable navmesh — the viewport's
	// base layer. Refreshed on a throttle (navmesh geometry changes rarely).
	auto Get_NavTriVerts() const -> const TArray<FVector>& { return _NavTriVerts; }
	auto Get_NavGeometryRevision() const -> uint64 { return _NavGeometryRevision; }
	auto Get_PathNetworkRibbons() const -> const TArray<FCkCrowdDebugger_PathNetworkRibbonSnapshot>&
	{ return _PathNetworkRibbons; }
	auto Get_Queues() const -> const TArray<FCkCrowdDebugger_QueueSnapshot>& { return _Queues; }

private:
	auto SampleAgent(FCk_Handle InHandle, const FCk_Handle& InSelectedAgent) -> void;

private:
	TArray<FCkCrowdDebugger_AgentSnapshot> _Agents;
	FCkCrowdDebugger_NavmeshStatus _NavmeshStatus;

	TArray<FVector> _NavTriVerts;
	TArray<FCkCrowdDebugger_PathNetworkRibbonSnapshot> _PathNetworkRibbons;
	TArray<FCkCrowdDebugger_QueueSnapshot> _Queues;
	// Content stamp of the last pulled navmesh geometry; the revision only bumps when it moves.
	uint32 _NavGeomSignature = 0;
	double _NavGeomLastPullTime = -1.0;
	uint64 _NavGeometryRevision = 0;

	// The player pawn's entity (when the pawn is ECS-bridged) -- SampleAgent marks the matching
	// agent row as PlayerProxy. This is the only reason the collector still resolves the pawn.
	FCk_Handle _PlayerPawnEntity;
};

// --------------------------------------------------------------------------------------------------------------------
