#include "CkEcsDebugConnectionPolicy.h"

#include "CkEcsDebugGraph.h"
#include "CkEcsDebugNode_Entity.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkCore/Macros/CkMacros.h"

#include "Styling/AppStyle.h"
#include "Rendering/DrawElements.h"

#include "CkEditorTools/Style/CkStyle.h"
// --------------------------------------------------------------------------------------------------------------------

FCkEcsDebugConnectionPolicy::FCkEcsDebugConnectionPolicy(
    int32 InBackLayerID,
    int32 InFrontLayerID,
    float InZoomFactor,
    const FSlateRect& InClippingRect,
    FSlateWindowElementList& InDrawElements,
    UEdGraph* InGraphObj)
    : FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
    , _Graph(Cast<UCkEcsDebugGraph>(InGraphObj))
{
    ArrowImage = FAppStyle::GetBrush(TEXT("Graph.Arrow"));
    ArrowRadius = FVector2D(8.0, 8.0);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEcsDebugConnectionPolicy::
    DetermineWiringStyle(
        UEdGraphPin* InOutputPin,
        UEdGraphPin* InInputPin,
        FConnectionParams& OutParams)
    -> void
{
    OutParams.AssociatedPin1 = InOutputPin;
    OutParams.AssociatedPin2 = InInputPin;
    OutParams.WireThickness = 1.5f;
    OutParams.WireColor = CkStyle::Graph_Edge();
    OutParams.bDrawBubbles = false;

    if (NOT InInputPin || NOT InOutputPin)
    { return; }

    // Color wire based on the target node's edge type
    if (auto* TargetNode = Cast<UCkEcsDebugNode_Entity>(InInputPin->GetOwningNode()))
    {
        switch (TargetNode->Get_EdgeType())
        {
        case ECkEcsDebugEdgeType::LifetimeOwner:
            OutParams.WireColor = CkStyle::Relationship();
            break;
        case ECkEcsDebugEdgeType::ContextOwner:
            OutParams.WireColor = CkStyle::Reference();
            break;
        case ECkEcsDebugEdgeType::LifetimeDependent:
            OutParams.WireColor = CkStyle::Transform();
            break;
        default:
            break;
        }
    }

    // Also check source node for edges going from center to dependents
    if (auto* SourceNode = Cast<UCkEcsDebugNode_Entity>(InOutputPin->GetOwningNode()))
    {
        if (auto* TargetNode = Cast<UCkEcsDebugNode_Entity>(InInputPin->GetOwningNode()))
        {
            if (TargetNode->Get_EdgeType() == ECkEcsDebugEdgeType::None
                && SourceNode->Get_EdgeType() != ECkEcsDebugEdgeType::None)
            {
                // Edge from non-center to center: use source's edge type color
                switch (SourceNode->Get_EdgeType())
                {
                case ECkEcsDebugEdgeType::LifetimeOwner:
                    OutParams.WireColor = CkStyle::Relationship();
                    break;
                case ECkEcsDebugEdgeType::ContextOwner:
                    OutParams.WireColor = CkStyle::Reference();
                    break;
                case ECkEcsDebugEdgeType::LifetimeDependent:
                    OutParams.WireColor = CkStyle::Transform();
                    break;
                default:
                    break;
                }
            }
        }
    }

    // Deemphasis for unhovered pins
    auto bDeemphasizeUnhoveredPins = HoveredPins.Num() > 0;
    if (bDeemphasizeUnhoveredPins)
    {
        ApplyHoverDeemphasis(InOutputPin, InInputPin, OutParams.WireThickness, OutParams.WireColor);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEcsDebugConnectionPolicy::
    Draw(
        TMap<TSharedRef<SWidget>, FArrangedWidget>& InPinGeometries,
        FArrangedChildren& InArrangedNodes)
    -> void
{
    _NodeWidgetMap.Empty();
    for (auto NodeIndex = 0; NodeIndex < InArrangedNodes.Num(); ++NodeIndex)
    {
        auto& CurWidget = InArrangedNodes[NodeIndex];
        auto ChildNode = StaticCastSharedRef<SGraphNode>(CurWidget.Widget);
        _NodeWidgetMap.Add(ChildNode->GetNodeObj(), NodeIndex);
    }

    FConnectionDrawingPolicy::Draw(InPinGeometries, InArrangedNodes);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEcsDebugConnectionPolicy::
    DetermineLinkGeometry(
        FArrangedChildren& InArrangedNodes,
        TSharedRef<SWidget>& InOutputPinWidget,
        UEdGraphPin* InOutputPin,
        UEdGraphPin* InInputPin,
        FArrangedWidget*& OutStartWidgetGeometry,
        FArrangedWidget*& OutEndWidgetGeometry)
    -> void
{
    // Resolve to node geometry (wires go node-to-node, not pin-to-pin)
    auto* SourceNode = InOutputPin ? InOutputPin->GetOwningNode() : nullptr;
    auto* TargetNode = InInputPin ? InInputPin->GetOwningNode() : nullptr;

    if (SourceNode && TargetNode)
    {
        auto* StartIdx = _NodeWidgetMap.Find(SourceNode);
        auto* EndIdx = _NodeWidgetMap.Find(TargetNode);

        if (StartIdx && EndIdx)
        {
            OutStartWidgetGeometry = &InArrangedNodes[*StartIdx];
            OutEndWidgetGeometry = &InArrangedNodes[*EndIdx];
            return;
        }
    }

    // Fallback: pin-based geometry
    OutStartWidgetGeometry = PinGeometries->Find(InOutputPinWidget);

    if (auto* pTargetWidget = PinToPinWidgetMap.Find(InInputPin))
    {
        auto InputWidget = (*pTargetWidget).ToSharedRef();
        OutEndWidgetGeometry = PinGeometries->Find(InputWidget);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEcsDebugConnectionPolicy::
    DrawSplineWithArrow(
        const FVector2D& InStartPoint,
        const FVector2D& InEndPoint,
        const FConnectionParams& InParams)
    -> void
{
    DrawLineWithArrow(InStartPoint, InEndPoint, InParams);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEcsDebugConnectionPolicy::
    DrawSplineWithArrow(
        const FGeometry& InStartGeom,
        const FGeometry& InEndGeom,
        const FConnectionParams& InParams)
    -> void
{
    auto StartCenter = FGeometryHelper::CenterOf(InStartGeom);
    auto EndCenter = FGeometryHelper::CenterOf(InEndGeom);
    auto SeedPoint = (StartCenter + EndCenter) * 0.5;

    auto StartPoint = FGeometryHelper::FindClosestPointOnGeom(InStartGeom, SeedPoint);
    auto EndPoint = FGeometryHelper::FindClosestPointOnGeom(InEndGeom, SeedPoint);

    DrawSplineWithArrow(StartPoint, EndPoint, InParams);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEcsDebugConnectionPolicy::
    ComputeSplineTangent(
        const FVector2D& InStart,
        const FVector2D& InEnd) const
    -> FVector2D
{
    return (InEnd - InStart).GetSafeNormal();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEcsDebugConnectionPolicy::
    DrawLineWithArrow(
        const FVector2D& InStart,
        const FVector2D& InEnd,
        const FConnectionParams& InParams)
    -> void
{
    constexpr auto LineSeparation = 4.5f;

    auto Delta = InEnd - InStart;
    auto UnitDelta = Delta.GetSafeNormal();
    auto Normal = FVector2D(Delta.Y, -Delta.X).GetSafeNormal();

    auto DirectionBias = Normal * LineSeparation;
    auto LengthBias = ArrowRadius.X * UnitDelta;
    auto StartPoint = InStart + DirectionBias + LengthBias;
    auto EndPoint = InEnd + DirectionBias - LengthBias;

    auto LinePoints = TArray<FVector2D>{ StartPoint, EndPoint };
    FSlateDrawElement::MakeLines(
        DrawElementsList,
        WireLayerID,
        FPaintGeometry(),
        LinePoints,
        ESlateDrawEffect::None,
        InParams.WireColor,
        true,
        InParams.WireThickness);
    DrawArrow(EndPoint, Delta, InParams);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEcsDebugConnectionPolicy::
    DrawArrow(
        const FVector2D& InPos,
        const FVector2D& InDelta,
        const FConnectionParams& InParams)
    -> void
{
    auto ArrowDrawPos = InPos - ArrowRadius;
    auto AngleInRadians = FMath::Atan2(InDelta.Y, InDelta.X);

    FSlateDrawElement::MakeRotatedBox(
        DrawElementsList,
        ArrowLayerID,
        FPaintGeometry(ArrowDrawPos, ArrowImage->ImageSize * ZoomFactor, ZoomFactor),
        ArrowImage,
        ESlateDrawEffect::None,
        AngleInRadians,
        TOptional<FVector2D>(),
        FSlateDrawElement::RelativeToElement,
        InParams.WireColor);
}

// --------------------------------------------------------------------------------------------------------------------
