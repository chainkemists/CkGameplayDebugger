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

	// Local-player view and pose. Every accessor below is still collected each refresh but has had
	// NO reader since the viewport became 3D (d2a71bd): the 2D map oriented itself to the view yaw
	// and drew the FOV wedge and player chevron. The 3D camera is user-driven and draws neither.
	// (_PlayerPawnEntity is not part of this -- it is live; see its own note below.)
	auto Get_ViewYawDegrees() const -> float { return _ViewYawDegrees; }
	auto Get_ViewYawValid() const -> bool { return _ViewYawValid; }

	// View camera position: the level-editor camera while ejected, else the possessed view point.
	auto Get_ViewCameraPosition() const -> FVector { return _ViewCameraPosition; }
	auto Get_ViewCameraValid() const -> bool { return _ViewCameraValid; }

	// Player pawn pose. Stays valid while ejected (the pawn is still in the world, just not driven).
	auto Get_PlayerPawnPosition() const -> FVector { return _PlayerPawnPosition; }
	auto Get_PlayerPawnYawDegrees() const -> float { return _PlayerPawnYawDegrees; }
	auto Get_PlayerPawnValid() const -> bool { return _PlayerPawnValid; }

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

	float _ViewYawDegrees = 0.0f;
	bool  _ViewYawValid = false;

	FVector _ViewCameraPosition = FVector::ZeroVector;
	bool    _ViewCameraValid = false;

	FVector _PlayerPawnPosition = FVector::ZeroVector;
	float   _PlayerPawnYawDegrees = 0.0f;
	bool    _PlayerPawnValid = false;

	// The player pawn's entity (when the pawn is ECS-bridged) -- SampleAgent marks the matching
	// agent row as PlayerProxy. LIVE, unlike the pose fields above.
	FCk_Handle _PlayerPawnEntity;
};

// --------------------------------------------------------------------------------------------------------------------
