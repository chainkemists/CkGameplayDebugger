#include "CkEcsDebugGraphFactory.h"

#if WITH_EDITOR

#include "CkEcsDebugGraph.h"
#include "CkEcsDebugNode_Entity.h"
#include "SGraphNode_EcsEntity.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEcsDebugGraphFactory::
    CreateNode(
        UEdGraphNode* InNode) const
    -> TSharedPtr<SGraphNode>
{
    if (auto* EntityNode = Cast<UCkEcsDebugNode_Entity>(InNode))
    {
        auto GraphNodeWidget = SNew(SGraphNode_EcsEntity, EntityNode);

        // Wire double-click to the graph's callback
        if (auto* Graph = Cast<UCkEcsDebugGraph>(InNode->GetGraph()))
        {
            GraphNodeWidget->OnDoubleClicked.BindLambda(
                [Graph](UCkEcsDebugNode_Entity* InClickedNode)
                {
                    if (Graph->OnNodeDoubleClicked)
                    {
                        Graph->OnNodeDoubleClicked(InClickedNode);
                    }
                });
        }

        return GraphNodeWidget;
    }

    return nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

#endif // WITH_EDITOR
