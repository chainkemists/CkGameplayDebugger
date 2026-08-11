#pragma once

#include "CkDebuggerCommon/Graph/CkDebugGraphLayout.h"
#include "CkSchedulerDebugger/Data/CkSchedulerDebugger_Types.h"
#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Runtime-only presentation model for the selected scheduler processor and its direct dependencies.
// It deliberately contains no editor graph types, so the same snapshot can drive a packaged Slate
// canvas.

struct CKSCHEDULERDEBUGGER_API FCkSchedulerRuntimeGraphNode
{
    int32 StableId = INDEX_NONE;
    FIntPoint Position = FIntPoint::ZeroValue;
    FCkSchedulerDebugger_ProcessorInfo Processor;
};

// --------------------------------------------------------------------------------------------------------------------

struct CKSCHEDULERDEBUGGER_API FCkSchedulerRuntimeGraphEdge
{
    int32 SourceId = INDEX_NONE;
    int32 TargetId = INDEX_NONE;
};

// --------------------------------------------------------------------------------------------------------------------

class CKSCHEDULERDEBUGGER_API FCkSchedulerRuntimeGraphModel
{
  public:
    // Returns true only when selected-node identity, subgraph membership, edges, or layout controls
    // changed. InSelectedProcessorId is a FCkSchedulerDebugger_ProcessorInfo::NodeIndex, never an
    // array offset.
    auto Rebuild(const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors,
                 int32 InSelectedProcessorId,
                 const FCkDebugGraphLayoutParams& InLayoutParams) -> bool;

    // Updates per-frame presentation data in place. Returns true when a displayed node changed.
    // Topology and positions intentionally remain untouched.
    auto Update_LiveState(const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors) -> bool;

    auto Reset() -> void;

    auto Get_Nodes() const -> const TArray<TSharedPtr<FCkSchedulerRuntimeGraphNode>>&
    {
        return _Nodes;
    }
    auto Get_Edges() const -> const TArray<FCkSchedulerRuntimeGraphEdge>&
    {
        return _Edges;
    }
    auto Get_SelectedProcessorId() const -> int32
    {
        return _SelectedProcessorId;
    }
    auto Get_NodeById(int32 InStableId) const -> TSharedPtr<FCkSchedulerRuntimeGraphNode>;

  private:
    auto Compute_TopologyHash(const TArray<FCkSchedulerDebugger_ProcessorInfo>& InProcessors,
                              int32 InSelectedProcessorId) const -> uint32;

    int32 _SelectedProcessorId = INDEX_NONE;
    FCkDebugGraphLayoutParams _LayoutParams;
    uint32 _TopologyHash = 0;
    TArray<TSharedPtr<FCkSchedulerRuntimeGraphNode>> _Nodes;
    TArray<FCkSchedulerRuntimeGraphEdge> _Edges;
};

// --------------------------------------------------------------------------------------------------------------------
