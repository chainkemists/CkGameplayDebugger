#include "CkGoapDebugGraphFactory.h"

#include "CkGoapDebugNode_Action.h"
#include "CkGoapDebugNode_Goal.h"
#include "SGraphNode_GoapAction.h"
#include "SGraphNode_GoapGoal.h"

// ====================================================================================================================

auto
	FCkGoapDebugGraphFactory::
	CreateNode(UEdGraphNode* InNode) const
	-> TSharedPtr<SGraphNode>
{
	if (auto* ActionNode = Cast<UCkGoapDebugNode_Action>(InNode))
	{
		return SNew(SGraphNode_GoapAction, ActionNode);
	}

	if (auto* GoalNode = Cast<UCkGoapDebugNode_Goal>(InNode))
	{
		return SNew(SGraphNode_GoapGoal, GoalNode);
	}

	return nullptr;
}

// ====================================================================================================================
