#pragma once

#if WITH_EDITOR

#include "EdGraphUtilities.h"
#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

// Node factory — maps UCkEcsDebugNode_Entity to SGraphNode_EcsEntity so the graph
// editor instantiates the correct Slate widgets.
//
// Register in module startup via FEdGraphUtilities::RegisterVisualNodeFactory().
// --------------------------------------------------------------------------------------------------------------------

class CKECSDEBUGGER_API FCkEcsDebugGraphFactory : public FGraphPanelNodeFactory
{
public:
    virtual auto CreateNode(UEdGraphNode* InNode) const -> TSharedPtr<SGraphNode> override;
};

// --------------------------------------------------------------------------------------------------------------------

#endif // WITH_EDITOR
