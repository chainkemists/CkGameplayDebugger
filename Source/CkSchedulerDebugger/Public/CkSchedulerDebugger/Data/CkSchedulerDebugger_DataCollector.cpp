#include "CkSchedulerDebugger_DataCollector.h"

#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"
#include "CkCore/String/CkFuzzyMatch_Utils.h"

#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcs/Scheduler/CkProcessorScheduler.h"
#include "CkEcs/Scheduler/CkSchedulerDebugData.h"

// --------------------------------------------------------------------------------------------------------------------

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

	// ---- Cache frame history summaries from all schedulers
	_CachedSchedulerHistories.Reset();
	for (const auto& [TickGroup, ActorPtr] : Subsystem->Get_WorldActors())
	{
		if (NOT ActorPtr.IsValid())
		{ continue; }
		const auto& SchedulerOpt = ActorPtr->Get_Scheduler();
		if (NOT SchedulerOpt.IsSet())
		{ continue; }

#if !UE_BUILD_SHIPPING
		auto CachedHistory = FCachedSchedulerHistory{};
		CachedHistory.TickGroup = TickGroup;
		CachedHistory.Snapshots = SchedulerOpt.GetValue().Get_DebugFrameHistory();
		_CachedSchedulerHistories.Add(MoveTemp(CachedHistory));
#endif
	}

	DoCacheFrameHistory();
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
		Info.DirtyMarkerHash = Node._DirtyMarkerHash;
		Info.DirtyMarkerName = Node._DirtyMarkerName;
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
			Info.MainPassEntityCount = Timing.MainPassEntityCount;
			Info.PumpPassEntityCounts = Timing.PumpPassEntityCounts;
		}

		constexpr auto MaxHistoryFrames = 3000;
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

	// ---- Second pass: compute parent-child relationships from execution order containment.
	// A group B is a child of group A if A's exec range fully contains B's exec range.
	// We pick the tightest (smallest) containing group as the parent.

	for (auto ChildIdx = 0; ChildIdx < _Groups.Num(); ++ChildIdx)
	{
		const auto& Child = _Groups[ChildIdx];
		if (Child.StartNodeIndex == INDEX_NONE || Child.EndNodeIndex == INDEX_NONE)
		{ continue; }

		const auto ChildStartExec = _Processors[Child.StartNodeIndex].ExecutionOrder;
		const auto ChildEndExec = _Processors[Child.EndNodeIndex].ExecutionOrder;

		auto BestParentIdx = static_cast<int32>(INDEX_NONE);
		auto BestParentSpan = INT32_MAX;

		for (auto ParentIdx = 0; ParentIdx < _Groups.Num(); ++ParentIdx)
		{
			if (ParentIdx == ChildIdx) { continue; }

			const auto& Parent = _Groups[ParentIdx];
			if (Parent.TickGroup != Child.TickGroup) { continue; }
			if (Parent.StartNodeIndex == INDEX_NONE || Parent.EndNodeIndex == INDEX_NONE)
			{ continue; }

			const auto ParentStartExec = _Processors[Parent.StartNodeIndex].ExecutionOrder;
			const auto ParentEndExec = _Processors[Parent.EndNodeIndex].ExecutionOrder;

			if (ParentStartExec < ChildStartExec && ParentEndExec > ChildEndExec)
			{
				const auto Span = ParentEndExec - ParentStartExec;
				if (Span < BestParentSpan)
				{
					BestParentSpan = Span;
					BestParentIdx = ParentIdx;
				}
			}
		}

		if (BestParentIdx != INDEX_NONE)
		{
			_Groups[ChildIdx].ParentGroupIndex = BestParentIdx;
			_Groups[BestParentIdx].ChildGroupIndices.Add(ChildIdx);
		}
	}

	// ---- Third pass: remove processors from parent groups that belong to a child group.
	// A processor should only appear in its most specific (leaf) group.

	for (auto GroupIdx = 0; GroupIdx < _Groups.Num(); ++GroupIdx)
	{
		auto& Group = _Groups[GroupIdx];
		if (Group.ChildGroupIndices.IsEmpty()) { continue; }

		auto ChildMembers = TSet<int32>{};
		for (const auto ChildGroupIdx : Group.ChildGroupIndices)
		{
			for (const auto MemberIdx : _Groups[ChildGroupIdx].MemberIndices)
			{
				ChildMembers.Add(MemberIdx);
			}
			// Also exclude the child group's start/end nodes
			ChildMembers.Add(_Groups[ChildGroupIdx].StartNodeIndex);
			ChildMembers.Add(_Groups[ChildGroupIdx].EndNodeIndex);
		}

		Group.MemberIndices.RemoveAll([&ChildMembers](int32 Idx)
		{
			return ChildMembers.Contains(Idx);
		});
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

		// ---- Recursive lambda to build a group node and its children (including nested sub-groups)

		TFunction<TSharedPtr<FCkSchedulerDebugger_TreeNode>(int32, TSharedPtr<FCkSchedulerDebugger_TreeNode>)>
			BuildGroupNode = [&](int32 GroupIdx, TSharedPtr<FCkSchedulerDebugger_TreeNode> InParentNode)
				-> TSharedPtr<FCkSchedulerDebugger_TreeNode>
		{
			const auto& Group = _Groups[GroupIdx];

			auto GroupNode = MakeShared<FCkSchedulerDebugger_TreeNode>();
			GroupNode->Type = ECkSchedulerDebugger_TreeNodeType::Group;
			GroupNode->DisplayName = Group.DisplayName;
			GroupNode->GroupIndex = GroupIdx;
			GroupNode->Parent = InParentNode;

			// ---- Add child groups first (in execution order)
			auto SortedChildGroups = Group.ChildGroupIndices;
			SortedChildGroups.Sort([this](int32 A, int32 B)
			{
				return _Processors[_Groups[A].StartNodeIndex].ExecutionOrder
					< _Processors[_Groups[B].StartNodeIndex].ExecutionOrder;
			});

			for (const auto ChildGroupIdx : SortedChildGroups)
			{
				auto ChildGroupNode = BuildGroupNode(ChildGroupIdx, GroupNode);
				if (ChildGroupNode.IsValid() && ChildGroupNode->Children.Num() > 0)
				{
					GroupNode->Children.Add(ChildGroupNode);
				}
			}

			// ---- Add direct member processors
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

			// ---- Mark group start/end nodes as assigned
			AssignedToGroup.Add(Group.StartNodeIndex);
			if (Group.EndNodeIndex != INDEX_NONE)
			{
				AssignedToGroup.Add(Group.EndNodeIndex);
			}

			return GroupNode;
		};

		// ---- Build only root-level groups (no parent) for this tick group
		for (auto GroupIdx = 0; GroupIdx < _Groups.Num(); ++GroupIdx)
		{
			const auto& Group = _Groups[GroupIdx];
			if (Group.TickGroup != TickGroup)
			{ continue; }
			if (Group.ParentGroupIndex != INDEX_NONE)
			{ continue; }

			auto GroupNode = BuildGroupNode(GroupIdx, TickGroupNode);
			if (GroupNode.IsValid() && GroupNode->Children.Num() > 0)
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
	// The canonical name comes from entt::type_name and looks like:
	//   "class ck::FProcessor_Inventory_HandleRequests"
	//   "class ck::TProcessor_AttributeModifier_EndPlayAll_CurrentMinMax<struct ck::TFragment_ByteAttributeModifier>"
	//   "class ck::TProcessor_Attribute_Replicate_All<struct ck::TFragment_ByteAttribute,struct FCk_RepData_ByteAttributes>"
	//   "class ck::TProcessor_SceneNode_Update<struct ck::FTag_SceneNode_Layer0>"
	//
	// Goal:
	//   Inventory: HandleRequests
	//   AttributeModifier_EndPlayAll: CurrentMinMax<Byte>
	//   Attribute_Replicate: All<Byte>
	//   SceneNode: Update<Layer0>

	auto StripNamespaceDecorations = [](FString& InOut)
	{
		InOut.RemoveFromStart(TEXT("class "));
		InOut.RemoveFromStart(TEXT("struct "));
		InOut.RemoveFromStart(TEXT("ck::"));
	};

	auto SimplifyTemplateArg = [&](FString InArg) -> FString
	{
		InArg.TrimStartAndEndInline();
		StripNamespaceDecorations(InArg);
		InArg.RemoveFromStart(TEXT("TFragment_"));
		InArg.RemoveFromStart(TEXT("FFragment_"));
		InArg.RemoveFromStart(TEXT("FTag_"));
		InArg.RemoveFromStart(TEXT("FCk_"));

		// Strip common noisy suffixes so e.g. ByteAttributeModifier → Byte, FloatAttribute → Float.
		const auto TrySuffixStrip = [&](const TCHAR* Suffix) -> bool
		{
			if (InArg.EndsWith(Suffix))
			{
				InArg.LeftChopInline(FCString::Strlen(Suffix));
				return true;
			}
			return false;
		};

		if (NOT TrySuffixStrip(TEXT("AttributeModifier"))
			&& NOT TrySuffixStrip(TEXT("Attribute")))
		{
			// For names like "SceneNode_Layer0", keep only the final _-separated segment.
			int32 UnderscoreIdx = INDEX_NONE;
			if (InArg.FindLastChar(TCHAR('_'), UnderscoreIdx))
			{
				InArg = InArg.Mid(UnderscoreIdx + 1);
			}
		}

		return InArg;
	};

	auto NameStr = InProcessorName.ToString();
	StripNamespaceDecorations(NameStr);

	// Split off the first template argument (if present) so the main name is free of angle brackets.
	auto MainName     = NameStr;
	auto TemplateArg  = FString{};
	int32 TemplateOpen = INDEX_NONE;
	if (NameStr.FindChar(TCHAR('<'), TemplateOpen))
	{
		MainName = NameStr.Left(TemplateOpen);

		auto ArgsStr = NameStr.Mid(TemplateOpen + 1);
		if (ArgsStr.EndsWith(TEXT(">")))
		{ ArgsStr.LeftChopInline(1); }

		// Only surface the first argument; the rest (e.g. FCk_RepData_*) is redundant for display.
		int32 CommaIdx = INDEX_NONE;
		if (ArgsStr.FindChar(TCHAR(','), CommaIdx))
		{
			ArgsStr = ArgsStr.Left(CommaIdx);
		}

		TemplateArg = SimplifyTemplateArg(MoveTemp(ArgsStr));
	}

	MainName.RemoveFromStart(TEXT("FProcessor_"));
	MainName.RemoveFromStart(TEXT("TProcessor_"));
	MainName.RemoveFromStart(TEXT("F"));

	TArray<FString> Segments;
	MainName.ParseIntoArray(Segments, TEXT("_"));

	auto Display = FString{};
	if (Segments.Num() >= 2)
	{
		// Join all but the last segment with '_' as the Category, and the final segment as the Action.
		// For two-segment names this yields "Inventory: HandleRequests"; for three-plus-segment names
		// it yields "AttributeModifier_EndPlayAll: CurrentMinMax" — which preserves the full category
		// path rather than collapsing to a misleading suffix-only pair.
		const auto Category = FString::Join(
			TArrayView<const FString>(Segments.GetData(), Segments.Num() - 1), TEXT("_"));
		Display = FString::Printf(TEXT("%s: %s"), *Category, *Segments.Last());
	}
	else if (Segments.Num() == 1)
	{
		Display = Segments[0];
	}
	else
	{
		Display = MainName;
	}

	if (NOT TemplateArg.IsEmpty())
	{
		Display = FString::Printf(TEXT("%s<%s>"), *Display, *TemplateArg);
	}

	return Display;
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
		Proc.MainPassEntityCount = Timing.MainPassEntityCount;
		Proc.PumpPassEntityCounts = Timing.PumpPassEntityCounts;
	}
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugger_DataCollector::
	Get_FrameSnapshots() const
	-> const TArray<FCkSchedulerDebugger_FrameSnapshotInfo>&
{
	return _FrameSnapshots;
}

auto
	FCkSchedulerDebugger_DataCollector::
	Get_FrameSnapshotCount() const
	-> int32
{
	return _FrameSnapshots.Num();
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugger_DataCollector::
	FrameContainsProcessor(
		int32 InSnapshotIdx,
		const FString& InFilter) const
	-> bool
{
#if !UE_BUILD_SHIPPING
	if (InFilter.IsEmpty())
	{ return false; }

	if (_CachedSchedulerHistories.Num() == 0)
	{ return false; }

	// _FrameSnapshots is built from _CachedSchedulerHistories[0] 1:1, so indices align.
	const auto& PrimaryHistory = _CachedSchedulerHistories[0].Snapshots;
	if (NOT PrimaryHistory.IsValidIndex(InSnapshotIdx))
	{ return false; }

	const auto& Timings = PrimaryHistory[InSnapshotIdx].ProcessorTimings;
	for (const auto& Timing : Timings)
	{
		// Only match processors that processed at least 1 entity this frame
		// (main pass or any pump pass). Processors with 0 entities are skipped
		// even if they have non-zero timing overhead.
		auto EntityCount = Timing.MainPassEntityCount;
		for (const auto PumpCount : Timing.PumpPassEntityCounts)
		{
			EntityCount += PumpCount;
		}
		if (EntityCount <= 0)
		{ continue; }

		const auto Name = Timing.ProcessorName.ToString();
		if (ck::fuzzy::Match(InFilter, Name, {}).Get_IsMatch())
		{
			return true;
		}
	}
#endif
	return false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugger_DataCollector::
	DoCacheFrameHistory()
	-> void
{
#if !UE_BUILD_SHIPPING
	_FrameSnapshots.Reset();

	// Merge frame histories from all schedulers (usually just one) into a single timeline
	// keyed by FrameNumber. For simplicity, use the first scheduler's history as the primary
	// timeline (multiple schedulers in different tick groups share the same frame numbers).

	if (_CachedSchedulerHistories.Num() == 0)
	{ return; }

	const auto& PrimaryHistory = _CachedSchedulerHistories[0].Snapshots;
	_FrameSnapshots.Reserve(PrimaryHistory.Num());

	for (const auto& Snapshot : PrimaryHistory)
	{
		auto Info = FCkSchedulerDebugger_FrameSnapshotInfo{};
		Info.FrameNumber = Snapshot.FrameNumber;
		Info.TotalFrameTimeMs = Snapshot.TotalFrameTimeMs;
		Info.PumpIterationCount = Snapshot.PumpIterationCount;

		auto DirtyCount = 0;
		for (const auto& Timing : Snapshot.ProcessorTimings)
		{
			if (Timing.WasDirtyThisFrame) { ++DirtyCount; }
		}
		Info.DirtyProcessorCount = DirtyCount;

		_FrameSnapshots.Add(MoveTemp(Info));
	}
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugger_DataCollector::
	ApplyFrameSnapshot(
		int32 InOffset)
	-> void
{
#if !UE_BUILD_SHIPPING
	if (_CachedSchedulerHistories.Num() == 0)
	{ return; }

	// Apply timing data from the historical frame at InOffset (0=latest, 1=one back, ...)
	// to the current _Processors array.

	_TotalFrameTimeMs = 0.0;
	_PumpCount = 0;

	for (const auto& CachedHistory : _CachedSchedulerHistories)
	{
		const auto& Snapshots = CachedHistory.Snapshots;
		if (Snapshots.Num() == 0)
		{ continue; }

		const auto SnapshotIdx = Snapshots.Num() - 1 - InOffset;
		if (SnapshotIdx < 0 || SnapshotIdx >= Snapshots.Num())
		{ continue; }

		const auto& Snapshot = Snapshots[SnapshotIdx];
		_TotalFrameTimeMs += Snapshot.TotalFrameTimeMs;
		_PumpCount = FMath::Max(_PumpCount, Snapshot.PumpIterationCount);

		for (auto ProcIdx = 0; ProcIdx < _Processors.Num(); ++ProcIdx)
		{
			auto& Proc = _Processors[ProcIdx];
			if (Proc.TickGroup != CachedHistory.TickGroup)
			{ continue; }

			const auto NodeIdx = Proc.NodeIndex;
			if (NOT Snapshot.ProcessorTimings.IsValidIndex(NodeIdx))
			{ continue; }

			const auto& Timing = Snapshot.ProcessorTimings[NodeIdx];
			Proc.MainPassTimeMs = Timing.MainPassTimeMs;
			Proc.PumpPassTimesMs = Timing.PumpPassTimesMs;
			Proc.WasDirtyThisFrame = Timing.WasDirtyThisFrame;
			Proc.PumpCountThisFrame = Timing.PumpCountThisFrame;
			Proc.MainPassEntityCount = Timing.MainPassEntityCount;
			Proc.PumpPassEntityCounts = Timing.PumpPassEntityCounts;
		}
	}

	// Recompute group aggregate times
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
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugger_DataCollector::
	RestoreLiveData()
	-> void
{
	ApplyFrameSnapshot(0);
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugger_DataCollector::
	Set_FrameHistoryMaxSize(
		UWorld* InWorld,
		int32 InMaxFrames)
	-> void
{
#if !UE_BUILD_SHIPPING
	if (NOT IsValid(InWorld))
	{ return; }

	auto Subsystem = InWorld->GetSubsystem<UCk_EcsWorld_Subsystem_UE>();
	if (NOT IsValid(Subsystem))
	{ return; }

	const auto ClampedMax = FMath::Max(InMaxFrames, 10);

	for (const auto& [TickGroup, ActorPtr] : Subsystem->Get_WorldActors())
	{
		if (NOT ActorPtr.IsValid())
		{ continue; }
		const auto& SchedulerOpt = ActorPtr->Get_Scheduler();
		if (NOT SchedulerOpt.IsSet())
		{ continue; }

		// const_cast is acceptable here: this is a debug-only setter for a debug-only buffer size,
		// and the world actor only exposes const access to the scheduler.
		auto& MutableScheduler = const_cast<ck::FProcessorScheduler&>(SchedulerOpt.GetValue());
		MutableScheduler.Set_DebugFrameHistoryMax(ClampedMax);
	}
#endif
}

// --------------------------------------------------------------------------------------------------------------------
