#pragma once

#include "CoreMinimal.h"

struct FCkGraphNode;
struct FCkGraphEdge;
enum class ECkGraphEdgeType : uint8;

struct FCkGraphNodePosition
{
    int32 NodeIndex = INDEX_NONE;
    FVector2D Position = FVector2D::ZeroVector;
};

class ICkEcsGraphLayoutStrategy
{
public:
    virtual ~ICkEcsGraphLayoutStrategy() = default;

    virtual auto ComputeLayout(
        const TArray<FCkGraphNode>& InNodes,
        const TArray<FCkGraphEdge>& InEdges,
        int32 InCenterNodeIndex) -> TArray<FCkGraphNodePosition> = 0;
};

/**
 * Directional layout: positions nodes based on their relationship type.
 *
 *   Owners (Lifetime, Context) go ABOVE the center node.
 *   Children (SceneNode) go BELOW.
 *   Within each group, nodes spread horizontally.
 */
class FCkDirectionalGraphLayout : public ICkEcsGraphLayoutStrategy
{
public:
    auto ComputeLayout(
        const TArray<FCkGraphNode>& InNodes,
        const TArray<FCkGraphEdge>& InEdges,
        int32 InCenterNodeIndex) -> TArray<FCkGraphNodePosition> override;

private:
    static constexpr float VerticalSpacing = 120.0f;
    static constexpr float HorizontalSpacing = 170.0f;
};

// Keep radial as an option for future use
class FCkRadialGraphLayout : public ICkEcsGraphLayoutStrategy
{
public:
    auto ComputeLayout(
        const TArray<FCkGraphNode>& InNodes,
        const TArray<FCkGraphEdge>& InEdges,
        int32 InCenterNodeIndex) -> TArray<FCkGraphNodePosition> override;

private:
    static constexpr float RingRadius = 200.0f;
};
