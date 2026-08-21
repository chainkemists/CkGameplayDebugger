#pragma once

#include "CkCrowdDebugger/Data/CkCrowdDebugger_DataCollector.h"
#include "CkCrowdDebugger/Data/CkCrowdDebugger_Types.h"

#include "CkEcs/Handle/CkHandle.h"

#include "Delegates/Delegate.h"

class UWorld;

// --------------------------------------------------------------------------------------------------------------------
// MVVM-style view model. Plain C++ (not UObject). Owned by the window. Owns
// the DataCollector and exposes 5 multicast delegates that panels bind to.
// Mirrors the CkGoapDebugger ViewModel pattern.
// --------------------------------------------------------------------------------------------------------------------

enum class ECk_CrowdDebugger_ViewMode : uint8
{
	Live,
	Scrub,    // reserved; only Live is implemented in v1
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCrowd_AgentListChanged,    const TArray<FCkCrowdDebugger_AgentSnapshot>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCrowd_AgentDataRefreshed,  const FCkCrowdDebugger_AgentSnapshot*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCrowd_SelectedAgentChanged,FCk_Handle);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCrowd_PausedChanged,       bool);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCrowd_ViewModeChanged,     ECk_CrowdDebugger_ViewMode);

// --------------------------------------------------------------------------------------------------------------------

class FCkCrowdDebugger_ViewModel
{
public:
	auto Tick(UWorld* InWorld, float InDeltaTime) -> void;
	auto Reset_ForWorldChange() -> void;

	// Run a synthetic FindPathSync probe via the DataCollector. The world is the
	// last one passed to Tick; if there isn't one (no PIE / world torn down), the
	// probe records a NoWorld failure so the panel still updates.
	auto Run_HealthCheckProbe() -> void;

	// Selection
	auto Set_SelectedHandle(FCk_Handle InHandle) -> void;
	auto Get_SelectedHandle() const -> FCk_Handle { return _SelectedHandle; }
	auto Get_SelectedSnapshot() const -> const FCkCrowdDebugger_AgentSnapshot*;

	// One-shot "center the 2D map on the selected agent" request (e.g. a
	// cross-debugger sync resolved to an agent). The viewport panel listens.
	auto Request_FrameSelectedAgent() -> void { OnFrameSelectedAgentRequested.Broadcast(); }

	// All agents
	auto Get_AllAgents() const -> const TArray<FCkCrowdDebugger_AgentSnapshot>&
	{ return _DataCollector.Get_AllAgents(); }

	auto Get_AgentCount() const -> int32 { return _DataCollector.Get_AgentCount(); }

	// Navmesh
	auto Get_NavmeshStatus() const -> const FCkCrowdDebugger_NavmeshStatus&
	{ return _DataCollector.Get_NavmeshStatus(); }

	auto Get_NavTriVerts() const -> const TArray<FVector>&
	{ return _DataCollector.Get_NavTriVerts(); }
	auto Get_NavGeometryRevision() const -> uint64
	{ return _DataCollector.Get_NavGeometryRevision(); }
	auto Get_PathNetworkRibbons() const -> const TArray<FCkCrowdDebugger_PathNetworkRibbonSnapshot>&
	{ return _DataCollector.Get_PathNetworkRibbons(); }
	auto Get_Queues() const -> const TArray<FCkCrowdDebugger_QueueSnapshot>&
	{ return _DataCollector.Get_Queues(); }

	// Viewport camera orientation
	auto Get_ViewYawDegrees() const -> float { return _DataCollector.Get_ViewYawDegrees(); }
	auto Get_ViewYawValid() const -> bool { return _DataCollector.Get_ViewYawValid(); }

	// View camera + player pawn pose (2D-map player marker)
	auto Get_ViewCameraPosition() const -> FVector { return _DataCollector.Get_ViewCameraPosition(); }
	auto Get_ViewCameraValid() const -> bool { return _DataCollector.Get_ViewCameraValid(); }
	auto Get_PlayerPawnPosition() const -> FVector { return _DataCollector.Get_PlayerPawnPosition(); }
	auto Get_PlayerPawnYawDegrees() const -> float { return _DataCollector.Get_PlayerPawnYawDegrees(); }
	auto Get_PlayerPawnValid() const -> bool { return _DataCollector.Get_PlayerPawnValid(); }

	// Pause
	auto Set_Paused(bool InPaused) -> void;
	auto Get_Paused() const -> bool { return _IsPaused; }

	// View Mode
	auto Set_ViewMode(ECk_CrowdDebugger_ViewMode InMode) -> void;
	auto Get_ViewMode() const -> ECk_CrowdDebugger_ViewMode { return _ViewMode; }

public:
	FOnCrowd_AgentListChanged     OnAgentListChanged;
	FOnCrowd_AgentDataRefreshed   OnAgentDataRefreshed;
	FOnCrowd_SelectedAgentChanged OnSelectedAgentChanged;
	FOnCrowd_PausedChanged        OnPausedChanged;
	FOnCrowd_ViewModeChanged      OnViewModeChanged;
	FSimpleMulticastDelegate      OnFrameSelectedAgentRequested;

private:
	FCkCrowdDebugger_DataCollector _DataCollector;
	FCk_Handle _SelectedHandle;
	bool _IsPaused = false;
	int32 _LastAgentCount = -1;
	ECk_CrowdDebugger_ViewMode _ViewMode = ECk_CrowdDebugger_ViewMode::Live;

	// Cached on each Tick so the Run_HealthCheckProbe can grab the current world
	// without the button click handler having to re-find one.
	TWeakObjectPtr<UWorld> _LastWorld;
};

// --------------------------------------------------------------------------------------------------------------------
