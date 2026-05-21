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

private:
    UCkGoapDebugGraph* _Graph = nullptr;
};

// ====================================================================================================================
