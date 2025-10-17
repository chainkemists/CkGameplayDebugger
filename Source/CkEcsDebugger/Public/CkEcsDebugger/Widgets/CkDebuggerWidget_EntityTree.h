#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "CkEcs/Handle/CkHandle.h"

class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_WorldContext;

struct FCkEntityTreeNode
{
    FCk_Handle Entity;
    TArray<TSharedPtr<FCkEntityTreeNode>> Children;
    TWeakPtr<FCkEntityTreeNode> Parent;
    bool IsVisible = true;
    bool IsExpanded = false;
};

class SCkDebuggerWidget_EntityTree : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerWidget_EntityTree) {}
    SLATE_END_ARGS()

    auto Construct(
        const FArguments& InArgs,
        TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel,
        TSharedPtr<FCkDebuggerModel_WorldContext> InWorldModel) -> void;

    auto Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override;

    auto RefreshTree() -> void;
    auto ApplyFilter(const FString& InFilterText) -> void;
    auto ExpandAll() -> void;
    auto CollapseAll() -> void;

private:
    auto BuildEntityTree() -> void;
    auto BuildHierarchy(const TArray<FCk_Handle>& InEntities) -> void;
    auto ApplyFilterToNodes() -> void;
    auto MarkNodeVisibilityRecursive(TSharedPtr<FCkEntityTreeNode> InNode, bool InVisible) -> void;

    auto OnGetChildren(TSharedPtr<FCkEntityTreeNode> InNode, TArray<TSharedPtr<FCkEntityTreeNode>>& OutChildren) -> void;
    auto OnGenerateRow(TSharedPtr<FCkEntityTreeNode> InNode, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;
    auto OnSelectionChanged(TSharedPtr<FCkEntityTreeNode> InNode, ESelectInfo::Type InSelectInfo) -> void;
    auto OnExpansionChanged(TSharedPtr<FCkEntityTreeNode> InNode, bool InIsExpanded) -> void;
    auto OnContextMenuOpening() -> TSharedPtr<SWidget>;

    auto Get_NodeDisplayText(TSharedPtr<FCkEntityTreeNode> InNode) const -> FText;
    auto Get_NodeColorAndOpacity(TSharedPtr<FCkEntityTreeNode> InNode) const -> FSlateColor;
    auto DoesNodeMatchFilter(TSharedPtr<FCkEntityTreeNode> InNode) const -> bool;

    TSharedPtr<STreeView<TSharedPtr<FCkEntityTreeNode>>> TreeView;
    TArray<TSharedPtr<FCkEntityTreeNode>> RootNodes;
    TArray<TSharedPtr<FCkEntityTreeNode>> AllNodes;

    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;

    FString CurrentFilter;
    bool NeedsRefresh = true;
    float TimeSinceLastRefresh = 0.0f;
    static constexpr float RefreshInterval = 0.1f;
};