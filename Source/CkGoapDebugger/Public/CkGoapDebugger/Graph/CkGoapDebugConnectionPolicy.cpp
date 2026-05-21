#include "CkGoapDebugConnectionPolicy.h"

#include "CkGoapDebugGraph.h"
#include "CkGoapDebugNode_Action.h"
#include "CkGoapDebugNode_Goal.h"
#include "CkGoapDebugger/CkGoapDebuggerStyle.h"

#include "CkCore/Macros/CkMacros.h"

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
    auto Is_EdgeInPlan(UEdGraphNode* InSrc, UEdGraphNode* InDst) -> bool
    {
        const auto* SrcAction = Cast<UCkGoapDebugNode_Action>(InSrc);
        if (NOT SrcAction || NOT SrcAction->Get_IsInPlan()) { return false; }

        if (const auto* DstAction = Cast<UCkGoapDebugNode_Action>(InDst))
        { return DstAction->Get_IsInPlan(); }

        if (Cast<UCkGoapDebugNode_Goal>(InDst) != nullptr)
        { return true; }

        return false;
    }

    auto Is_EdgeFailureBlocked(UEdGraphNode* InSrc, UEdGraphNode* InDst) -> bool
    {
        const auto* SrcAction = Cast<UCkGoapDebugNode_Action>(InSrc);
        const auto* DstAction = Cast<UCkGoapDebugNode_Action>(InDst);
        if (SrcAction && SrcAction->Get_IsFailureBlocked()) { return true; }
        if (DstAction && DstAction->Get_IsFailureBlocked()) { return true; }
        return false;
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

    // Defaults — muted off-plan edge.
    OutParams.WireColor     = FCkGoapDebuggerStyle::Color_Border_Subtle;
    OutParams.WireColor.A   = 0.40f;
    OutParams.WireThickness = 1.0f;

    if (NOT InOutputPin || NOT InInputPin) { return; }

    auto* SrcNode = InOutputPin->GetOwningNode();
    auto* DstNode = InInputPin->GetOwningNode();

    if (Is_EdgeFailureBlocked(SrcNode, DstNode))
    {
        OutParams.WireColor     = FCkGoapDebuggerStyle::Color_Status_Failed;
        OutParams.WireThickness = 2.0f;
        return;
    }

    if (Is_EdgeInPlan(SrcNode, DstNode))
    {
        OutParams.WireColor     = FCkGoapDebuggerStyle::Color_Status_PlanFound;
        OutParams.WireThickness = 2.0f;
        // bUserFlag1 reserved for a future dashed/animated draw pass — base
        // class currently draws straight lines + arrows, which is good enough
        // for D5. Visual TODO captured in the D5 report.
        OutParams.bUserFlag1 = true;
        return;
    }
}

// ====================================================================================================================
