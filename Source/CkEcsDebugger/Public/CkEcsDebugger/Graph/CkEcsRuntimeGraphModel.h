#pragma once

#include "CkEcs/Handle/CkHandle.h"
#include "CoreMinimal.h"

// Runtime-only representation of the ECS relationship graph.  It deliberately
// contains no UEdGraph or GraphEditor types so the same topology can be drawn
// by a Slate canvas in a packaged game.

enum class ECkEcsRuntimeGraphRelationship : uint8
{
    None,
    LifetimeOwner,
    ContextOwner,
    LifetimeDependent,
};

struct FCkEcsRuntimeGraphLayoutParams
{
    static constexpr auto MinNodeWidth = 200;
    static constexpr auto NodeGap = 50;
    static constexpr auto SpacingY = 200;
    static constexpr auto MaxDependentsPerRow = 6;
    static constexpr auto CharWidthEstimate = 8.0f;
    static constexpr auto NodePaddingX = 40;
};

struct FCkEcsRuntimeGraphLayoutInput
{
    FString CenterLabel;
    TArray<FString> OwnerLabels;
    TArray<FString> DependentLabels;
};

struct FCkEcsRuntimeGraphLayoutResult
{
    int32 DynamicSpacingY = FCkEcsRuntimeGraphLayoutParams::SpacingY;
    FIntPoint CenterPosition = FIntPoint::ZeroValue;
    TArray<int32> OwnerWidths;
    TArray<FIntPoint> OwnerPositions;
    TArray<int32> DependentWidths;
    TArray<FIntPoint> DependentPositions;
};

struct FCkEcsRuntimeGraphNode
{
    FString StableId;
    FCk_Handle Entity;
    FString DisplayName;
    FLinearColor AccentColor = FLinearColor::Transparent;
    FIntPoint Position = FIntPoint::ZeroValue;
    ECkEcsRuntimeGraphRelationship Relationship = ECkEcsRuntimeGraphRelationship::None;
    bool bIsCenterNode = false;
    bool bIsFilterMatch = true;
};

struct FCkEcsRuntimeGraphEdge
{
    FString SourceNodeId;
    FString TargetNodeId;
    FLinearColor Color = FLinearColor::Transparent;
    ECkEcsRuntimeGraphRelationship Relationship = ECkEcsRuntimeGraphRelationship::None;
    bool bIsDirected = true;
};

namespace ck::ecs_runtime_graph
{
    CKECSDEBUGGER_API auto EstimateNodeWidth(const FString& InLabel) -> int32;
    CKECSDEBUGGER_API auto BuildLayout(const FCkEcsRuntimeGraphLayoutInput& InInput)
        -> FCkEcsRuntimeGraphLayoutResult;
    CKECSDEBUGGER_API auto GetRelationshipColor(ECkEcsRuntimeGraphRelationship InRelationship)
        -> FLinearColor;
} // namespace ck::ecs_runtime_graph

class CKECSDEBUGGER_API FCkEcsRuntimeGraphModel
{
  public:
    // Returns true when the visible graph changed. Invalid input clears every
    // value atomically and never leaves a graph for the prior selection behind.
    auto RebuildFromEntity(const FCk_Handle& InEntity) -> bool;
    auto Clear() -> bool;

    auto GetNodes() const -> const TArray<TSharedPtr<FCkEcsRuntimeGraphNode>>&
    {
        return _Nodes;
    }
    auto GetEdges() const -> const TArray<FCkEcsRuntimeGraphEdge>&
    {
        return _Edges;
    }
    auto IsEmpty() const -> bool
    {
        return _Nodes.IsEmpty();
    }

    static auto MakeStableId(const FCk_Handle& InEntity) -> FString;
    static auto ComputeTopologyHash(const FCk_Handle& InCenter,
                                    const FCk_Handle& InLifetimeOwner,
                                    const FCk_Handle& InContextOwner,
                                    TConstArrayView<FCk_Handle> InDependents) -> uint32;

  private:
    FCk_Handle _LastBuiltEntity;
    uint32 _LastTopologyHash = 0;
    TArray<TSharedPtr<FCkEcsRuntimeGraphNode>> _Nodes;
    TArray<FCkEcsRuntimeGraphEdge> _Edges;
};
