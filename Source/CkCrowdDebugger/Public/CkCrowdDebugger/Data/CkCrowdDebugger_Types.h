#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CkNavigation/Nav/CkNav_Fragment_Data.h"

#include "CkPathNetwork/Network/CkPathNetwork_Fragment_Data.h"

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>

// --------------------------------------------------------------------------------------------------------------------
// Per-frame snapshot of a single crowd agent. The DataCollector populates an array
// of these each tick; the ViewModel exposes them to the panels. Keep this struct
// flat / copyable — no pointers into ECS state. Gates 0–7 grow this struct by
// adding fields incrementally; the agent-list panel reads the subset it cares
// about (handle id + tags + status badge) regardless of which gate's fields are
// present.
// --------------------------------------------------------------------------------------------------------------------

enum class ECkCrowdDebugger_AgentStatus : uint8
{
	None,           // Gate 0 default — no movement / pathfinding state yet
	Idle,           // No goal, awake
	Walking,        // Has goal + path, moving (Gate 2+)
	Asleep,         // Sleep tag stamped (Gate 4+)
	Replanning,     // Recovering from blocked path (Gate 4+)
	Failed,         // Path failed N times → Failed tag (Gate 4+)
	PlayerProxy,    // Player proxy entity (Gate 5+)
};

// --------------------------------------------------------------------------------------------------------------------
// One row in the Agent Detail Neighbors section. Mirrors FCk_CrowdAgent_Neighbor flat-copyable, no
// pointers into ECS state. The handle's id is rendered cyan in the panel; distance is in cm.

struct FCkCrowdDebugger_NeighborInfo
{
	FCk_Handle Handle;
	float      Distance = 0.0f;
	FVector    RelativeOffset = FVector::ZeroVector;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkCrowdDebugger_AgentSnapshot
{
	FCk_Handle               Handle;
	FGameplayTagContainer    Tags;
	ECkCrowdDebugger_AgentStatus Status = ECkCrowdDebugger_AgentStatus::None;
	ECk_Nav_PathFailReason   PathFailReason = ECk_Nav_PathFailReason::None;
	int32                    NeighborCount = 0;
	FString                  PrimaryTag;       // Display string — first tag, or "—"

	// Who the agent belongs to — the crowd-agent feature entity's lifetime owner
	// (the actual NPC/player entity). Name pre-sampled so rows don't walk ECS per paint.
	FCk_Handle               OwnerHandle;
	FString                  OwnerName;

	// Gate 2+ fills these
	FVector                  Position = FVector::ZeroVector;
	FVector                  Velocity = FVector::ZeroVector;

	// Locomotion params + active-goal state — feeds the "Orbit Diagnosis" section + WILL-ORBIT
	// verdict in the detail panel (predicted orbit radius = MaxSpeed/MaxTurnRate vs ArrivalRadius).
	float                    MaxSpeed = 0.0f;
	float                    MaxTurnRate = 0.0f;
	float                    MaxAcceleration = 0.0f;
	float                    ArrivalRadius = 0.0f;        // params default
	float                    ActiveArrivalRadius = 0.0f;  // per-active-goal override (valid while Walking)
	FVector                  ActiveGoal = FVector::ZeroVector;
	bool                     IsWalking = false;

	// Last path problem in this movement episode. These are flat values copied from CkCrowd's
	// retained record; Slate never holds a live fragment reference or handle-derived state.
	bool                     HasPathTroubleEvent = false;
	bool                     HadPathNetworkFailure = false;
	bool                     UsedNavigationFallback = false;
	ECk_PathNetwork_RouteFailReason PathNetworkFailReason = ECk_PathNetwork_RouteFailReason::None;
	ECk_Nav_PathStatus       TroubleNavigationStatus = ECk_Nav_PathStatus::None;
	ECk_Nav_PathFailReason   TroubleNavigationFailReason = ECk_Nav_PathFailReason::None;
	FVector                  PathTroubleAgentPosition = FVector::ZeroVector;
	FVector                  PathTroubleGoal = FVector::ZeroVector;
	double                   PathTroubleEventTimeSeconds = -1.0;
	FString                  PathTroubleSummary;

	// Remaining nav waypoints (world space) — drawn as the planned-path polyline in the viewport.
	TArray<FVector>          PlannedPath;

	// Gate 0 minimum — these are enough for the agent list to render
	float                    Radius = 0.0f;
	float                    Height = 0.0f;

	// Gate 3 — separation. Neighbors is sorted by distance ascending and trimmed to the
	// agent's _MaxNeighborsForSteering. SeparationForce is the Gate 3B output (zero until 3B
	// lands). SeparationRadius / SeparationWeight come from the params struct so the detail
	// panel can label the Neighbors section "(within Xcm)" without reaching back into ECS.
	TArray<FCkCrowdDebugger_NeighborInfo> Neighbors;
	FVector                  SeparationForce  = FVector::ZeroVector;
	float                    SeparationRadius = 0.0f;
	float                    SeparationWeight = 0.0f;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkCrowdDebugger_NavmeshStatus
{
	bool _Sampled = false;        // false until Gate 1 lands
	bool _NavSystemPresent = false;
	FString _NavDataClassName;
	bool _DefaultFilterValid = false;
	int32 _SupportedAgents = 0;
	double _LastRegenTimestamp = -1.0;

	// Navmesh world bounds (ARecastNavMesh::GetBounds) — the viewport's world->screen extent.
	bool    _NavBoundsValid = false;
	FVector _NavBoundsMin = FVector::ZeroVector;
	FVector _NavBoundsMax = FVector::ZeroVector;

	// Synthetic FindPathSync probe ("Run Health Check" button). Stays default-zero
	// until the user runs a probe; then populated with the latest result so the
	// panel can render "Last health check: PASS 3.4s ago" / "FAIL — reason …".
	bool    _HealthCheckRun = false;
	bool    _HealthCheckPassed = false;
	FString _HealthCheckFailReason;       // human-readable enum tag string
	float   _HealthCheckDurationMs = 0.0f;
	double  _HealthCheckTimestamp = -1.0; // FPlatformTime::Seconds() at last probe
	int32   _HealthCheckWaypoints = 0;
};

// --------------------------------------------------------------------------------------------------------------------

// Flat path-network copy for the debugger viewport. The collector owns only value data
// from the selected PIE world; it never retains the actor or ECS network handle.
struct FCkCrowdDebugger_PathNetworkRibbonSnapshot
{
	TArray<FVector> Points;
	TArray<float> HalfWidths;
};

// --------------------------------------------------------------------------------------------------------------------
