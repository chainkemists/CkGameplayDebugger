#pragma once

#include "CkSchedulerDebugger/Data/CkSchedulerDebugger_Types.h"

#include "CkCore/Macros/CkMacros.h"

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
};

// --------------------------------------------------------------------------------------------------------------------
