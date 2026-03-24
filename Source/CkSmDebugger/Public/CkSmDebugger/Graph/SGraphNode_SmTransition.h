#pragma once

#include "SGraphNode.h"
#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Slate visual for UCkSmDebugNode_Transition — small circular badge at the wire midpoint
// using Graph.TransitionNode.ColorSpill and Graph.TransitionNode.Icon brushes.
// Positioned via PerformSecondPassLayout between source and target state nodes.
// --------------------------------------------------------------------------------------------------------------------

class UCkSmDebugNode_Transition;

// --------------------------------------------------------------------------------------------------------------------

class CKSMDEBUGGER_API SGraphNode_SmTransition : public SGraphNode
{
public:
    SLATE_BEGIN_ARGS(SGraphNode_SmTransition) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, UCkSmDebugNode_Transition* InNode) -> void;

    // SGraphNode
    virtual auto UpdateGraphNode() -> void override;
    virtual auto CreatePinWidgets() -> void override;
    virtual auto RequiresSecondPassLayout() const -> bool override { return true; }
    virtual auto PerformSecondPassLayout(
        const TMap<UObject*, TSharedRef<SNode>>& InNodeToWidgetLookup) const -> void override;
    virtual auto MoveTo(
        const FVector2D& InNewPosition,
        FNodeSet& InNodeFilter,
        bool bMarkDirty = true) -> void override;

private:
    auto GetCenterBetweenNodes(
        const TMap<UObject*, TSharedRef<SNode>>& InNodeToWidgetLookup) const -> FVector2D;

private:
    UCkSmDebugNode_Transition* _TransitionNode = nullptr;
};

// --------------------------------------------------------------------------------------------------------------------
