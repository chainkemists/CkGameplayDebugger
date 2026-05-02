#pragma once

#include "CkEcs/Handle/CkHandle.h"

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
	int32                    NeighborCount = 0;
	FString                  PrimaryTag;       // Display string — first tag, or "—"

	// Gate 2+ fills these
	FVector                  Position = FVector::ZeroVector;
	FVector                  Velocity = FVector::ZeroVector;

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
