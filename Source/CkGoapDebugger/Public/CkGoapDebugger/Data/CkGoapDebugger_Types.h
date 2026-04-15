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

	// A* search stats
	int32 OpenSetSize = 0;
	int32 ClosedSetSize = 0;
	int32 IterationsThisFrame = 0;
	int64 TimeThisFrameMicroseconds = 0;
	float BudgetUsagePercent = 0.0f;
	int64 BudgetMicroseconds = 0;
};

// ====================================================================================================================
// HISTORY ENTRY — Plan completion event
// ====================================================================================================================

struct FCkGoapDebugger_HistoryEntry
{
	double WallTime = 0.0;
	int64 FrameNumber = 0;
	ECk_GoapPlanStatus FinalStatus = ECk_GoapPlanStatus::Idle;
	int32 PlanLength = 0;
	float PlanCost = 0.0f;
	int32 TotalIterations = 0;
	int64 TotalTimeMicroseconds = 0;
};

// ====================================================================================================================
