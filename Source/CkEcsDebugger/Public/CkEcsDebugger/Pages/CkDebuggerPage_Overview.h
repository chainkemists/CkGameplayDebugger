#pragma once

#include "CkDebuggerPage_Base.h"
#include "CkEcsDebugger/Graph/CkEcsRuntimeGraphModel.h"

class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_InspectorFilter;
class FCkDebuggerModel_WorldContext;
class SCkDebug_GraphCanvas;
class SCkEcsEntityGraphCard;

class FCkDebuggerPage_Overview : public ICkDebuggerPage_Base
{
  public:
    FCkDebuggerPage_Overview();
    ~FCkDebuggerPage_Overview();

    auto Get_PageName() const -> FText override;
    auto Get_PageIcon() const -> const FSlateBrush* override;
    auto Build_Content(const FCkDebuggerPageContext& InContext) -> TSharedRef<SWidget> override;
    auto Tick(float InDeltaTime) -> void override;
    auto IsActive() const -> bool override;
    auto Set_IsActive(bool InIsActive) -> void override;

  private:
    auto OnSelectionChanged(const TArray<FCk_Handle>& InEntities) -> void;
    auto OnWorldChanged(UWorld* InWorld) -> void;
    auto OnInspectorFilterChanged() -> void;
    auto OnCanvasSelectionChanged(const TSet<uint64>& InSelectedNodeIds) -> void;
    auto OnCanvasNodeDoubleClicked(uint64 InNodeId) -> void;
    auto OnCanvasNodeContextMenu(uint64 InNodeId, const FPointerEvent& InMouseEvent) -> void;
    auto RebuildGraph(bool InForceCards = false) -> void;
    auto RebuildCanvasScene(bool InFrameAll) -> void;
    auto Apply_InspectorFilterToGraph() -> void;
    auto ClearGraph() -> void;

    bool IsActivePage = false;

    FCkEcsRuntimeGraphModel _RuntimeGraphModel;
    TSharedPtr<SCkDebug_GraphCanvas> _GraphCanvas;
    TMap<uint64, TSharedPtr<SCkEcsEntityGraphCard>> _EntityCards;
    TMap<uint64, TSharedPtr<FCkEcsRuntimeGraphNode>> _CanvasNodes;
    bool _PendingFrameAll = false;

    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;
    TSharedPtr<FCkDebuggerModel_InspectorFilter> FilterModel;
    FDelegateHandle SelectionChangedHandle;
    FDelegateHandle WorldChangedHandle;
    FDelegateHandle FilterChangedHandle;

    bool _bNavigatingFromGraph = false;
};
