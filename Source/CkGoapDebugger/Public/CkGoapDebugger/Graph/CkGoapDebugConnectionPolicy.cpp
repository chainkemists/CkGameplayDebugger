#include "CkGoapDebugConnectionPolicy.h"

#include "CkGoapDebugGraph.h"
#include "CkGoapDebugNode_Action.h"
#include "CkGoapDebugNode_Goal.h"
#include "CkGoapDebugger/CkGoapDebuggerStyle.h"

#include "CkCore/Macros/CkMacros.h"

#include "Rendering/DrawElements.h"

// ====================================================================================================================

FCkGoapDebugConnectionPolicy::FCkGoapDebugConnectionPolicy(
    int32 InBackLayerID,
    int32 InFrontLayerID,
    float InZoomFactor,
    const FSlateRect& InClippingRect,
    FSlateWindowElementList& InDrawElements,
    UEdGraph* InGraphObj)
    : FCkDebugConnectionPolicyBase(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
    , _Graph(Cast<UCkGoapDebugGraph>(InGraphObj))
{
}

// ====================================================================================================================

namespace
{
    // Helper: an edge is "in plan" if both endpoints are in-plan Action nodes,
    // OR if the consumer is the Goal anchor and the producer is in-plan.
    auto Is_EdgeInPlan_ConnectionPolicy(UEdGraphNode* InSrc, UEdGraphNode* InDst) -> bool
    {
        const auto* SrcAction = Cast<UCkGoapDebugNode_Action>(InSrc);
        if (NOT SrcAction || NOT SrcAction->Get_IsInPlan()) { return false; }

        if (const auto* DstAction = Cast<UCkGoapDebugNode_Action>(InDst))
        { return DstAction->Get_IsInPlan(); }

        if (Cast<UCkGoapDebugNode_Goal>(InDst) != nullptr)
        { return true; }

        return false;
    }

    auto Is_EdgeFailureBlocked_ConnectionPolicy(UEdGraphNode* InSrc, UEdGraphNode* InDst) -> bool
    {
        const auto* SrcAction = Cast<UCkGoapDebugNode_Action>(InSrc);
        const auto* DstAction = Cast<UCkGoapDebugNode_Action>(InDst);
        if (SrcAction && SrcAction->Get_IsFailureBlocked()) { return true; }
        if (DstAction && DstAction->Get_IsFailureBlocked()) { return true; }
        return false;
    }

    // Dash spacing for tree-edge stippling (U11.7-D). Picked to read clearly
    // at default zoom; tuned along with the dependency-edge solid wire.
    constexpr float TreeEdge_DashLength_ConnectionPolicy = 8.0f;
    constexpr float TreeEdge_GapLength_ConnectionPolicy  = 6.0f;
}

// ====================================================================================================================

auto
    FCkGoapDebugConnectionPolicy::
    DetermineWiringStyle(
        UEdGraphPin* InOutputPin,
        UEdGraphPin* InInputPin,
        FConnectionParams& OutParams)
    -> void
{
    OutParams.AssociatedPin1 = InOutputPin;
    OutParams.AssociatedPin2 = InInputPin;
    OutParams.bDrawBubbles   = false;
    OutParams.bUserFlag1     = false;

    // Defaults — muted off-plan edge.
    OutParams.WireColor     = FCkGoapDebuggerStyle::Color_Border_Subtle;
    OutParams.WireColor.A   = 0.40f;
    OutParams.WireThickness = 1.0f;

    if (NOT InOutputPin || NOT InInputPin) { return; }

    // ----------------------------------------------------------------------
    // Tree edges (parent → child) — dashed, neutral purple/composite tint.
    // The graph records (Output, Input) pin pairs in its tree-edge side set;
    // we ask the graph rather than inspect the pin name so this stays cheap
    // (TSet hash lookup) and the pin-name choice is encapsulated in the
    // Action node + graph. In-plan tree edges get the plan-found tint so the
    // user can still trace plan flow visually; bUserFlag1 marks the wire as
    // dashed for the DrawSplineWithArrow override below.
    // ----------------------------------------------------------------------
    if (_Graph && _Graph->Is_TreeEdge(InOutputPin, InInputPin))
    {
        auto* SrcNodeForTree = InOutputPin->GetOwningNode();
        auto* DstNodeForTree = InInputPin->GetOwningNode();
        const auto bInPlan = Is_EdgeInPlan_ConnectionPolicy(SrcNodeForTree, DstNodeForTree);

        OutParams.WireColor = bInPlan
            ? FCkGoapDebuggerStyle::Color_Status_PlanFound
            : FCkGoapDebuggerStyle::Color_Status_Composite;
        OutParams.WireColor.A   = bInPlan ? 1.0f : 0.55f;
        OutParams.WireThickness = bInPlan ? 2.0f : 1.25f;
        OutParams.bUserFlag1    = true;   // dashed render path
        return;
    }

    auto* SrcNode = InOutputPin->GetOwningNode();
    auto* DstNode = InInputPin->GetOwningNode();

    if (Is_EdgeFailureBlocked_ConnectionPolicy(SrcNode, DstNode))
    {
        OutParams.WireColor     = FCkGoapDebuggerStyle::Color_Status_Failed;
        OutParams.WireThickness = 2.0f;
        return;
    }

    if (Is_EdgeInPlan_ConnectionPolicy(SrcNode, DstNode))
    {
        OutParams.WireColor     = FCkGoapDebuggerStyle::Color_Status_PlanFound;
        OutParams.WireThickness = 2.0f;
        return;
    }
}

// ====================================================================================================================

auto
    FCkGoapDebugConnectionPolicy::
    DrawSplineWithArrow(
        const FVector2D& InStartPoint,
        const FVector2D& InEndPoint,
        const FConnectionParams& InParams)
    -> void
{
    // Solid dependency edges go through the base implementation untouched
    // (straight line + arrow). bUserFlag1 marks tree edges, which we render
    // as a sequence of short segments to read as dashed against the solid
    // dependency wires that share the same canvas.
    if (NOT InParams.bUserFlag1)
    {
        FCkDebugConnectionPolicyBase::DrawSplineWithArrow(InStartPoint, InEndPoint, InParams);
        return;
    }

    const auto Delta     = InEndPoint - InStartPoint;
    const auto Length    = Delta.Size();
    if (Length <= KINDA_SMALL_NUMBER)
    { return; }   // degenerate — nothing to draw

    const auto UnitDelta = Delta / Length;

    const auto DashLen = TreeEdge_DashLength_ConnectionPolicy;
    const auto GapLen  = TreeEdge_GapLength_ConnectionPolicy;
    const auto Stride  = DashLen + GapLen;

    auto T = 0.0f;
    while (T < Length)
    {
        const auto SegEnd = FMath::Min(T + DashLen, Length);
        const auto P0     = InStartPoint + UnitDelta * T;
        const auto P1     = InStartPoint + UnitDelta * SegEnd;

        auto LinePoints = TArray<FVector2D>{ P0, P1 };
        FSlateDrawElement::MakeLines(
            DrawElementsList,
            WireLayerID,
            FPaintGeometry(),
            LinePoints,
            ESlateDrawEffect::None,
            InParams.WireColor,
            true,
            InParams.WireThickness);

        T += Stride;
    }

    // Arrow head at the endpoint (matches the base policy's solid-edge feel).
    DrawArrow(InEndPoint, Delta, InParams);
}

// ====================================================================================================================
