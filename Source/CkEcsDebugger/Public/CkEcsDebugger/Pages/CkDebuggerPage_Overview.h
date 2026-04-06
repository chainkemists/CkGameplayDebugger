#pragma once

#include "CkDebuggerPage_Base.h"
#include "CkEcs/Handle/CkHandle.h"

class UCkEcsDebugGraph;
class UCkEcsDebugNode_Entity;
class SGraphEditor;
class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_WorldContext;

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
    auto OnGraphNodeDoubleClicked(UCkEcsDebugNode_Entity* InNode) -> void;
    auto RebuildGraph() -> void;

    bool IsActivePage = false;

    UCkEcsDebugGraph* _Graph = nullptr;
    TSharedPtr<SGraphEditor> _GraphEditor;

    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;
    FDelegateHandle SelectionChangedHandle;
    FDelegateHandle WorldChangedHandle;

    bool _bNavigatingFromGraph = false;
};
