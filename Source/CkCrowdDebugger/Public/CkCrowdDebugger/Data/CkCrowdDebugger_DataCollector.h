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
	auto Get_PathNetworkRibbons() const -> const TArray<FCkCrowdDebugger_PathNetworkRibbonSnapshot>&
	{ return _PathNetworkRibbons; }

	// Current local-player view yaw (degrees) + validity — used to orient the viewport to the camera.
	auto Get_ViewYawDegrees() const -> float { return _ViewYawDegrees; }
	auto Get_ViewYawValid() const -> bool { return _ViewYawValid; }

	// View camera position (level-editor camera while ejected, else the possessed
	// view point) — drawn as the FOV wedge on the 2D map.
	auto Get_ViewCameraPosition() const -> FVector { return _ViewCameraPosition; }
	auto Get_ViewCameraValid() const -> bool { return _ViewCameraValid; }

	// Local player pawn pose — drawn as the player chevron on the 2D map. Stays
	// valid while ejected (the pawn is still in the world, just not driven).
	auto Get_PlayerPawnPosition() const -> FVector { return _PlayerPawnPosition; }
	auto Get_PlayerPawnYawDegrees() const -> float { return _PlayerPawnYawDegrees; }
	auto Get_PlayerPawnValid() const -> bool { return _PlayerPawnValid; }

private:
	auto SampleAgent(FCk_Handle InHandle) -> void;

private:
	TArray<FCkCrowdDebugger_AgentSnapshot> _Agents;
	FCkCrowdDebugger_NavmeshStatus _NavmeshStatus;

	TArray<FVector> _NavTriVerts;
	TArray<FCkCrowdDebugger_PathNetworkRibbonSnapshot> _PathNetworkRibbons;
	double _NavGeomLastPullTime = -1.0;

	float _ViewYawDegrees = 0.0f;
	bool  _ViewYawValid = false;

	FVector _ViewCameraPosition = FVector::ZeroVector;
	bool    _ViewCameraValid = false;

	FVector _PlayerPawnPosition = FVector::ZeroVector;
	float   _PlayerPawnYawDegrees = 0.0f;
	bool    _PlayerPawnValid = false;

	// The player pawn's entity (when the pawn is ECS-bridged) — SampleAgent marks
	// the matching agent row as PlayerProxy.
	FCk_Handle _PlayerPawnEntity;
};

// --------------------------------------------------------------------------------------------------------------------
