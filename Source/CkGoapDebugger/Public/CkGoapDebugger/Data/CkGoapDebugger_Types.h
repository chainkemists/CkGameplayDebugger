#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "CkGoap/CkGoap_Fragment_Data.h"

// ====================================================================================================================

class UCk_GoapAction_EntityScript;
class UCk_GoapGoal_EntityScript;

// ====================================================================================================================
// ACTION INFO — Debugger snapshot of a registered action
// ====================================================================================================================

struct FCkGoapDebugger_ActionInfo
{
	TSubclassOf<UCk_GoapAction_EntityScript> ActionClass;
	FString ClassName;
	TMap<FGameplayTag, bool> Preconditions;
	TMap<FGameplayTag, bool> Effects;
	float Cost = 1.0f;
};

// ====================================================================================================================
// GOAL INFO — Debugger snapshot of a registered goal
// ====================================================================================================================

struct FCkGoapDebugger_GoalInfo
{
	TSubclassOf<UCk_GoapGoal_EntityScript> GoalClass;
	FString ClassName;
	TMap<FGameplayTag, bool> Conditions;
	int32 Priority = 0;
	bool IsActiveGoal = false;
};

// ====================================================================================================================
// DIAGNOSTIC SNAPSHOT — Surfaces FFragment_Goap_Diagnostics into the debugger
// ====================================================================================================================

struct FCkGoapDebugger_CycleInfo
{
	TArray<FString> ActionNames;
	TArray<FGameplayTag> CycleConditions;
};

struct FCkGoapDebugger_Diagnostics
{
	TArray<FCkGoapDebugger_CycleInfo> DependencyCycles;
	TArray<TPair<FGameplayTag, bool>> UnreachableGoalConditions;
	FString LastFailedGoalName;

	auto HasAnyWarning() const -> bool
	{
		return DependencyCycles.Num() > 0 || UnreachableGoalConditions.Num() > 0;
	}
};

// ====================================================================================================================
// GOAP INFO — Per-entity snapshot for the debugger
// ====================================================================================================================

struct FCkGoapDebugger_GoapInfo
{
	FCk_Handle Handle;
	FString DebugName;
	ECk_GoapPlanStatus PlanStatus = ECk_GoapPlanStatus::Idle;

	// World state
	TMap<FGameplayTag, bool> WorldState;

	// Registered actions
	TArray<FCkGoapDebugger_ActionInfo> Actions;

	// Registered goals
	TArray<FCkGoapDebugger_GoalInfo> Goals;

	// Current plan (execution order)
	TArray<FString> PlanActionNames;
	float PlanCost = 0.0f;

	// Framework-incremented counter. Used by the data collector to spot a new
	// plan attempt even when Planning → terminal happens in a single frame.
	int32 PlanAttemptCount = 0;

	// A* search stats
	int32 OpenSetSize = 0;
	int32 ClosedSetSize = 0;
	int32 IterationsThisFrame = 0;
	int64 TimeThisFrameMicroseconds = 0;
	float BudgetUsagePercent = 0.0f;
	int64 BudgetMicroseconds = 0;

	// Framework-emitted diagnostics (graph cycles, unreachable conditions).
	FCkGoapDebugger_Diagnostics Diagnostics;
};

// ====================================================================================================================
// HISTORY ENTRY — Plan completion event + full snapshot
// ====================================================================================================================
//
// The snapshot field holds a frozen FCkGoapDebugger_GoapInfo (world state,
// plan, diagnostics, active goal) captured at the moment the plan completed.
// The debugger's scrub mode rehydrates the entire UI from this snapshot so
// the user can click a history entry and see exactly what the planner saw.
//
// Note: this does cost memory — each entry holds a copy of the whole info
// struct. For long-running sessions we rely on a bounded ring (caller decides).

struct FCkGoapDebugger_HistoryEntry
{
	double WallTime = 0.0;
	int64 FrameNumber = 0;
	ECk_GoapPlanStatus FinalStatus = ECk_GoapPlanStatus::Idle;
	int32 PlanLength = 0;
	float PlanCost = 0.0f;
	int32 TotalIterations = 0;
	int64 TotalTimeMicroseconds = 0;

	// Full snapshot of the entity at the moment the plan completed. Used by
	// scrub mode to rebuild the graph / world-state / details panels.
	FCkGoapDebugger_GoapInfo Snapshot;
};

// ====================================================================================================================
