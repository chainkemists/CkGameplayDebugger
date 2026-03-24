#include "CkSmDebugGraphFactory.h"

#include "CkSmDebugNode_Entry.h"
#include "CkSmDebugNode_State.h"
#include "CkSmDebugNode_Transition.h"
#include "SGraphNode_SmEntry.h"
#include "SGraphNode_SmState.h"
#include "SGraphNode_SmTransition.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugGraphFactory::
    CreateNode(
        UEdGraphNode* InNode) const
    -> TSharedPtr<SGraphNode>
{
    if (auto* StateNode = Cast<UCkSmDebugNode_State>(InNode))
    {
        return SNew(SGraphNode_SmState, StateNode);
    }

    if (auto* TransitionNode = Cast<UCkSmDebugNode_Transition>(InNode))
    {
        return SNew(SGraphNode_SmTransition, TransitionNode);
    }

    if (auto* EntryNode = Cast<UCkSmDebugNode_Entry>(InNode))
    {
        return SNew(SGraphNode_SmEntry, EntryNode);
    }

    return nullptr;
}

// --------------------------------------------------------------------------------------------------------------------
