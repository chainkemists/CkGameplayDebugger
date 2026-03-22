#include "CkEcsGraphLayoutStrategy.h"

#include "CkEcsDebugger/Graph/CkEcsGraphModel.h"

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

    // Collect satellite node indices (everything except center)
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

    // Handle edge case: center-only graph
    if (SatelliteIndices.IsEmpty())
    {
        return Positions;
    }

    // Distribute satellites evenly around the ring
    const auto SatelliteCount = SatelliteIndices.Num();
    const auto AngleStep = 360.0f / static_cast<float>(SatelliteCount);

    // Start from top (-90 degrees) so first satellite is directly above center
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
