#include "CkSchedulerDebugger/Graph/CkSchedulerRuntimeGraphModel.h"

namespace ck_scheduler_runtime_graph_model
{
    auto FindProcessorById(const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors,
                           int32 InProcessorId) -> const FCkSchedulerDebugger_ProcessorInfo*
    {
        return InProcessors.FindByPredicate(
            [InProcessorId](const FCkSchedulerDebugger_ProcessorInfo& InInfo)
            {
                return InInfo.NodeIndex == InProcessorId;
            });
    }

    auto FindProcessorIndexById(const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors,
                                const int32 InProcessorId) -> int32
    {
        return InProcessors.IndexOfByPredicate(
            [InProcessorId](const FCkSchedulerDebugger_ProcessorInfo& InInfo)
            {
                return InInfo.NodeIndex == InProcessorId;
            });
    }

    auto HasSameLiveState(const FCkSchedulerDebugger_ProcessorInfo& InA,
                          const FCkSchedulerDebugger_ProcessorInfo& InB) -> bool
    {
        return InA.MainPassTimeMs == InB.MainPassTimeMs &&
               InA.PumpPassTimesMs == InB.PumpPassTimesMs &&
               InA.HasDirtyMarker == InB.HasDirtyMarker &&
               InA.WasDirtyThisFrame == InB.WasDirtyThisFrame &&
               InA.PumpCountThisFrame == InB.PumpCountThisFrame &&
               InA.MainPassEntityCount == InB.MainPassEntityCount &&
               InA.PumpPassEntityCounts == InB.PumpPassEntityCounts &&
               InA.TotalTicks == InB.TotalTicks && InA.TickRate == InB.TickRate &&
               InA.TimingHistory == InB.TimingHistory;
    }

    auto CopyLiveState(FCkSchedulerDebugger_ProcessorInfo& OutTarget,
                       const FCkSchedulerDebugger_ProcessorInfo& InSource) -> void
    {
        OutTarget.MainPassTimeMs = InSource.MainPassTimeMs;
        OutTarget.PumpPassTimesMs = InSource.PumpPassTimesMs;
        OutTarget.HasDirtyMarker = InSource.HasDirtyMarker;
        OutTarget.WasDirtyThisFrame = InSource.WasDirtyThisFrame;
        OutTarget.PumpCountThisFrame = InSource.PumpCountThisFrame;
        OutTarget.MainPassEntityCount = InSource.MainPassEntityCount;
        OutTarget.PumpPassEntityCounts = InSource.PumpPassEntityCounts;
        OutTarget.TotalTicks = InSource.TotalTicks;
        OutTarget.TickRate = InSource.TickRate;
        OutTarget.TimingHistory = InSource.TimingHistory;
    }
} // namespace ck_scheduler_runtime_graph_model

// --------------------------------------------------------------------------------------------------------------------

auto FCkSchedulerRuntimeGraphModel::Rebuild(
    const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors,
    int32 InSelectedProcessorId,
    const FCkDebugGraphLayoutParams& InLayoutParams) -> bool
{
    const auto* Selected =
        ck_scheduler_runtime_graph_model::FindProcessorById(InProcessors, InSelectedProcessorId);
    if (Selected == nullptr)
    {
        const auto bWasPopulated = _Nodes.Num() > 0 || _SelectedProcessorId != INDEX_NONE;
        Reset();
        return bWasPopulated;
    }

    // Preserve the previous detail graph's construction order exactly: it collected array indices
    // into a TSet and then iterated that set to create the subset passed to the graph layout.
    const auto SelectedArrayIndex =
        ck_scheduler_runtime_graph_model::FindProcessorIndexById(InProcessors, InSelectedProcessorId);
    auto IncludedArrayIndices = TSet<int32>{SelectedArrayIndex};
    for (const auto Id : Selected->InEdges)
    {
        const auto ArrayIndex = ck_scheduler_runtime_graph_model::FindProcessorIndexById(InProcessors, Id);
        if (ArrayIndex != INDEX_NONE)
        {
            IncludedArrayIndices.Add(ArrayIndex);
        }
    }
    for (const auto Id : Selected->OutEdges)
    {
        const auto ArrayIndex = ck_scheduler_runtime_graph_model::FindProcessorIndexById(InProcessors, Id);
        if (ArrayIndex != INDEX_NONE)
        {
            IncludedArrayIndices.Add(ArrayIndex);
        }
    }
    auto SubgraphProcessors = TArray<const FCkSchedulerDebugger_ProcessorInfo*>{};
    SubgraphProcessors.Reserve(IncludedArrayIndices.Num());
    auto IncludedIds = TSet<int32>{};
    for (const auto ArrayIndex : IncludedArrayIndices)
    {
        const auto& Processor = InProcessors[ArrayIndex];
        SubgraphProcessors.Add(&Processor);
        IncludedIds.Add(Processor.NodeIndex);
    }

    const auto NewHash = Compute_TopologyHash(InProcessors, InSelectedProcessorId);
    const auto bSameTopology = _SelectedProcessorId == InSelectedProcessorId &&
                               _TopologyHash == NewHash &&
                               _LayoutParams.SpacingX == InLayoutParams.SpacingX &&
                               _LayoutParams.SpacingY == InLayoutParams.SpacingY &&
                               _LayoutParams.CrossingReductionPasses ==
                                   InLayoutParams.CrossingReductionPasses &&
                               _LayoutParams.IsDirectedBFS == InLayoutParams.IsDirectedBFS;
    if (bSameTopology)
    {
        Update_LiveState(InProcessors);
        return false;
    }

    _SelectedProcessorId = InSelectedProcessorId;
    _LayoutParams = InLayoutParams;
    _TopologyHash = NewHash;
    _Nodes.Reset(SubgraphProcessors.Num());
    _Edges.Reset();

    auto LayoutNodes = TArray<FCkDebugGraphLayoutNode>{};
    auto LayoutEdges = TArray<FCkDebugGraphLayoutEdge>{};
    for (const auto* Processor : SubgraphProcessors)
    {
        auto RuntimeNode = MakeShared<FCkSchedulerRuntimeGraphNode>();
        RuntimeNode->StableId = Processor->NodeIndex;
        RuntimeNode->Processor = *Processor;
        _Nodes.Add(MoveTemp(RuntimeNode));
        LayoutNodes.Add({Processor->NodeIndex});
        for (const auto TargetId : Processor->OutEdges)
        {
            if (IncludedIds.Contains(TargetId))
            {
                LayoutEdges.Add({Processor->NodeIndex, TargetId});
                _Edges.Add({Processor->NodeIndex, TargetId});
            }
        }
    }

    // Keep the editor graph's root policy: first non-ghost group start, otherwise the first
    // subgraph node.
    auto Params = InLayoutParams;
    Params.InitialNodeIndex = INDEX_NONE;
    for (const auto* Processor : SubgraphProcessors)
    {
        if (Processor->IsGroupStart && NOT Processor->IsGhost)
        {
            Params.InitialNodeIndex = Processor->NodeIndex;
            break;
        }
    }
    if (Params.InitialNodeIndex == INDEX_NONE && SubgraphProcessors.Num() > 0)
    {
        Params.InitialNodeIndex = SubgraphProcessors[0]->NodeIndex;
    }

    const auto Layout = FCkDebugGraphLayout::ComputeLayout(LayoutNodes, LayoutEdges, Params);
    for (const auto& Node : _Nodes)
    {
        if (Node.IsValid())
        {
            if (const auto* Position = Layout.Positions.Find(Node->StableId))
            {
                Node->Position = *Position;
            }
        }
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkSchedulerRuntimeGraphModel::Update_LiveState(
    const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors) -> bool
{
    auto bChanged = false;
    for (const auto& Node : _Nodes)
    {
        if (NOT Node.IsValid())
        {
            continue;
        }
        const auto* Latest = ck_scheduler_runtime_graph_model::FindProcessorById(InProcessors,
                                                                                 Node->StableId);
        if (Latest == nullptr ||
            ck_scheduler_runtime_graph_model::HasSameLiveState(Node->Processor, *Latest))
        {
            continue;
        }
        ck_scheduler_runtime_graph_model::CopyLiveState(Node->Processor, *Latest);
        bChanged = true;
    }
    return bChanged;
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkSchedulerRuntimeGraphModel::Reset() -> void
{
    _SelectedProcessorId = INDEX_NONE;
    _TopologyHash = 0;
    _Nodes.Reset();
    _Edges.Reset();
}

auto FCkSchedulerRuntimeGraphModel::Get_NodeById(const int32 InStableId) const
    -> TSharedPtr<FCkSchedulerRuntimeGraphNode>
{
    const auto* Found = _Nodes.FindByPredicate(
        [InStableId](const TSharedPtr<FCkSchedulerRuntimeGraphNode>& InNode)
        {
            return InNode.IsValid() && InNode->StableId == InStableId;
        });
    return Found != nullptr ? *Found : nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkSchedulerRuntimeGraphModel::Compute_TopologyHash(
    const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors,
    const int32 InSelectedProcessorId) const -> uint32
{
    const auto* Selected =
        ck_scheduler_runtime_graph_model::FindProcessorById(InProcessors, InSelectedProcessorId);
    if (Selected == nullptr)
    {
        return 0;
    }

    auto IncludedIds = TSet<int32>{InSelectedProcessorId};
    for (const auto Id : Selected->InEdges)
    {
        IncludedIds.Add(Id);
    }
    for (const auto Id : Selected->OutEdges)
    {
        IncludedIds.Add(Id);
    }

    auto Hash = uint32{0};
    for (const auto& Processor : InProcessors)
    {
        if (NOT IncludedIds.Contains(Processor.NodeIndex))
        {
            continue;
        }
        Hash = HashCombine(Hash, GetTypeHash(Processor.NodeIndex));
        Hash = HashCombine(Hash, GetTypeHash(Processor.ProcessorName));
        Hash = HashCombine(Hash, GetTypeHash(Processor.DisplayName));
        Hash = HashCombine(Hash, GetTypeHash(Processor.GroupName));
        Hash = HashCombine(Hash, GetTypeHash(Processor.ExecutionOrder));
        Hash = HashCombine(Hash, GetTypeHash(Processor.IsGhost));
        Hash = HashCombine(Hash, GetTypeHash(Processor.IsGroupStart));
        Hash = HashCombine(Hash, GetTypeHash(Processor.IsParallel));
        if (Processor.NodeIndex == InSelectedProcessorId)
        {
            for (const auto SourceId : Processor.InEdges)
            {
                Hash = HashCombine(Hash, GetTypeHash(SourceId));
            }
        }
        for (const auto TargetId : Processor.OutEdges)
        {
            if (IncludedIds.Contains(TargetId))
            {
                Hash = HashCombine(Hash, GetTypeHash(TargetId));
            }
        }
    }
    return Hash;
}

// --------------------------------------------------------------------------------------------------------------------
