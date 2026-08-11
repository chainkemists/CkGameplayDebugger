#if WITH_EDITOR

#include "CkGoapDebugConnectionPolicy.h"

#include "CkGoapDebugGraph.h"
#include "CkGoapDebugNode_Action.h"
#include "CkGoapDebugNode_Goal.h"
#include "CkGoapDebugger/CkGoapDebuggerStyle.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkGoapDebugger/CkGoapDebugger_Axes.h"

#include "CkEditorTools/Style/CkStyle.h"

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

    // GraphNodeStyle reaches the wires too — a Minimal graph whose nodes lost their fill but kept
    // fat wires reads worse than either extreme. Both are SCALES on the policy's own tuned values
    // (Card == 1.0 == today's graph), and DetermineWiringStyle runs per paint, so they are live
    // without any binding.
    auto Wire_ThicknessScale_ConnectionPolicy() -> float
    {
        return ck_goap_debugger_axes::Get_NodeBorderScale();
    }

    auto Wire_DimScale_ConnectionPolicy() -> float
    {
        return ck_goap_debugger_axes::Get_NodeDimScale();
    }
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

    // Defaults — muted off-plan edge. Alpha bumped 0.40 → 0.65 so the
    // dependency graph stays readable even when no plan is active (e.g., when
    // the goal is already satisfied and Plan[0] is invalid).
    //
    // Border(), not Graph_Edge(): the GOAP tree deliberately draws its off-plan
    // wires at divider weight so the in-plan spine is the only thing that reads
    // at a glance. Graph_Edge is several stops brighter and drowns it out.
    const auto ThicknessScale = Wire_ThicknessScale_ConnectionPolicy();
    const auto DimScale       = Wire_DimScale_ConnectionPolicy();

    OutParams.WireColor     = CkStyle::Border();
    OutParams.WireColor.A   = FMath::Clamp(0.65f * DimScale, 0.05f, 1.0f);
    OutParams.WireThickness = 1.0f * ThicknessScale;

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
            ? CkStyle::Ok()
            : CkStyle::CategoryAge();
        OutParams.WireColor.A   = bInPlan ? 1.0f : FMath::Clamp(0.55f * DimScale, 0.05f, 1.0f);
        OutParams.WireThickness = (bInPlan ? 2.0f : 1.25f) * ThicknessScale;
        OutParams.bUserFlag1    = true;   // dashed render path
        return;
    }

    auto* SrcNode = InOutputPin->GetOwningNode();
    auto* DstNode = InInputPin->GetOwningNode();

    if (Is_EdgeFailureBlocked_ConnectionPolicy(SrcNode, DstNode))
    {
        OutParams.WireColor     = CkStyle::Err();
        OutParams.WireThickness = 2.0f * ThicknessScale;
        return;
    }

    if (Is_EdgeInPlan_ConnectionPolicy(SrcNode, DstNode))
    {
        OutParams.WireColor     = CkStyle::Ok();
        OutParams.WireThickness = 2.0f * ThicknessScale;
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

#endif
