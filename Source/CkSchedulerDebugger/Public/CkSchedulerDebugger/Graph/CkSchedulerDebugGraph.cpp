#if WITH_EDITOR

#include "CkSchedulerDebugger/Graph/CkSchedulerDebugGraph.h"
#include "CkSchedulerDebugger/Graph/CkSchedulerDebugNode_Processor.h"
#include "CkSchedulerDebugger/Graph/CkSchedulerDebugGraphSchema.h"
#include "CkSchedulerDebugger/Data/CkSchedulerDebugger_Types.h"

// --------------------------------------------------------------------------------------------------------------------

auto
	UCkSchedulerDebugGraph::
	RebuildFromData(
		const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors)
	-> void
{
	const auto NewHash = ComputeTopologyHash(InProcessors);

	if (NewHash == _TopologyHash && _TopologyHash != 0)
	{
		UpdateTimingData(InProcessors);
		return;
	}

	_TopologyHash = NewHash;
	_LastProcessors = InProcessors;

	Nodes.Empty();

	SetSuppressNotifications(true);

	Schema = UCkSchedulerDebugGraphSchema::StaticClass();

	DoCreateNodes(InProcessors);
	DoCreateEdges(InProcessors);
	DoLayoutNodes(InProcessors);

	SetSuppressNotifications(false);
	NotifyGraphChanged();
}

// --------------------------------------------------------------------------------------------------------------------

auto
	UCkSchedulerDebugGraph::
	DoCreateNodes(
		const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors)
	-> void
{
	for (const auto& Info : InProcessors)
	{
		auto Node = NewObject<UCkSchedulerDebugNode_Processor>(this);

		Node->ProcessorNodeIndex = Info.NodeIndex;
		Node->ProcessorName      = Info.ProcessorName;
		Node->DisplayName        = Info.DisplayName;
		Node->GroupName          = Info.GroupName;
		Node->IsGhost            = Info.IsGhost;
		Node->IsGroupStart       = Info.IsGroupStart;
		Node->IsGroupEnd         = Info.IsGroupEnd;
		Node->HasDirtyMarker     = Info.HasDirtyMarker;
		Node->IsParallel         = Info.IsParallel;
		Node->LastFrameTimeMs    = Info.MainPassTimeMs;
		Node->PumpCountThisFrame = Info.PumpCountThisFrame;
		Node->WasDirtyThisFrame  = Info.WasDirtyThisFrame;
		Node->ExecutionOrder     = Info.ExecutionOrder;

		Node->CreateNewGuid();
		Node->AllocateDefaultPins();

		constexpr auto FromUI = false;
		constexpr auto SelectNewNode = false;
		AddNode(Node, FromUI, SelectNewNode);
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	UCkSchedulerDebugGraph::
	DoCreateEdges(
		const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors)
	-> void
{
	for (const auto& Info : InProcessors)
	{
		auto SourceNode = FindNodeByProcessorIndex(Info.NodeIndex);
		if (NOT SourceNode) { continue; }

		auto SourcePin = SourceNode->GetOutputPin();
		if (NOT SourcePin) { continue; }

		for (const auto TargetIndex : Info.OutEdges)
		{
			auto TargetNode = FindNodeByProcessorIndex(TargetIndex);
			if (NOT TargetNode) { continue; }

			auto TargetPin = TargetNode->GetInputPin();
			if (NOT TargetPin) { continue; }

			SourcePin->MakeLinkTo(TargetPin);
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	UCkSchedulerDebugGraph::
	DoLayoutNodes(
		const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors)
	-> void
{
	// Build layout input from processor data
	auto LayoutNodes = TArray<FCkDebugGraphLayoutNode>{};
	for (const auto& Info : InProcessors)
	{ LayoutNodes.Add({ Info.NodeIndex }); }

	auto LayoutEdges = TArray<FCkDebugGraphLayoutEdge>{};
	for (const auto& Info : InProcessors)
	{
		for (const auto TargetIndex : Info.OutEdges)
		{ LayoutEdges.Add({ Info.NodeIndex, TargetIndex }); }
	}

	// Find BFS root: prefer first non-ghost group start node
	auto InitialNodeIndex = static_cast<int32>(INDEX_NONE);
	for (const auto& Info : InProcessors)
	{
		if (Info.IsGroupStart && NOT Info.IsGhost)
		{
			InitialNodeIndex = Info.NodeIndex;
			break;
		}
	}
	if (InitialNodeIndex == INDEX_NONE && InProcessors.Num() > 0)
	{ InitialNodeIndex = InProcessors[0].NodeIndex; }

	auto Params = LayoutParams;
	Params.InitialNodeIndex = InitialNodeIndex;

	auto Result = FCkDebugGraphLayout::ComputeLayout(LayoutNodes, LayoutEdges, Params);

	// Apply positions
	for (auto Node : Nodes)
	{
		auto ProcessorNode = Cast<UCkSchedulerDebugNode_Processor>(Node);
		if (NOT ProcessorNode) { continue; }

		auto* Pos = Result.Positions.Find(ProcessorNode->ProcessorNodeIndex);
		if (Pos)
		{
			ProcessorNode->NodePosX = Pos->X;
			ProcessorNode->NodePosY = Pos->Y;
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	UCkSchedulerDebugGraph::
	UpdateTimingData(
		const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors)
	-> void
{
	for (const auto& Info : InProcessors)
	{
		auto Node = FindNodeByProcessorIndex(Info.NodeIndex);
		if (NOT Node) { continue; }

		Node->LastFrameTimeMs    = Info.MainPassTimeMs;
		Node->PumpCountThisFrame = Info.PumpCountThisFrame;
		Node->WasDirtyThisFrame  = Info.WasDirtyThisFrame;
		Node->HasDirtyMarker     = Info.HasDirtyMarker;
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	UCkSchedulerDebugGraph::
	FindNodeByProcessorIndex(int32 InProcessorIndex) const
	-> UCkSchedulerDebugNode_Processor*
{
	for (auto Node : Nodes)
	{
		auto ProcessorNode = Cast<UCkSchedulerDebugNode_Processor>(Node);
		if (ProcessorNode && ProcessorNode->ProcessorNodeIndex == InProcessorIndex)
		{
			return ProcessorNode;
		}
	}
	return nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	UCkSchedulerDebugGraph::
	ComputeTopologyHash(
		const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors) const
	-> uint32
{
	auto Hash = uint32{0};

	for (const auto& Info : InProcessors)
	{
		Hash = HashCombine(Hash, GetTypeHash(Info.ProcessorName));
		for (const auto Edge : Info.OutEdges)
		{
			Hash = HashCombine(Hash, GetTypeHash(Edge));
		}
	}

	return Hash;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	UCkSchedulerDebugGraph::
	RequestRelayout()
	-> void
{
	if (_LastProcessors.Num() == 0) { return; }

	_TopologyHash = 0;

	Nodes.Empty();
	SetSuppressNotifications(true);

	Schema = UCkSchedulerDebugGraphSchema::StaticClass();

	DoCreateNodes(_LastProcessors);
	DoCreateEdges(_LastProcessors);
	DoLayoutNodes(_LastProcessors);

	SetSuppressNotifications(false);
	NotifyGraphChanged();
}

// --------------------------------------------------------------------------------------------------------------------

#endif
