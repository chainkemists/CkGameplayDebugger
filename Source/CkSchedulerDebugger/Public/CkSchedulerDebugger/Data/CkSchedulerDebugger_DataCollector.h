#pragma once

#include "CkSchedulerDebugger/Data/CkSchedulerDebugger_Types.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkEcs/Scheduler/CkSchedulerDebugData.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck { class FProcessorScheduler; }

// --------------------------------------------------------------------------------------------------------------------

class FCkSchedulerDebugger_DataCollector
{
public:
	CK_GENERATED_BODY(FCkSchedulerDebugger_DataCollector);

public:
	auto Collect(UWorld* InWorld) -> void;

	auto Get_Processors() const -> const TArray<FCkSchedulerDebugger_ProcessorInfo>&;
	auto Get_Groups() const -> const TArray<FCkSchedulerDebugger_GroupInfo>&;
	auto Get_TreeRoots() const -> const TArray<TSharedPtr<FCkSchedulerDebugger_TreeNode>>&;
	auto Get_TotalFrameTimeMs() const -> double;
	auto Get_PumpCount() const -> int32;
	auto Get_ProcessorCount() const -> int32;
	auto Get_GhostCount() const -> int32;
	auto Get_DirtyCount() const -> int32;
	auto Get_ParallelCount() const -> int32;

	auto Get_FrameSnapshots() const -> const TArray<FCkSchedulerDebugger_FrameSnapshotInfo>&;
	auto Get_FrameSnapshotCount() const -> int32;

	/** Overwrites processor timing data with a historical frame. InOffset=0 means latest, 1=one frame back, etc. */
	auto ApplyFrameSnapshot(int32 InOffset) -> void;

	/** Restores the latest frame's timing data (offset 0). */
	auto RestoreLiveData() -> void;

	/** Sets the max frame history buffer size on all active schedulers. */
	auto Set_FrameHistoryMaxSize(UWorld* InWorld, int32 InMaxFrames) -> void;

private:
	auto DoCollectFromScheduler(
		const ck::FProcessorScheduler& InScheduler,
		ETickingGroup InTickGroup) -> void;

	auto DoBuildTreeHierarchy() -> void;
	auto DoIdentifyGroups() -> void;

	static auto DoComputeDisplayName(
		const FName& InProcessorName) -> FString;

	auto DoUpdateTimingOnly(
		const ck::FProcessorScheduler& InScheduler,
		ETickingGroup InTickGroup) -> void;

	auto DoComputeTopologyHash() const -> uint32;

private:
	TArray<FCkSchedulerDebugger_ProcessorInfo> _Processors;
	TArray<FCkSchedulerDebugger_GroupInfo> _Groups;
	TArray<TSharedPtr<FCkSchedulerDebugger_TreeNode>> _TreeRoots;

	double _TotalFrameTimeMs = 0.0;
	int32 _PumpCount = 0;
	int32 _ProcessorCount = 0;
	int32 _GhostCount = 0;
	int32 _DirtyCount = 0;
	int32 _ParallelCount = 0;

	uint32 _LastTopologyHash = 0;

	// ---- Frame history (cached from scheduler's _DebugFrameHistory)
	TArray<FCkSchedulerDebugger_FrameSnapshotInfo> _FrameSnapshots;

	// We need to store the raw scheduler snapshots so ApplyFrameSnapshot can read per-processor data.
	// This is a copy of the scheduler's circular buffer taken each Collect().
	struct FCachedSchedulerHistory
	{
		ETickingGroup TickGroup;
		TArray<ck::FSchedulerDebug_FrameSnapshot> Snapshots;
	};
	TArray<FCachedSchedulerHistory> _CachedSchedulerHistories;

	auto DoCacheFrameHistory() -> void;
};

// --------------------------------------------------------------------------------------------------------------------
