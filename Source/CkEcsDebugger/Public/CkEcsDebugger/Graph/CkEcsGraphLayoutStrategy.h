#pragma once

#include "CoreMinimal.h"

struct FCkGraphNode;
struct FCkGraphEdge;

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
