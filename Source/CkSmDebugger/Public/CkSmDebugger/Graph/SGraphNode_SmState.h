#pragma once

#include "SGraphNode.h"
#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Slate visual for UCkSmDebugNode_State — LogicDriverPro-style rounded-rect body with
// Graph.StateNode.Body border, Graph.StateNode.ColorSpill inner content, colored icon,
// and an overlay pin that fills the entire node for edge connection geometry.
// --------------------------------------------------------------------------------------------------------------------

class UCkSmDebugNode_State;

// --------------------------------------------------------------------------------------------------------------------

class CKSMDEBUGGER_API SGraphNode_SmState : public SGraphNode
{
public:
    SLATE_BEGIN_ARGS(SGraphNode_SmState) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, UCkSmDebugNode_State* InNode) -> void;

    // SGraphNode
    virtual auto UpdateGraphNode() -> void override;
    virtual auto CreatePinWidgets() -> void override;
    virtual auto AddPin(const TSharedRef<SGraphPin>& PinToAdd) -> void override;
    virtual auto GetNodeInfoPopups(
        FNodeInfoContext* InContext,
        TArray<FGraphInformationPopupInfo>& OutPopups) const -> void override;

protected:
    auto GetBorderBackgroundColor() const -> FSlateColor;

private:
    UCkSmDebugNode_State* _StateNode = nullptr;
};

// --------------------------------------------------------------------------------------------------------------------
