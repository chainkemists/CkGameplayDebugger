#include "CkSchedulerDebugger_DataCollector.h"

#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"

#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcs/Scheduler/CkProcessorScheduler.h"
#include "CkEcs/Scheduler/CkSchedulerDebugData.h"

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugger_DataCollector::
	Collect(
		UWorld* InWorld)
	-> void
{
	if (NOT IsValid(InWorld))
	{ return; }

	auto Subsystem = InWorld->GetSubsystem<UCk_EcsWorld_Subsystem_UE>();
	if (NOT IsValid(Subsystem))
	{ return; }

	auto TempHash = uint32{0};
	for (const auto& [TickGroup, ActorPtr] : Subsystem->Get_WorldActors())
	{
		if (NOT ActorPtr.IsValid())
		{ continue; }
		const auto& SchedulerOpt = ActorPtr->Get_Scheduler();
		if (NOT SchedulerOpt.IsSet())
		{ continue; }
		const auto& Partition = SchedulerOpt.GetValue().Get_Partition();
		TempHash = HashCombine(TempHash, GetTypeHash(Partition._Nodes.Num()));
		for (const auto& Node : Partition._Nodes)
		{
			TempHash = HashCombine(TempHash, GetTypeHash(Node._ProcessorName));
		}
	}

	const auto TopologyChanged = (TempHash != _LastTopologyHash) || (_Processors.Num() == 0);

	if (TopologyChanged)
	{
		_Processors.Reset();
		_Groups.Reset();
		_TreeRoots.Reset();
		_LastTopologyHash = TempHash;

		_ProcessorCount = 0;
		_GhostCount = 0;
		_DirtyCount = 0;
		_ParallelCount = 0;

		for (const auto& [TickGroup, ActorPtr] : Subsystem->Get_WorldActors())
		{
			if (NOT ActorPtr.IsValid())
			{ continue; }
			const auto& SchedulerOpt = ActorPtr->Get_Scheduler();
			if (NOT SchedulerOpt.IsSet())
			{ continue; }
			DoCollectFromScheduler(SchedulerOpt.GetValue(), TickGroup);
		}

		DoIdentifyGroups();
		DoBuildTreeHierarchy();
	}
	else
	{
		_TotalFrameTimeMs = 0.0;
		_PumpCount = 0;

		for (const auto& [TickGroup, ActorPtr] : Subsystem->Get_WorldActors())
		{
			if (NOT ActorPtr.IsValid())
			{ continue; }
			const auto& SchedulerOpt = ActorPtr->Get_Scheduler();
			if (NOT SchedulerOpt.IsSet())
			{ continue; }
			DoUpdateTimingOnly(SchedulerOpt.GetValue(), TickGroup);
		}

		for (auto GroupIdx = 0; GroupIdx < _Groups.Num(); ++GroupIdx)
		{
			auto AggregateTime = 0.0;
			for (const auto MemberIdx : _Groups[GroupIdx].MemberIndices)
			{
				if (_Processors.IsValidIndex(MemberIdx))
				{
					AggregateTime += _Processors[MemberIdx].MainPassTimeMs;
				}
			}
			_Groups[GroupIdx].AggregateTimeMs = AggregateTime;
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugger_DataCollector::
	DoCollectFromScheduler(
		const ck::FProcessorScheduler& InScheduler,
		ETickingGroup InTickGroup)
	-> void
{
	const auto& Partition = InScheduler.Get_Partition();
	const auto& Nodes = Partition._Nodes;
	const auto& ExecutionOrder = Partition._ExecutionOrder;

	auto ExecutionOrderLookup = TMap<int32, int32>{};
	for (auto OrderPos = 0; OrderPos < ExecutionOrder.Num(); ++OrderPos)
	{
		ExecutionOrderLookup.Add(ExecutionOrder[OrderPos], OrderPos);
	}

#if !UE_BUILD_SHIPPING
	const auto& FrameHistory = InScheduler.Get_DebugFrameHistory();
	const auto HasLatestSnapshot = FrameHistory.Num() > 0;
	const auto& LatestSnapshot = HasLatestSnapshot
		? FrameHistory.Last()
		: ck::FSchedulerDebug_FrameSnapshot{};

	if (HasLatestSnapshot)
	{
		_TotalFrameTimeMs += LatestSnapshot.TotalFrameTimeMs;
		_PumpCount = FMath::Max(_PumpCount, LatestSnapshot.PumpIterationCount);
	}
#endif

	const auto BaseProcessorIndex = _Processors.Num();

	for (auto NodeIdx = 0; NodeIdx < Nodes.Num(); ++NodeIdx)
	{
		const auto& Node = Nodes[NodeIdx];

		auto Info = FCkSchedulerDebugger_ProcessorInfo{};
		Info.ProcessorName = Node._ProcessorName;
		Info.DisplayName = DoComputeDisplayName(Node._ProcessorName);
		Info.NodeIndex = Node._Index;
		Info.InEdges = Node._InEdges;
		Info.OutEdges = Node._OutEdges;
		Info.TickGroup = InTickGroup;
		Info.IsGhost = Node._IsGhost;
		Info.IsGroupStart = Node._IsGroupStart;
		Info.IsGroupEnd = Node._IsGroupEnd;
		Info.PairedGroupNodeIndex = Node._PairedGroupNodeIndex;
		Info.HasDirtyMarker = Node._HasDirtyMarker;
		Info.GroupName = NAME_None;

		const auto* FoundOrder = ExecutionOrderLookup.Find(NodeIdx);
		Info.ExecutionOrder = FoundOrder ? *FoundOrder : INDEX_NONE;

#if !UE_BUILD_SHIPPING
		if (HasLatestSnapshot && LatestSnapshot.ProcessorTimings.IsValidIndex(NodeIdx))
		{
			const auto& Timing = LatestSnapshot.ProcessorTimings[NodeIdx];
			Info.MainPassTimeMs = Timing.MainPassTimeMs;
			Info.PumpPassTimesMs = Timing.PumpPassTimesMs;
			Info.WasDirtyThisFrame = Timing.WasDirtyThisFrame;
			Info.PumpCountThisFrame = Timing.PumpCountThisFrame;
		}

		constexpr auto MaxHistoryFrames = 300;
		const auto HistoryStart = FMath::Max(0, FrameHistory.Num() - MaxHistoryFrames);
		for (auto FrameIdx = HistoryStart; FrameIdx < FrameHistory.Num(); ++FrameIdx)
		{
			const auto& Snapshot = FrameHistory[FrameIdx];
			if (Snapshot.ProcessorTimings.IsValidIndex(NodeIdx))
			{
				Info.TimingHistory.Add(Snapshot.ProcessorTimings[NodeIdx].MainPassTimeMs);
			}
		}

		auto TickCount = 0;
		for (auto FrameIdx = HistoryStart; FrameIdx < FrameHistory.Num(); ++FrameIdx)
		{
			const auto& Snapshot = FrameHistory[FrameIdx];
			if (Snapshot.ProcessorTimings.IsValidIndex(NodeIdx)
				&& Snapshot.ProcessorTimings[NodeIdx].MainPassTimeMs > 0.0)
			{
				++TickCount;
			}
		}
		Info.TotalTicks = TickCount;

		const auto HistoryCount = FrameHistory.Num() - HistoryStart;
		Info.TickRate = HistoryCount > 0
			? static_cast<double>(TickCount) / static_cast<double>(HistoryCount)
			: 0.0;
#endif

		if (NOT Info.IsGhost && NOT Info.IsGroupStart && NOT Info.IsGroupEnd)
		{
			++_ProcessorCount;
		}
		if (Info.IsGhost) { ++_GhostCount; }
		if (Info.HasDirtyMarker) { ++_DirtyCount; }
		if (Info.IsParallel) { ++_ParallelCount; }

		_Processors.Add(MoveTemp(Info));
	}

	for (auto Idx = BaseProcessorIndex; Idx < _Processors.Num(); ++Idx)
	{
		const auto& Node = Nodes[Idx - BaseProcessorIndex];

		if (Node._IsGroupStart)
		{
			const auto PairedIdx = Node._PairedGroupNodeIndex;
			if (Nodes.IsValidIndex(PairedIdx))
			{
				auto GroupNameStr = Node._ProcessorName.ToString();
				GroupNameStr.RemoveFromStart(TEXT("GroupStart_"));
				_Processors[Idx].GroupName = FName(*GroupNameStr);
			}
		}
		else if (Node._IsGroupEnd)
		{
			auto GroupNameStr = Node._ProcessorName.ToString();
			GroupNameStr.RemoveFromStart(TEXT("GroupEnd_"));
			_Processors[Idx].GroupName = FName(*GroupNameStr);
		}
	}

	// Attach any write-conflict records that involve processors we just imported. Each partition
	// owns its conflicts (all pairs R1.3 flags are same-tick-group by construction), so this loop
	// only needs to look at the newly added slice [BaseProcessorIndex, _Processors.Num()).
	for (const auto& Conflict : Partition._WriteConflicts)
	{
		for (auto Idx = BaseProcessorIndex; Idx < _Processors.Num(); ++Idx)
		{
			const auto& ProcName = _Processors[Idx].ProcessorName;

			if (ProcName != Conflict._First && ProcName != Conflict._Second)
			{ continue; }

			auto Record = FCkSchedulerDebugger_WriteConflictInfo{};
			Record.PeerProcessorName = (ProcName == Conflict._First)
				? Conflict._Second
				: Conflict._First;
			Record.FragmentName = Conflict._FragmentName;
			Record.WasAutoResolved = Conflict._AutoInserted;

			_Processors[Idx].WriteConflicts.Add(MoveTemp(Record));
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugger_DataCollector::
	DoIdentifyGroups()
	-> void
{
	for (auto Idx = 0; Idx < _Processors.Num(); ++Idx)
	{
		const auto& Proc = _Processors[Idx];
		if (NOT Proc.IsGroupStart)
		{ continue; }

		auto GroupInfo = FCkSchedulerDebugger_GroupInfo{};

		auto GroupNameStr = Proc.ProcessorName.ToString();
		GroupNameStr.RemoveFromStart(TEXT("GroupStart_"));
		GroupInfo.GroupName = FName(*GroupNameStr);
		GroupInfo.DisplayName = GroupNameStr;
		GroupInfo.TickGroup = Proc.TickGroup;
		GroupInfo.StartNodeIndex = Idx;
		GroupInfo.AccentColor = FCkSchedulerDebuggerStyle::Get_GroupColor(GroupInfo.GroupName);

		const auto PairedNodeIndex = Proc.PairedGroupNodeIndex;
		auto EndIdx = static_cast<int32>(INDEX_NONE);
		for (auto SearchIdx = 0; SearchIdx < _Processors.Num(); ++SearchIdx)
		{
			if (_Processors[SearchIdx].NodeIndex == PairedNodeIndex
				&& _Processors[SearchIdx].IsGroupEnd)
			{
				EndIdx = SearchIdx;
				break;
			}
		}
		GroupInfo.EndNodeIndex = EndIdx;

		auto AggregateTime = 0.0;
		for (auto MemberIdx = 0; MemberIdx < _Processors.Num(); ++MemberIdx)
		{
			if (MemberIdx == Idx || MemberIdx == EndIdx)
			{ continue; }

			const auto& Member = _Processors[MemberIdx];
			if (Member.TickGroup != Proc.TickGroup)
			{ continue; }

			if (Member.ExecutionOrder > Proc.ExecutionOrder
				&& EndIdx != INDEX_NONE
				&& Member.ExecutionOrder < _Processors[EndIdx].ExecutionOrder)
			{
				GroupInfo.MemberIndices.Add(MemberIdx);
				_Processors[MemberIdx].GroupName = GroupInfo.GroupName;
				AggregateTime += Member.MainPassTimeMs;
			}
		}
		GroupInfo.AggregateTimeMs = AggregateTime;

		_Groups.Add(MoveTemp(GroupInfo));
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugger_DataCollector::
	DoBuildTreeHierarchy()
	-> void
{
	_TreeRoots.Reset();

	const TArray<ETickingGroup> TickGroups = {
		TG_PrePhysics,
		TG_DuringPhysics,
		TG_PostPhysics,
		TG_PostUpdateWork
	};

	const TMap<ETickingGroup, FString> TickGroupNames = {
		{ TG_PrePhysics, TEXT("Pre Physics") },
		{ TG_DuringPhysics, TEXT("During Physics") },
		{ TG_PostPhysics, TEXT("Post Physics") },
		{ TG_PostUpdateWork, TEXT("Post Update Work") }
	};

	for (const auto TickGroup : TickGroups)
	{
		auto HasProcessorsInGroup = false;
		for (const auto& Proc : _Processors)
		{
			if (Proc.TickGroup == TickGroup && NOT Proc.IsGroupStart && NOT Proc.IsGroupEnd)
			{
				HasProcessorsInGroup = true;
				break;
			}
		}

		if (NOT HasProcessorsInGroup)
		{ continue; }

		auto TickGroupNode = MakeShared<FCkSchedulerDebugger_TreeNode>();
		TickGroupNode->Type = ECkSchedulerDebugger_TreeNodeType::TickGroup;
		TickGroupNode->TickGroup = TickGroup;

		const auto* FoundName = TickGroupNames.Find(TickGroup);
		TickGroupNode->DisplayName = FoundName ? *FoundName : TEXT("Unknown");

		auto AssignedToGroup = TSet<int32>{};

		for (auto GroupIdx = 0; GroupIdx < _Groups.Num(); ++GroupIdx)
		{
			const auto& Group = _Groups[GroupIdx];
			if (Group.TickGroup != TickGroup)
			{ continue; }

			auto GroupNode = MakeShared<FCkSchedulerDebugger_TreeNode>();
			GroupNode->Type = ECkSchedulerDebugger_TreeNodeType::Group;
			GroupNode->DisplayName = Group.DisplayName;
			GroupNode->GroupIndex = GroupIdx;
			GroupNode->Parent = TickGroupNode;

			auto SortedMembers = Group.MemberIndices;
			SortedMembers.Sort([this](int32 A, int32 B)
			{
				return _Processors[A].ExecutionOrder < _Processors[B].ExecutionOrder;
			});

			for (const auto MemberIdx : SortedMembers)
			{
				const auto& Member = _Processors[MemberIdx];
				if (Member.IsGhost && NOT Member.HasDirtyMarker)
				{ continue; }

				auto ProcNode = MakeShared<FCkSchedulerDebugger_TreeNode>();
				ProcNode->Type = ECkSchedulerDebugger_TreeNodeType::Processor;
				ProcNode->DisplayName = Member.DisplayName;
				ProcNode->ProcessorIndex = MemberIdx;
				ProcNode->Parent = GroupNode;

				GroupNode->Children.Add(ProcNode);
				AssignedToGroup.Add(MemberIdx);
			}

			if (GroupNode->Children.Num() > 0)
			{
				TickGroupNode->Children.Add(GroupNode);
			}
		}

		auto UngroupedProcessors = TArray<int32>{};
		for (auto ProcIdx = 0; ProcIdx < _Processors.Num(); ++ProcIdx)
		{
			const auto& Proc = _Processors[ProcIdx];
			if (Proc.TickGroup != TickGroup)
			{ continue; }
			if (Proc.IsGroupStart || Proc.IsGroupEnd)
			{ continue; }
			if (AssignedToGroup.Contains(ProcIdx))
			{ continue; }

			UngroupedProcessors.Add(ProcIdx);
		}

		UngroupedProcessors.Sort([this](int32 A, int32 B)
		{
			return _Processors[A].ExecutionOrder < _Processors[B].ExecutionOrder;
		});

		for (const auto ProcIdx : UngroupedProcessors)
		{
			auto ProcNode = MakeShared<FCkSchedulerDebugger_TreeNode>();
			ProcNode->Type = ECkSchedulerDebugger_TreeNodeType::Processor;
			ProcNode->DisplayName = _Processors[ProcIdx].DisplayName;
			ProcNode->ProcessorIndex = ProcIdx;
			ProcNode->Parent = TickGroupNode;

			TickGroupNode->Children.Add(ProcNode);
		}

		if (TickGroupNode->Children.Num() > 0)
		{
			_TreeRoots.Add(TickGroupNode);
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugger_DataCollector::
	DoComputeDisplayName(
		const FName& InProcessorName)
	-> FString
{
	auto NameStr = InProcessorName.ToString();

	NameStr.RemoveFromStart(TEXT("FProcessor_"));
	NameStr.RemoveFromStart(TEXT("F"));

	TArray<FString> Segments;
	NameStr.ParseIntoArray(Segments, TEXT("_"));

	if (Segments.Num() >= 2)
	{
		return FString::Printf(TEXT("%s: %s"),
			*Segments[Segments.Num() - 2],
			*Segments[Segments.Num() - 1]);
	}

	if (Segments.Num() == 1)
	{
		return Segments[0];
	}

	return NameStr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugger_DataCollector::
	Get_Processors() const
	-> const TArray<FCkSchedulerDebugger_ProcessorInfo>&
{
	return _Processors;
}

auto
	FCkSchedulerDebugger_DataCollector::
	Get_Groups() const
	-> const TArray<FCkSchedulerDebugger_GroupInfo>&
{
	return _Groups;
}

auto
	FCkSchedulerDebugger_DataCollector::
	Get_TreeRoots() const
	-> const TArray<TSharedPtr<FCkSchedulerDebugger_TreeNode>>&
{
	return _TreeRoots;
}

auto
	FCkSchedulerDebugger_DataCollector::
	Get_TotalFrameTimeMs() const
	-> double
{
	return _TotalFrameTimeMs;
}

auto
	FCkSchedulerDebugger_DataCollector::
	Get_PumpCount() const
	-> int32
{
	return _PumpCount;
}

auto
	FCkSchedulerDebugger_DataCollector::
	Get_ProcessorCount() const
	-> int32
{
	return _ProcessorCount;
}

auto
	FCkSchedulerDebugger_DataCollector::
	Get_GhostCount() const
	-> int32
{
	return _GhostCount;
}

auto
	FCkSchedulerDebugger_DataCollector::
	Get_DirtyCount() const
	-> int32
{
	return _DirtyCount;
}

auto
	FCkSchedulerDebugger_DataCollector::
	Get_ParallelCount() const
	-> int32
{
	return _ParallelCount;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugger_DataCollector::
	DoUpdateTimingOnly(
		const ck::FProcessorScheduler& InScheduler,
		ETickingGroup InTickGroup)
	-> void
{
#if !UE_BUILD_SHIPPING
	const auto& FrameHistory = InScheduler.Get_DebugFrameHistory();
	if (FrameHistory.Num() == 0)
	{ return; }

	const auto& LatestSnapshot = FrameHistory.Last();
	_TotalFrameTimeMs += LatestSnapshot.TotalFrameTimeMs;
	_PumpCount = FMath::Max(_PumpCount, LatestSnapshot.PumpIterationCount);

	const auto& Partition = InScheduler.Get_Partition();
	const auto& Nodes = Partition._Nodes;

	for (auto ProcIdx = 0; ProcIdx < _Processors.Num(); ++ProcIdx)
	{
		auto& Proc = _Processors[ProcIdx];
		if (Proc.TickGroup != InTickGroup)
		{ continue; }

		const auto NodeIdx = Proc.NodeIndex;
		if (NOT LatestSnapshot.ProcessorTimings.IsValidIndex(NodeIdx))
		{ continue; }

		const auto& Timing = LatestSnapshot.ProcessorTimings[NodeIdx];
		Proc.MainPassTimeMs = Timing.MainPassTimeMs;
		Proc.PumpPassTimesMs = Timing.PumpPassTimesMs;
		Proc.WasDirtyThisFrame = Timing.WasDirtyThisFrame;
		Proc.PumpCountThisFrame = Timing.PumpCountThisFrame;
	}
#endif
}

// --------------------------------------------------------------------------------------------------------------------
