#pragma once

#include "EdGraphUtilities.h"
#include "CoreMinimal.h"

// ====================================================================================================================

class CKGOAPDEBUGGER_API FCkGoapDebugGraphFactory : public FGraphPanelNodeFactory
{
public:
	virtual auto CreateNode(UEdGraphNode* InNode) const -> TSharedPtr<SGraphNode> override;
};

// ====================================================================================================================
