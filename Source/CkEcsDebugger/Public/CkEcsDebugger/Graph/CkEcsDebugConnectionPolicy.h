#pragma once

#include "ConnectionDrawingPolicy.h"
#include "CoreMinimal.h"

class UCkEcsDebugGraph;

// --------------------------------------------------------------------------------------------------------------------
// Connection drawing policy for the ECS entity graph.
// Colors wires based on relationship type (lifetime owner, context owner, dependent).
// Draws straight lines with arrows between entity nodes.
// --------------------------------------------------------------------------------------------------------------------

class CKECSDEBUGGER_API FCkEcsDebugConnectionPolicy : public FConnectionDrawingPolicy
{
public:
    FCkEcsDebugConnectionPolicy(
        int32 InBackLayerID,
        int32 InFrontLayerID,
        float InZoomFactor,
        const FSlateRect& InClippingRect,
        FSlateWindowElementList& InDrawElements,
        UEdGraph* InGraphObj);

    // FConnectionDrawingPolicy
    virtual auto DetermineWiringStyle(
        UEdGraphPin* InOutputPin,
        UEdGraphPin* InInputPin,
        FConnectionParams& OutParams) -> void override;

    virtual auto Draw(
        TMap<TSharedRef<SWidget>, FArrangedWidget>& InPinGeometries,
        FArrangedChildren& InArrangedNodes) -> void override;

    virtual auto DetermineLinkGeometry(
        FArrangedChildren& InArrangedNodes,
        TSharedRef<SWidget>& InOutputPinWidget,
        UEdGraphPin* InOutputPin,
        UEdGraphPin* InInputPin,
        FArrangedWidget*& OutStartWidgetGeometry,
        FArrangedWidget*& OutEndWidgetGeometry) -> void override;

    virtual auto DrawSplineWithArrow(
        const FVector2D& InStartPoint,
        const FVector2D& InEndPoint,
        const FConnectionParams& InParams) -> void override;

    virtual auto DrawSplineWithArrow(
        const FGeometry& InStartGeom,
        const FGeometry& InEndGeom,
        const FConnectionParams& InParams) -> void override;

    virtual auto ComputeSplineTangent(
        const FVector2D& InStart,
        const FVector2D& InEnd) const -> FVector2D override;

private:
    auto DrawLineWithArrow(
        const FVector2D& InStart,
        const FVector2D& InEnd,
        const FConnectionParams& InParams) -> void;

    auto DrawArrow(
        const FVector2D& InPos,
        const FVector2D& InDelta,
        const FConnectionParams& InParams) -> void;

private:
    UCkEcsDebugGraph* _Graph = nullptr;
    TMap<UEdGraphNode*, int32> _NodeWidgetMap;
};

// --------------------------------------------------------------------------------------------------------------------
