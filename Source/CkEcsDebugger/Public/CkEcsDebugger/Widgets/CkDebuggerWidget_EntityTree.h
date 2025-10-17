#pragma once

#include "CoreMinimal.h"
#include "CkEcs/Handle/CkHandle.h"

class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_WorldContext;

struct FCkEntityTreeNode
{
    FCk_Handle Entity;
    TArray<TSharedPtr<FCkEntityTreeNode>> Children;
    TWeakPtr<FCkEntityTreeNode> Parent;
    bool bIsVisible = true;
    bool bIsExpanded = false;
    bool bMatchesFilter = false;
    int32 IndentLevel = 0;
};

struct FCkDebuggerWidget_EntityTree
{
public:
    auto Initialize(
        TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel,
        TSharedPtr<FCkDebuggerModel_WorldContext> InWorldModel) -> void;

    auto Draw() -> void;

    auto SetFilterText(const FString& InFilterText) -> void;
    auto RefreshTree() -> void;
    auto ExpandAll() -> void;
    auto CollapseAll() -> void;

    auto Get_VisibleEntityCount() const -> int32;
    auto Get_TotalEntityCount() const -> int32;

private:
    auto BuildTreeFromEntities(const TArray<FCk_Handle>& InEntities) -> void;
    auto ApplyFilter() -> void;
    auto DrawNode(const TSharedPtr<FCkEntityTreeNode>& InNode) -> void;
    auto HandleNodeClick(const TSharedPtr<FCkEntityTreeNode>& InNode, bool bCtrlHeld) -> void;
    auto IsNodeSelected(const FCk_Handle& InEntity) const -> bool;
    auto ShouldHighlightAsLocalPlayer(const FCk_Handle& InEntity) const -> bool;

    auto SetExpanded(const TSharedPtr<FCkEntityTreeNode>& InNode, bool bExpanded, bool bRecursive) -> void;

    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;

    TArray<TSharedPtr<FCkEntityTreeNode>> RootNodes;
    TArray<TSharedPtr<FCkEntityTreeNode>> FlattenedNodes;

    FString FilterText;
    bool bNeedsRebuild = true;
    int32 VisibleCount = 0;
};