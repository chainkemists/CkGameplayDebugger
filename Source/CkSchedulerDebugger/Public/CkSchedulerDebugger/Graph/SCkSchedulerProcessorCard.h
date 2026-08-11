#pragma once

#include "CkSchedulerDebugger/Graph/CkSchedulerRuntimeGraphModel.h"
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------
// Runtime Slate processor card. The canvas owns placement; this widget owns card visuals.

class CKSCHEDULERDEBUGGER_API SCkSchedulerProcessorCard : public SCompoundWidget
{
  public:
    SLATE_BEGIN_ARGS(SCkSchedulerProcessorCard) : _Selected(false) {}
    SLATE_ARGUMENT(TSharedPtr<FCkSchedulerRuntimeGraphNode>, Node)
    SLATE_ARGUMENT(bool, Selected)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto Set_Selected(bool InSelected) -> void;

  private:
    auto Get_TimingText() const -> FText;
    auto Get_TimingColor() const -> FSlateColor;
    auto Get_ExecutionOrderText() const -> FText;
    auto Get_DirtyVisibility() const -> EVisibility;
    auto Get_ParallelVisibility() const -> EVisibility;
    auto Get_DirtyBorderColor() const -> FLinearColor;
    auto Get_GroupAccentColor() const -> FLinearColor;

    TWeakPtr<FCkSchedulerRuntimeGraphNode> _Node;
    TSharedPtr<class SCkDebug_NodePill> _NodePill;
    bool _Selected = false;
};

// --------------------------------------------------------------------------------------------------------------------
