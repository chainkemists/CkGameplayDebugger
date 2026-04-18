#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "CkGoap/CkGoap_Fragment_Data.h"

// ====================================================================================================================

class UCk_GoapAction_EntityScript;
class UCk_GoapGoal_EntityScript;

// ====================================================================================================================
// DISPLAY TYPES — debugger-side copies sized for rendering
// ====================================================================================================================

struct FCkGoapDebugger_Condition
{
	FGameplayTag Key;
	bool Value = false;

	auto AsString() const -> FString
	{
		return FString::Printf(TEXT("%s = %s"),
			*Key.ToString(),
			Value ? TEXT("true") : TEXT("false"));
	}
};

struct FCkGoapDebugger_Effect
{
	FGameplayTag Key;
	bool Value = false;

	auto AsString() const -> FString
	{
		return FString::Printf(TEXT("%s := %s"),
			*Key.ToString(),
			Value ? TEXT("true") : TEXT("false"));
	}
};

struct FCkGoapDebugger_WorldStateEntry
{
	FGameplayTag Key;
	bool Value = false;
};

// ====================================================================================================================
// ACTION / GOAL INFO
// ====================================================================================================================

struct FCkGoapDebugger_ActionInfo
{
	TSubclassOf<UCk_GoapAction_EntityScript> ActionClass;
	FString ClassName;
	TArray<FCkGoapDebugger_Condition> Preconditions;
	TArray<FCkGoapDebugger_Effect>    Effects;
	float Cost = 1.0f;
};

struct FCkGoapDebugger_GoalInfo
{
	TSubclassOf<UCk_GoapGoal_EntityScript> GoalClass;
	FString ClassName;
	TArray<FCkGoapDebugger_Condition> Conditions;
	int32 Priority = 0;
	bool IsActiveGoal = false;
};

// ====================================================================================================================
// DIAGNOSTICS
// ====================================================================================================================

struct FCkGoapDebugger_CycleInfo
{
	TArray<FString> ActionNames;
	TArray<FGameplayTag> CycleConditions;
};

struct FCkGoapDebugger_Diagnostics
{
	TArray<FCkGoapDebugger_CycleInfo> DependencyCycles;
	TArray<FCkGoapDebugger_Condition> UnreachableGoalConditions;
	FString LastFailedGoalName;

	auto HasAnyWarning() const -> bool
	{
		return DependencyCycles.Num() > 0 || UnreachableGoalConditions.Num() > 0;
	}
};

// ====================================================================================================================
// FAILURE ANALYSIS
// ====================================================================================================================

struct FCkGoapDebugger_ConditionAnalysis
{
	FCkGoapDebugger_Condition Condition;

	bool HasCurrentValue = false;
	bool CurrentValue = false;

	bool IsSatisfiedByWorldState = false;
	bool IsUnreachable = false;

	TArray<FString> RelevantActionClassNames;
};

enum class ECkGoapDebugger_TreeNodeKind : uint8
{
	Condition,
	Action,
};

struct FCkGoapDebugger_PlanTreeNode
{
	int32 Depth = 0;
	ECkGoapDebugger_TreeNodeKind Kind = ECkGoapDebugger_TreeNodeKind::Condition;

	// Condition-kind fields
	FCkGoapDebugger_Condition Condition;
	bool HasCurrentValue = false;
	bool CurrentValue = false;
	bool IsSatisfiedByWorldState = false;
	bool IsUnreachable = false;
	int32 CandidateActionCount = 0;

	// Action-kind fields
	FString ActionClassName;
	float ActionCost = 0.0f;

	// Dimmed annotation suffix, e.g. "(cycle)", "(depth limit)".
	FString Note;
};

struct FCkGoapDebugger_FailureAnalysis
{
	bool HasActiveGoal = false;
	FString ActiveGoalClassName;
	TArray<FCkGoapDebugger_ConditionAnalysis> ConditionAnalyses;
	TArray<FCkGoapDebugger_PlanTreeNode> PlanTree;
	int32 UnreachableConditionCount = 0;
	int32 UnsatisfiedConditionCount = 0;
	int32 DeepUnreachableNodeCount = 0;
};

// ====================================================================================================================
// GOAP INFO — Per-entity snapshot
// ====================================================================================================================

struct FCkGoapDebugger_GoapInfo
{
	FCk_Handle Handle;
	FString DebugName;
	ECk_GoapPlanStatus PlanStatus = ECk_GoapPlanStatus::Idle;

	TArray<FCkGoapDebugger_WorldStateEntry> WorldState;

	TArray<FCkGoapDebugger_ActionInfo> Actions;
	TArray<FCkGoapDebugger_GoalInfo>   Goals;

	TArray<FString> PlanActionNames;
	float PlanCost = 0.0f;

	int32 PlanAttemptCount = 0;

	int32 OpenSetSize = 0;
	int32 ClosedSetSize = 0;
	int32 IterationsThisFrame = 0;
	int64 TimeThisFrameMicroseconds = 0;
	float BudgetUsagePercent = 0.0f;
	int64 BudgetMicroseconds = 0;

	FCkGoapDebugger_Diagnostics Diagnostics;
	FCkGoapDebugger_FailureAnalysis FailureAnalysis;
};

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

	FCkGoapDebugger_GoapInfo Snapshot;
};

struct FCkGoapDebugger_SearchSnapshot
{
	int64 FrameNumber = 0;
	int32 IterationsThisFrame = 0;
	int32 OpenSetSize = 0;
	int32 ClosedSetSize = 0;
	int64 TimeThisFrameMicroseconds = 0;
	float BudgetUsagePercent = 0.0f;
	ECk_GoapPlanStatus Status = ECk_GoapPlanStatus::Idle;
};

// ====================================================================================================================
