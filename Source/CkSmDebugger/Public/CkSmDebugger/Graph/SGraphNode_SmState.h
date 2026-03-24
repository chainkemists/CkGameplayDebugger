#pragma once

#include "CkSmDebugger/Data/CkSmDebugger_Types.h"

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
    virtual auto OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const -> int32 override;

protected:
    auto GetBorderBackgroundColor() const -> FSlateColor;

private:
    auto CreateTaskRows() -> TSharedRef<SWidget>;
    auto GetTaskResultBrushColor(ECk_SmTaskResult InResult) const -> FLinearColor;

    UCkSmDebugNode_State* _StateNode = nullptr;
};

// --------------------------------------------------------------------------------------------------------------------
