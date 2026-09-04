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

	// The GroundNav field the world is publishing, as this debugger's own copy, beside the revision
	// DERIVED from the key it was captured under. Handed out by const reference because the copy is a
	// whole bake and the viewport reads the same one the panels do.
	auto Get_GroundNavField() const -> const FCkCrowdDebugger_GroundNavField& { return _GroundNavField; }
	auto Get_GroundNavRevision() const -> uint64 { return _GroundNavRevision; }
	auto Get_ShadowParity() const -> const FCkCrowdDebugger_ShadowParity& { return _ShadowParity; }

	// Flat triangle soup (3 world-space verts per triangle) of the walkable navmesh — the viewport's
	// base layer. Refreshed on a throttle (navmesh geometry changes rarely).
	auto Get_NavTriVerts() const -> const TArray<FVector>& { return _NavTriVerts; }
	auto Get_NavGeometryRevision() const -> uint64 { return _NavGeometryRevision; }
	auto Get_PathNetworkRibbons() const -> const TArray<FCkCrowdDebugger_PathNetworkRibbonSnapshot>&
	{ return _PathNetworkRibbons; }
	auto Get_Queues() const -> const TArray<FCkCrowdDebugger_QueueSnapshot>& { return _Queues; }
	auto Get_AvoidanceVolumes() const -> const TArray<FCkCrowdDebugger_AvoidanceVolumeSnapshot>&
	{ return _AvoidanceVolumes; }

private:
	auto SampleAgent(FCk_Handle InHandle, const FCk_Handle& InSelectedAgent) -> void;

private:
	TArray<FCkCrowdDebugger_AgentSnapshot> _Agents;
	FCkCrowdDebugger_NavmeshStatus _NavmeshStatus;

	FCkCrowdDebugger_GroundNavField _GroundNavField;
	bool _GroundNavFieldSampled = false;
	uint64 _GroundNavRevision = 0;
	FCkCrowdDebugger_ShadowParity _ShadowParity;

	// The copy's own cache, not the world's: the snapshot is built HERE, off a published field, and the
	// key is what stops it being rebuilt for a field that has not moved. Whole-value replacement, so a
	// reader never enumerates one bake's plates beside another's counts.
	ck::groundnav::FCk_GroundNav_DebugSnapshotCache _GroundNavCache;

	TArray<FVector> _NavTriVerts;
	TArray<FCkCrowdDebugger_PathNetworkRibbonSnapshot> _PathNetworkRibbons;
	TArray<FCkCrowdDebugger_QueueSnapshot> _Queues;
	TArray<FCkCrowdDebugger_AvoidanceVolumeSnapshot> _AvoidanceVolumes;
	// Content stamp of the last pulled navmesh geometry; the revision only bumps when it moves.
	uint32 _NavGeomSignature = 0;
	double _NavGeomLastPullTime = -1.0;
	uint64 _NavGeometryRevision = 0;

	// The player pawn's entity (when the pawn is ECS-bridged) -- SampleAgent marks the matching
	// agent row as PlayerProxy. This is the only reason the collector still resolves the pawn.
	FCk_Handle _PlayerPawnEntity;
};

// --------------------------------------------------------------------------------------------------------------------
