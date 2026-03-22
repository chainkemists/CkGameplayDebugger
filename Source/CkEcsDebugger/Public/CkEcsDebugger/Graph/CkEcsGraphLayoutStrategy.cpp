#include "CkEcsGraphLayoutStrategy.h"

#include "CkEcsDebugger/Graph/CkEcsGraphModel.h"

// =====================================================================================================================
// Directional Layout
// =====================================================================================================================

namespace
{
    /** Classify an edge type into a vertical direction. */
    enum class ELayoutDirection : uint8
    {
        Above,    // Owners — entities that own the center node
        Below,    // Children — entities owned by or attached to the center node
    };

    auto GetLayoutDirection(ECkGraphEdgeType InType) -> ELayoutDirection
    {
        switch (InType)
        {
        case ECkGraphEdgeType::LifetimeOwner:
        case ECkGraphEdgeType::ContextOwner:
            return ELayoutDirection::Above;

        case ECkGraphEdgeType::SceneNodeChild:
        default:
            return ELayoutDirection::Below;
        }
    }
}

auto FCkDirectionalGraphLayout::ComputeLayout(
    const TArray<FCkGraphNode>& InNodes,
    const TArray<FCkGraphEdge>& InEdges,
    int32 InCenterNodeIndex) -> TArray<FCkGraphNodePosition>
{
    TArray<FCkGraphNodePosition> Positions;
    Positions.Reserve(InNodes.Num());

    if (InNodes.IsEmpty())
    {
        return Positions;
    }

    // Center node at origin
    {
        auto& CenterPos = Positions.AddDefaulted_GetRef();
        CenterPos.NodeIndex = InCenterNodeIndex;
        CenterPos.Position = FVector2D::ZeroVector;
    }

    // Group satellite nodes by direction
    TArray<int32> AboveNodes;
    TArray<int32> BelowNodes;

    for (int32 i = 0; i < InNodes.Num(); ++i)
    {
        if (i == InCenterNodeIndex)
        {
            continue;
        }

        const auto Direction = GetLayoutDirection(InNodes[i].SourceEdgeType);
        if (Direction == ELayoutDirection::Above)
        {
            AboveNodes.Add(i);
        }
        else
        {
            BelowNodes.Add(i);
        }
    }

    // Position above nodes (owners): centered horizontally, above the center
    if (NOT AboveNodes.IsEmpty())
    {
        const auto GroupWidth = (AboveNodes.Num() - 1) * HorizontalSpacing;
        const auto StartX = -GroupWidth * 0.5f;

        for (int32 i = 0; i < AboveNodes.Num(); ++i)
        {
            auto& Pos = Positions.AddDefaulted_GetRef();
            Pos.NodeIndex = AboveNodes[i];
            Pos.Position = FVector2D(
                StartX + i * HorizontalSpacing,
                -VerticalSpacing);
        }
    }

    // Position below nodes (children): centered horizontally, below the center
    if (NOT BelowNodes.IsEmpty())
    {
        const auto GroupWidth = (BelowNodes.Num() - 1) * HorizontalSpacing;
        const auto StartX = -GroupWidth * 0.5f;

        for (int32 i = 0; i < BelowNodes.Num(); ++i)
        {
            auto& Pos = Positions.AddDefaulted_GetRef();
            Pos.NodeIndex = BelowNodes[i];
            Pos.Position = FVector2D(
                StartX + i * HorizontalSpacing,
                VerticalSpacing);
        }
    }

    return Positions;
}

// =====================================================================================================================
// Radial Layout (kept for future use)
// =====================================================================================================================

auto FCkRadialGraphLayout::ComputeLayout(
    const TArray<FCkGraphNode>& InNodes,
    const TArray<FCkGraphEdge>& InEdges,
    int32 InCenterNodeIndex) -> TArray<FCkGraphNodePosition>
{
    TArray<FCkGraphNodePosition> Positions;
    Positions.Reserve(InNodes.Num());

    if (InNodes.IsEmpty())
    {
        return Positions;
    }

    TArray<int32> SatelliteIndices;
    SatelliteIndices.Reserve(InNodes.Num() - 1);

    for (int32 i = 0; i < InNodes.Num(); ++i)
    {
        if (i == InCenterNodeIndex)
        {
            auto& CenterPos = Positions.AddDefaulted_GetRef();
            CenterPos.NodeIndex = i;
            CenterPos.Position = FVector2D::ZeroVector;
        }
        else
        {
            SatelliteIndices.Add(i);
        }
    }

    if (SatelliteIndices.IsEmpty())
    {
        return Positions;
    }

    const auto SatelliteCount = SatelliteIndices.Num();
    const auto AngleStep = 360.0f / static_cast<float>(SatelliteCount);
    constexpr float StartAngleDegrees = -90.0f;

    for (int32 i = 0; i < SatelliteCount; ++i)
    {
        const auto AngleDegrees = StartAngleDegrees + AngleStep * static_cast<float>(i);
        const auto AngleRadians = FMath::DegreesToRadians(AngleDegrees);

        auto& Pos = Positions.AddDefaulted_GetRef();
        Pos.NodeIndex = SatelliteIndices[i];
        Pos.Position = FVector2D(
            FMath::Cos(AngleRadians) * RingRadius,
            FMath::Sin(AngleRadians) * RingRadius);
    }

    return Positions;
}
