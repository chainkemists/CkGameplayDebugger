#pragma once

#include "CoreMinimal.h"
#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------

enum class ECkSchedulerDebugger_TreeNodeType : uint8
{
	Root,
	TickGroup,
	Group,
	Processor
};

// --------------------------------------------------------------------------------------------------------------------

// Per-processor write-conflict record as surfaced in the scheduler debugger Inspector.
// One entry is emitted for every conflict in which this processor participates — the peer is whichever
// of (First, Second) is NOT this processor. WasAutoResolved mirrors FCk_WriteConflictInfo::_AutoInserted:
// true in Permissive mode (an edge was auto-inserted in declaration order), false in Strict mode (graph
// build failed and the user must add explicit RunAfter/RunBefore).
struct FCkSchedulerDebugger_WriteConflictInfo
{
	CK_GENERATED_BODY(FCkSchedulerDebugger_WriteConflictInfo);

	FName PeerProcessorName;
	FName FragmentName;
	bool  WasAutoResolved = false;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkSchedulerDebugger_ProcessorInfo
{
	CK_GENERATED_BODY(FCkSchedulerDebugger_ProcessorInfo);

	FName ProcessorName;
	FString DisplayName;
	int32 NodeIndex = INDEX_NONE;
	int32 ExecutionOrder = INDEX_NONE;

	TArray<int32> InEdges;
	TArray<int32> OutEdges;

	ETickingGroup TickGroup = TG_PrePhysics;
	FName GroupName;

	bool IsGhost = false;
	bool IsGroupStart = false;
	bool IsGroupEnd = false;
	int32 PairedGroupNodeIndex = INDEX_NONE;

	bool HasDirtyMarker = false;
	bool IsParallel = false;

	double MainPassTimeMs = 0.0;
	TArray<double> PumpPassTimesMs;
	bool WasDirtyThisFrame = false;
	int32 PumpCountThisFrame = 0;

	int32 TotalTicks = 0;
	double TickRate = 0.0;

	TArray<double> TimingHistory;

	// Populated from FProcessorGraphPartition::_WriteConflicts when this processor is one of the
	// two writers in a conflict. Empty for any processor with no detected conflicts.
	TArray<FCkSchedulerDebugger_WriteConflictInfo> WriteConflicts;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkSchedulerDebugger_GroupInfo
{
	CK_GENERATED_BODY(FCkSchedulerDebugger_GroupInfo);

	FName GroupName;
	FString DisplayName;
	ETickingGroup TickGroup = TG_PrePhysics;
	int32 StartNodeIndex = INDEX_NONE;
	int32 EndNodeIndex = INDEX_NONE;
	TArray<int32> MemberIndices;
	FLinearColor AccentColor;
	double AggregateTimeMs = 0.0;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkSchedulerDebugger_TreeNode
{
	CK_GENERATED_BODY(FCkSchedulerDebugger_TreeNode);

	ECkSchedulerDebugger_TreeNodeType Type = ECkSchedulerDebugger_TreeNodeType::Root;
	FString DisplayName;
	int32 ProcessorIndex = INDEX_NONE;
	int32 GroupIndex = INDEX_NONE;
	ETickingGroup TickGroup = TG_PrePhysics;
	TArray<TSharedPtr<FCkSchedulerDebugger_TreeNode>> Children;
	TWeakPtr<FCkSchedulerDebugger_TreeNode> Parent;
	bool IsVisible = true;
	bool IsExpanded = true;
};

// --------------------------------------------------------------------------------------------------------------------
