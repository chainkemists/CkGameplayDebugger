#pragma once

#include "CkDebuggerPage_Base.h"
#include "CkEcs/Handle/CkHandle.h"

class FCkEcsGraphModel;
class FCkDirectionalGraphLayout;
class SCkDebuggerWidget_GraphView;
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
    auto OnNodeClicked(const FCk_Handle& InEntity) -> void;
    auto RebuildGraph() -> void;

    bool IsActivePage = false;

    TUniquePtr<FCkEcsGraphModel> GraphModel;
    TUniquePtr<FCkDirectionalGraphLayout> LayoutStrategy;
    TSharedPtr<SCkDebuggerWidget_GraphView> GraphView;

    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;
    FDelegateHandle SelectionChangedHandle;
};
