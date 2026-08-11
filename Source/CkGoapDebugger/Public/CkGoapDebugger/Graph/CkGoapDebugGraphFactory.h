#pragma once

#if WITH_EDITOR

#include "EdGraphUtilities.h"
#include "CoreMinimal.h"

// ====================================================================================================================
// Factory — registered once in CkGoapDebugger module startup. Maps
// UCkGoapDebugNode_Action -> SGraphNode_GoapAction and UCkGoapDebugNode_Goal
// -> SGraphNode_GoapGoal so SGraphEditor instantiates the right visuals.
// ====================================================================================================================

class CKGOAPDEBUGGER_API FCkGoapDebugGraphFactory : public FGraphPanelNodeFactory
{
public:
    virtual auto CreateNode(UEdGraphNode* InNode) const -> TSharedPtr<SGraphNode> override;
};

#endif

// ====================================================================================================================
