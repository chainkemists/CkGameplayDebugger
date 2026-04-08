#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkSchedulerDebugGraphFactory : public FGraphPanelNodeFactory
{
public:
	virtual auto CreateNode(UEdGraphNode* InNode) const -> TSharedPtr<SGraphNode> override;
};

// --------------------------------------------------------------------------------------------------------------------
