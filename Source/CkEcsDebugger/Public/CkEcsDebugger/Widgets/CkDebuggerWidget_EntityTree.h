#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "CkEcs/Handle/CkHandle.h"

class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_WorldContext;
class FCkDebuggerModel_InspectorFilter;

struct FCkEntityTreeNode
{
    FCk_Handle Entity;
    TArray<TSharedPtr<FCkEntityTreeNode>> Children;
    TWeakPtr<FCkEntityTreeNode> Parent;
    bool IsVisible = true;
    bool IsExpanded = false;

    /**
     * Inspector-filter dim state. Independent of IsVisible — search-text filtering hides
     * non-matches via IsVisible, while inspector filtering dims non-matches via IsFilterMatch.
     * The two flags compose: a row is rendered iff IsVisible, and rendered dimmed iff !IsFilterMatch.
     */
    bool IsFilterMatch = true;

    /**
     * Highlight-text match state. Driven by the Highlight search input — when false,
     * the row is shown but dimmed so matches stand out within the filtered set.
     * Independent of IsVisible (which is driven by the Filter input).
     */
    bool IsSearchMatch = true;
};

class SCkDebuggerWidget_EntityTree : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerWidget_EntityTree) {}
    SLATE_END_ARGS()

    ~SCkDebuggerWidget_EntityTree();

    auto Construct(
        const FArguments& InArgs,
        TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel,
        TSharedPtr<FCkDebuggerModel_WorldContext> InWorldModel,
        TSharedPtr<FCkDebuggerModel_InspectorFilter> InFilterModel) -> void;

    auto Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override;

    auto RefreshTree() -> void;
    auto ApplyFilter(const FString& InFilterText) -> void;
    auto ApplyHighlight(const FString& InHighlightText) -> void;
    auto ExpandAll() -> void;
    auto CollapseAll() -> void;
    auto Get_CurrentFilter() const -> FText { return FText::FromString(CurrentFilter); }
    auto Get_CurrentFilterString() const -> const FString& { return CurrentFilter; }
    auto Get_CurrentHighlight() const -> FText { return FText::FromString(CurrentHighlight); }

private:
    auto BuildEntityTree() -> void;
    auto BuildHierarchy(const TArray<FCk_Handle>& InEntities) -> void;
    auto ApplyFilterToNodes() -> void;
    auto ApplyInspectorFilter() -> void;
    auto UpdateFilteredRootNodes() -> void;
    auto MarkNodeVisibilityRecursive(TSharedPtr<FCkEntityTreeNode> InNode, bool InVisible) -> void;
    auto RestoreSelection(const TArray<FCk_Handle>& InPreviousSelection) -> void;
    auto TrySelectLocallyControlledCharacter() -> void;

    auto OnGetChildren(TSharedPtr<FCkEntityTreeNode> InNode, TArray<TSharedPtr<FCkEntityTreeNode>>& OutChildren) -> void;
    auto OnGenerateRow(TSharedPtr<FCkEntityTreeNode> InNode, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;
    auto OnSelectionChanged(TSharedPtr<FCkEntityTreeNode> InNode, ESelectInfo::Type InSelectInfo) -> void;
    auto OnExpansionChanged(TSharedPtr<FCkEntityTreeNode> InNode, bool InIsExpanded) -> void;
    auto OnContextMenuOpening() -> TSharedPtr<SWidget>;

    auto OnExternalSelectionChanged(const TArray<FCk_Handle>& InNewSelection) -> void;

    bool IsUpdatingSelection = false;

    TSharedPtr<STreeView<TSharedPtr<FCkEntityTreeNode>>> TreeView;
    TArray<TSharedPtr<FCkEntityTreeNode>> RootNodes;
    TArray<TSharedPtr<FCkEntityTreeNode>> FilteredRootNodes;
    TArray<TSharedPtr<FCkEntityTreeNode>> AllNodes;
    TMap<FCk_Handle, TSharedPtr<FCkEntityTreeNode>> NodeMap;

    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;
    TSharedPtr<FCkDebuggerModel_InspectorFilter> FilterModel;
    FDelegateHandle FilterChangedHandle;

    FString CurrentFilter;
    FString CurrentHighlight;
    bool NeedsRefresh = true;
    float TimeSinceLastRefresh = 0.0f;
    static constexpr float RefreshInterval = 0.1f;
};