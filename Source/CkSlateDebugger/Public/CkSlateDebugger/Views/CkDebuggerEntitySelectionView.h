#pragma once

#include "CkDebuggerViewBase.h"
#include "Widgets/Views/STreeView.h"

struct FCkEntityTreeNode
{
    FCk_Handle Entity;
    FString DisplayName;
    TArray<TSharedPtr<FCkEntityTreeNode>> Children;
    TWeakPtr<FCkEntityTreeNode> Parent;
    bool bIsExpanded = false;
    bool bMatchesFilter = false;

    static TSharedPtr<FCkEntityTreeNode> Create(const FCk_Handle& InEntity)
    {
        auto Node = MakeShareable(new FCkEntityTreeNode);
        Node.Object->Entity = InEntity;
        return Node;
    }
};

class SCkDebuggerEntitySelectionView : public SCkDebuggerViewBase
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerEntitySelectionView) {}
        SLATE_ARGUMENT(TWeakPtr<SCkSlateDebuggerWindow>, DebuggerWindow)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual auto GetViewName() const -> FName override { return TEXT("EntitySelection"); }
    virtual auto GetViewDisplayName() const -> FText override;

    virtual auto UpdateView() -> void override;
    virtual auto RefreshView() -> void override;
    virtual auto OnWorldChanged(UWorld* NewWorld) -> void override;

private:
    auto BuildEntityTree() -> void;
    auto CreateTreeNode(const FCk_Handle& Entity, TSharedPtr<FCkEntityTreeNode> ParentNode = nullptr) -> TSharedPtr<FCkEntityTreeNode>;
    auto ApplyFilter(const FString& FilterText) -> void;
    auto ApplyFilterRecursive(TSharedPtr<FCkEntityTreeNode> Node, const FString& FilterText) -> bool;

    // Tree view delegates
    auto GenerateTreeRow(TSharedPtr<FCkEntityTreeNode> Node, const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>;
    auto GetTreeChildren(TSharedPtr<FCkEntityTreeNode> Parent, TArray<TSharedPtr<FCkEntityTreeNode>>& OutChildren) -> void;
    auto OnTreeSelectionChanged(TSharedPtr<FCkEntityTreeNode> Node, ESelectInfo::Type SelectInfo) -> void;
    auto OnTreeExpansionChanged(TSharedPtr<FCkEntityTreeNode> Node, bool bIsExpanded) -> void;

    auto OnSearchTextChanged(const FText& NewText) -> void;
    auto OnSearchTextCommitted(const FText& NewText, ETextCommit::Type CommitType) -> void;

    auto GetEntityCountText() const -> FText;
    auto GetEntityDisplayName(const FCk_Handle& Entity) const -> FString;
    auto GetEntityTagText(const FCk_Handle& Entity) const -> FString;

private:
    TSharedPtr<STreeView<TSharedPtr<FCkEntityTreeNode>>> EntityTreeView;
    TSharedPtr<SSearchBox> SearchBox;

    TArray<TSharedPtr<FCkEntityTreeNode>> RootNodes;
    TArray<TSharedPtr<FCkEntityTreeNode>> FilteredNodes;
    TMap<FCk_Handle, TSharedPtr<FCkEntityTreeNode>> EntityNodeMap;

    FString CurrentFilter;
    bool bNeedsRebuild = true;
    double LastRebuildTime = 0.0;
    static constexpr double RebuildCooldown = 0.5; // 500ms cooldown
};