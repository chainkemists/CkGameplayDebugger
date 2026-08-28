#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CkCrowd/Agent/CkCrowdAgent_Fragment_Data.h"

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
	// Which backend owns the in-flight query. Every provider parks the same nav slot, so without
	// this the panels would name CkNavigation for a stalled sidewalk or volumetric route.
	ECk_CrowdAgent_PathProvider ActiveProvider = ECk_CrowdAgent_PathProvider::None;
	ECk_Nav_PathFailReason   TroubleNavigationFailReason = ECk_Nav_PathFailReason::None;
	FVector                  PathTroubleAgentPosition = FVector::ZeroVector;
	FVector                  PathTroubleGoal = FVector::ZeroVector;
	double                   PathTroubleEventTimeSeconds = -1.0;
	FString                  PathTroubleSummary;

	// Remaining nav waypoints (world space) — drawn as the planned-path polyline in the viewport.
	// POPULATED FOR THE SELECTED AGENT ONLY. The scene adapter gates the planned-path polyline on
	// the selected identity, so carrying every agent's waypoints meant deep-copying them three
	// times per frame (collector -> list item -> 3d snapshot) plus a full finite-validation pass,
	// to draw exactly one line. Rows that just need the length read PlannedPathPointCount.
	TArray<FVector>          PlannedPath;

	// Waypoint count for EVERY agent, including those whose points are not carried. This is what
	// roster rows display, and it stays correct regardless of selection.
	int32                    PlannedPathPointCount = 0;

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

	// Copied queue membership, if this crowd mover is currently assigned to a queue reservation.
	FString                  QueueDebugName;
	FString                  QueueCategory;
	FString                  QueueState;
	int32                    QueueRank = INDEX_NONE;
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

// Detached avoidance-volume projection. This remains debugger-local so no Slate or retained scene code
// depends on CkCrowd fragments, handles, nav-area markup, or producer-owned arrays.
enum class ECkCrowdDebugger_AvoidanceVolumeState : uint8
{
	Pending,
	Confirmed,
	Invalid,
	Retiring,
};

// Policy is copied from CkCrowd rather than exposing its fragment/enum to Slate or retained-scene code.
enum class ECkCrowdDebugger_AvoidanceVolumeTraversalPolicy : uint8
{
	AvoidIfPossible,
	HardExclude,
	CostOnly,
};

struct FCkCrowdDebugger_AvoidanceVolumeSnapshot
{
	uint64 Identity = 0;
	uint64 Revision = 0;
	FString DebugName;
	FTransform YawWorldTransform = FTransform::Identity;
	FVector PhysicalWorldHalfExtents = FVector::ZeroVector;
	FVector InfluenceWorldHalfExtents = FVector::ZeroVector;
	FVector PaintedWorldHalfExtents = FVector::ZeroVector;
	float SecondsSincePaint = 0.0f;
	uint64 NavigationRevisionAtUnregister = 0;
	ECkCrowdDebugger_AvoidanceVolumeState State = ECkCrowdDebugger_AvoidanceVolumeState::Pending;
	ECkCrowdDebugger_AvoidanceVolumeTraversalPolicy TraversalPolicy =
		ECkCrowdDebugger_AvoidanceVolumeTraversalPolicy::AvoidIfPossible;
	bool HasValidGeometry = false;
};

// --------------------------------------------------------------------------------------------------------------------

// Every provider parks the SAME nav-path slot, so a pending label that hard-codes CkNavigation
// reports a stalled sidewalk or volumetric route as an Unreal-navmesh problem. One definition,
// shared by the overlay-facing summary and both panels, so they cannot drift into disagreeing.
inline auto CkCrowdDebugger_MakePendingLabel(ECk_CrowdAgent_PathProvider InProvider) -> FString
{
	switch (InProvider)
	{
		case ECk_CrowdAgent_PathProvider::PathNetwork: return TEXT("SIDEWALK: Pending");
		case ECk_CrowdAgent_PathProvider::VoxelNav:    return TEXT("VOXEL NAV: Pending");
		default:                                      return TEXT("UNREAL NAV: Pending");
	}
}

// --------------------------------------------------------------------------------------------------------------------

// Value-only queue snapshot.  Queue collection is deliberately separate from the
// crowd-agent rows: a queue is a first-class spatial structure with its own
// identity, category and reservation geometry.  Nothing in this type retains an
// ECS handle, registry or producer-owned array.
struct FCkCrowdDebugger_QueueMemberSnapshot
{
	uint64 AgentIdentity = 0;
	int32 Rank = INDEX_NONE;
	FVector ReservationLocation = FVector::ZeroVector;
	FVector ReservationForward = FVector::ForwardVector;
	bool HasReservation = false;
};

struct FCkCrowdDebugger_QueueSnapshot
{
	uint64 Identity = 0;
	uint64 Revision = 0;
	FString DebugName;
	FString Category;
	FString State;
	FVector OwnerTargetLocation = FVector::ZeroVector;
	FVector OwnerTargetForward = FVector::ForwardVector;
	TArray<FCkCrowdDebugger_QueueMemberSnapshot> Members;
};

// --------------------------------------------------------------------------------------------------------------------
