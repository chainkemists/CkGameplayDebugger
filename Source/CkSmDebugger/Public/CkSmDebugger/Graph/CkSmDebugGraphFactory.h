#pragma once

#include "EdGraphUtilities.h"
#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Node factory — maps UCkSmDebugNode_State / UCkSmDebugNode_Transition to their
// SGraphNode counterparts so the graph editor instantiates the correct Slate widgets.
//
// Register in module startup and pass to SGraphEditor via the NodeFactory argument.
// --------------------------------------------------------------------------------------------------------------------

class CKSMDEBUGGER_API FCkSmDebugGraphFactory : public FGraphPanelNodeFactory
{
public:
    virtual auto CreateNode(UEdGraphNode* InNode) const -> TSharedPtr<SGraphNode> override;
};

// --------------------------------------------------------------------------------------------------------------------
