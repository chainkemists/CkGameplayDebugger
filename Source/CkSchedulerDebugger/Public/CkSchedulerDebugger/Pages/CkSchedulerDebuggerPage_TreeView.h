#pragma once

#include "CkDebuggerCommon/Graph/CkDebugGraphLayout.h"
#include "CkSchedulerDebugger/Graph/CkSchedulerRuntimeGraphModel.h"
#include "CkSchedulerDebugger/Pages/ICkSchedulerDebuggerPage.h"

// --------------------------------------------------------------------------------------------------------------------

class SCkSchedulerDebugger_ProcessorTree;
class SCkSchedulerDebugger_Inspector;
class SCkDebug_GraphCanvas;
class SCkSchedulerProcessorCard;

// --------------------------------------------------------------------------------------------------------------------

class FCkSchedulerDebuggerPage_TreeView : public ICkSchedulerDebuggerPage
{
  public:
    ~FCkSchedulerDebuggerPage_TreeView() override;

    auto Get_PageName() const -> FText override;
    auto Build_Content(TSharedPtr<FCkSchedulerDebugger_ViewModel> InViewModel)
        -> TSharedRef<SWidget> override;
    auto Tick(float InDeltaTime) -> void override;
    auto OnSelectionChanged(int32 InProcessorIndex) -> void override;
    auto OnStyleRevisionChanged() -> void override;

  private:
    auto DoBuildDetailGraph() -> TSharedRef<SWidget>;
    auto DoRebuildDetailGraph(bool InForceCards = false, bool InFrameAll = true) -> void;
    auto DoOnSelectionChanged(int32 InProcessorIndex) -> void;
    auto DoOnCanvasSelectionChanged(const TSet<uint64>& InSelectedNodeIds) -> void;
    auto DoOnCanvasNodeContextMenu(uint64 InNodeId, const FPointerEvent& InMouseEvent) -> void;
    auto DoShowEmptyState() -> void;

  private:
    TSharedPtr<SCkSchedulerDebugger_ProcessorTree> _ProcessorTree;
    TSharedPtr<SCkSchedulerDebugger_Inspector> _Inspector;
    TSharedPtr<FCkSchedulerDebugger_ViewModel> _ViewModel;
    TSharedPtr<SBox> _DetailGraphContainer;
    TSharedPtr<SCkDebug_GraphCanvas> _DetailGraphCanvas;
    FCkSchedulerRuntimeGraphModel _RuntimeGraphModel;
    FCkDebugGraphLayoutParams _LayoutParams;
    TMap<int32, TSharedPtr<SCkSchedulerProcessorCard>> _ProcessorCards;
    bool _PendingFrameAll = false;

    FDelegateHandle _SelectionChangedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
