#pragma once

#include "CkDebuggerCommon/Graph/CkDebugConnectionPolicyBase.h"
#include "CoreMinimal.h"

class UCkGoapDebugGraph;

// ====================================================================================================================
// GOAP-specific connection drawing policy.
//
// - In-plan edges (both endpoints in the active chain): green, 2px thick,
//   slight phase pulse so the user sees plan flow.
// - Failure-blocked edges (consumer node flagged as failure): red, 2px.
// - Other edges: muted dim cyan, 1px, reduced opacity — they fade behind
//   the in-plan flow without disappearing.
// ====================================================================================================================

class CKGOAPDEBUGGER_API FCkGoapDebugConnectionPolicy : public FCkDebugConnectionPolicyBase
{
public:
    FCkGoapDebugConnectionPolicy(
        int32 InBackLayerID,
        int32 InFrontLayerID,
        float InZoomFactor,
        const FSlateRect& InClippingRect,
        FSlateWindowElementList& InDrawElements,
        UEdGraph* InGraphObj);

    virtual auto DetermineWiringStyle(
        UEdGraphPin* InOutputPin,
        UEdGraphPin* InInputPin,
        FConnectionParams& OutParams) -> void override;

    // Override to stipple parent → child tree edges (bUserFlag1 set by
    // DetermineWiringStyle) into dashed segments. Dependency edges still
    // go through the base solid-line path.
    virtual auto DrawSplineWithArrow(
        const FVector2D& InStartPoint,
        const FVector2D& InEndPoint,
        const FConnectionParams& InParams) -> void override;

    // Need to bring the geometry-flavoured overload back in scope (the
    // single-parameter override above hides every overload from the base
    // by name in C++ otherwise).
    using FCkDebugConnectionPolicyBase::DrawSplineWithArrow;

private:
    UCkGoapDebugGraph* _Graph = nullptr;
};

// ====================================================================================================================
