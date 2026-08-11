#pragma once

#include "CkEcsDebugger/Graph/CkEcsRuntimeGraphModel.h"
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SCkDebug_NodePill;
enum class ECkDebug_NodePillVariant : uint8;

// Runtime counterpart to SGraphNode_EcsEntity. The containing canvas owns
// selection, hit testing, dragging, zooming, and double-click navigation.
class CKECSDEBUGGER_API SCkEcsEntityGraphCard : public SCompoundWidget
{
  public:
    SLATE_BEGIN_ARGS(SCkEcsEntityGraphCard) : _bSelected(false) {}
    SLATE_ARGUMENT(TSharedPtr<FCkEcsRuntimeGraphNode>, Node)
    SLATE_ARGUMENT(bool, bSelected)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    auto SetNode(TSharedPtr<FCkEcsRuntimeGraphNode> InNode) -> void;
    auto SetSelected(bool bInSelected) -> void;

  private:
    auto RebuildCard() -> void;
    auto GetAccentColor() const -> FLinearColor;
    auto GetBorderColor() const -> FLinearColor;
    auto GetOpacity() const -> float;
    auto GetVariant() const -> ECkDebug_NodePillVariant;

    TSharedPtr<FCkEcsRuntimeGraphNode> _Node;
    TSharedPtr<SCkDebug_NodePill> _NodePill;
    bool _bSelected = false;
};
