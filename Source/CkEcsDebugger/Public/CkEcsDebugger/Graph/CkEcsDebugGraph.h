#pragma once

#include "CkEcsDebugNode_Entity.h"

#include "CkEcs/Handle/CkHandle.h"

#include "EdGraph/EdGraph.h"
#include "CoreMinimal.h"

#include "CkEcsDebugGraph.generated.h"

// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------

struct FCkEcsDebugLayoutParams
{
    int32 MinNodeWidth = 200;
    int32 NodeGap = 50;               // horizontal gap between adjacent nodes
    int32 SpacingY = 200;
    int32 MaxDependentsPerRow = 6;
    float CharWidthEstimate = 8.0f;    // approximate pixels per character
    int32 NodePaddingX = 40;           // accent strip + internal margins
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS()
class CKECSDEBUGGER_API UCkEcsDebugGraph : public UEdGraph
{
    GENERATED_BODY()

public:
    /** Rebuild graph from a selected entity. Returns true if the topology changed. */
    auto RebuildFromEntity(const FCk_Handle& InEntity) -> bool;

    /** Force a full rebuild on the next call to RebuildFromEntity(). */
    auto ForceRebuild() -> void { _TopologyHash = 0; }

    /** Find a node by its entity handle. */
    auto FindNodeByEntity(const FCk_Handle& InEntity) const -> UCkEcsDebugNode_Entity*;

    // Suppress change notifications during batch population
    auto SetSuppressNotifications(bool bSuppress) -> void { _SuppressNotifications = bSuppress; }
    virtual void NotifyGraphChanged() override
    {
        if (!_SuppressNotifications) { Super::NotifyGraphChanged(); }
    }

    FCkEcsDebugLayoutParams LayoutParams;

    /** Called when a node is double-clicked in the graph view */
    TFunction<void(UCkEcsDebugNode_Entity*)> OnNodeDoubleClicked;

private:
    auto ClearGraph() -> void;
    auto GatherAndBuild(const FCk_Handle& InEntity) -> void;

    auto CreateEntityNode(
        const FCk_Handle& InEntity,
        const FString& InDisplayName,
        const FLinearColor& InAccentColor,
        bool InIsCenterNode,
        ECkEcsDebugEdgeType InEdgeType,
        int32 InPosX,
        int32 InPosY) -> UCkEcsDebugNode_Entity*;

    auto ConnectNodes(
        UCkEcsDebugNode_Entity* InSourceNode,
        UCkEcsDebugNode_Entity* InTargetNode) -> void;

    static auto MakeEntityLabel(const FCk_Handle& InEntity) -> FString;

private:
    UPROPERTY()
    uint32 _TopologyHash = 0;

    bool _SuppressNotifications = false;

    FCk_Handle _LastBuiltEntity;

    TMap<FCk_Handle, UCkEcsDebugNode_Entity*> _EntityToNode;
};

// --------------------------------------------------------------------------------------------------------------------
