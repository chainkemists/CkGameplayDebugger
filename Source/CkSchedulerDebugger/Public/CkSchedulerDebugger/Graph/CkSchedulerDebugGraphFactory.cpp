#include "CkSchedulerDebugger/Graph/CkSchedulerDebugGraphFactory.h"

#if WITH_EDITOR

#include "CkSchedulerDebugger/Graph/CkSchedulerDebugNode_Processor.h"
#include "CkSchedulerDebugger/Graph/SGraphNode_SchedulerProcessor.h"

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSchedulerDebugGraphFactory::
	CreateNode(UEdGraphNode* InNode) const
	-> TSharedPtr<SGraphNode>
{
	auto ProcessorNode = Cast<UCkSchedulerDebugNode_Processor>(InNode);
	if (NOT ProcessorNode)
	{
		return nullptr;
	}

	return SNew(SGraphNode_SchedulerProcessor, ProcessorNode);
}

// --------------------------------------------------------------------------------------------------------------------

#endif // WITH_EDITOR
