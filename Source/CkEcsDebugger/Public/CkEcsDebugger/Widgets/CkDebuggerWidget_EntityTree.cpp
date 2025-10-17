#include "CkDebuggerWidget_EntityTree.h"

#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Input/SButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"

auto SCkDebuggerWidget_EntityTree::Construct(
    const FArguments& InArgs,
    TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel,
    TSharedPtr<FCkDebuggerModel_WorldContext> InWorldModel) -> void
{
    SelectionModel = InSelectionModel;
    WorldModel = InWorldModel;

    ChildSlot
    [
        SAssignNew(TreeView, STreeView<TSharedPtr<FCkEntityTreeNode>>)
        .TreeItemsSource(&RootNodes)
        .OnGenerateRow(this, &SCkDebuggerWidget_EntityTree::OnGenerateRow)
        .OnGetChildren(this, &SCkDebuggerWidget_EntityTree::OnGetChildren)
        .OnSelectionChanged(this, &SCkDebuggerWidget_EntityTree::OnSelectionChanged)
        .OnExpansionChanged(this, &SCkDebuggerWidget_EntityTree::OnExpansionChanged)
        .OnContextMenuOpening(this, &SCkDebuggerWidget_EntityTree::OnContextMenuOpening)
        .SelectionMode(ESelectionMode::Multi)
        .ClearSelectionOnClick(false)
    ];

    RefreshTree();
}

auto SCkDebuggerWidget_EntityTree::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    TimeSinceLastRefresh += InDeltaTime;

    if (NeedsRefresh && TimeSinceLastRefresh >= RefreshInterval)
    {
        RefreshTree();
        NeedsRefresh = false;
        TimeSinceLastRefresh = 0.0f;
    }
}

auto SCkDebuggerWidget_EntityTree::RefreshTree() -> void
{
    if (NOT WorldModel.IsValid())
    { return; }

    if (WorldModel->IsCacheDirty())
    {
        WorldModel->Refresh_EntityCache();
    }

    BuildEntityTree();
    ApplyFilterToNodes();

    if (TreeView.IsValid())
    {
        TreeView->RequestTreeRefresh();
    }
}

auto SCkDebuggerWidget_EntityTree::ApplyFilter(const FString& InFilterText) -> void
{
    CurrentFilter = InFilterText;
    ApplyFilterToNodes();

    if (TreeView.IsValid())
    {
        TreeView->RequestTreeRefresh();
    }
}

auto SCkDebuggerWidget_EntityTree::ExpandAll() -> void
{
    if (NOT TreeView.IsValid())
    { return; }

    for (const auto& Node : AllNodes)
    {
        if (Node.IsValid() && Node->Children.Num() > 0)
        {
            TreeView->SetItemExpansion(Node, true);
        }
    }
}

auto SCkDebuggerWidget_EntityTree::CollapseAll() -> void
{
    if (NOT TreeView.IsValid())
    { return; }

    for (const auto& Node : AllNodes)
    {
        if (Node.IsValid())
        {
            TreeView->SetItemExpansion(Node, false);
        }
    }
}

auto SCkDebuggerWidget_EntityTree::BuildEntityTree() -> void
{
    RootNodes.Empty();
    AllNodes.Empty();

    if (NOT WorldModel.IsValid())
    { return; }

    const auto& CachedEntities = WorldModel->Get_CachedEntities();
    BuildHierarchy(CachedEntities);
}

auto SCkDebuggerWidget_EntityTree::BuildHierarchy(const TArray<FCk_Handle>& InEntities) -> void
{
    auto NodeMap = TMap<FCk_Handle, TSharedPtr<FCkEntityTreeNode>>{};

    for (const auto& Entity : InEntities)
    {
        if (ck::Is_NOT_Valid(Entity))
        { continue; }

        auto Node = MakeShared<FCkEntityTreeNode>();
        Node->Entity = Entity;
        Node->IsVisible = true;

        NodeMap.Add(Entity, Node);
        AllNodes.Add(Node);
    }

    for (const auto& [Entity, Node] : NodeMap)
    {
        if (NOT Entity.Has<ck::FFragment_LifetimeOwner>())
        {
            RootNodes.Add(Node);
            continue;
        }

        const auto& LifetimeOwner = Entity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();

        if (UCk_Utils_EntityLifetime_UE::Get_IsTransientEntity(LifetimeOwner))
        {
            RootNodes.Add(Node);
            continue;
        }

        if (const auto ParentNode = NodeMap.Find(LifetimeOwner))
        {
            (*ParentNode)->Children.Add(Node);
            Node->Parent = *ParentNode;
        }
        else
        {
            RootNodes.Add(Node);
        }
    }
}

auto SCkDebuggerWidget_EntityTree::ApplyFilterToNodes() -> void
{
    if (CurrentFilter.IsEmpty())
    {
        for (const auto& Node : AllNodes)
        {
            if (Node.IsValid())
            {
                Node->IsVisible = true;
            }
        }
        return;
    }

    for (const auto& Node : AllNodes)
    {
        if (NOT Node.IsValid())
        { continue; }

        const auto Matches = DoesNodeMatchFilter(Node);
        MarkNodeVisibilityRecursive(Node, Matches);
    }
}

auto SCkDebuggerWidget_EntityTree::MarkNodeVisibilityRecursive(
    TSharedPtr<FCkEntityTreeNode> InNode,
    bool InVisible) -> void
{
    if (NOT InNode.IsValid())
    { return; }

    InNode->IsVisible = InVisible;

    if (InVisible)
    {
        auto CurrentNode = InNode->Parent.Pin();
        while (CurrentNode.IsValid())
        {
            CurrentNode->IsVisible = true;
            CurrentNode = CurrentNode->Parent.Pin();
        }
    }
}

auto SCkDebuggerWidget_EntityTree::OnGetChildren(
    TSharedPtr<FCkEntityTreeNode> InNode,
    TArray<TSharedPtr<FCkEntityTreeNode>>& OutChildren) -> void
{
    if (NOT InNode.IsValid())
    { return; }

    for (const auto& Child : InNode->Children)
    {
        if (Child.IsValid() && Child->IsVisible)
        {
            OutChildren.Add(Child);
        }
    }
}

auto SCkDebuggerWidget_EntityTree::OnGenerateRow(
    TSharedPtr<FCkEntityTreeNode> InNode,
    const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>
{
    return SNew(STableRow<TSharedPtr<FCkEntityTreeNode>>, InOwnerTable)
        [
            SNew(STextBlock)
            .Text(this, &SCkDebuggerWidget_EntityTree::Get_NodeDisplayText, InNode)
            .ColorAndOpacity(this, &SCkDebuggerWidget_EntityTree::Get_NodeColorAndOpacity, InNode)
        ];
}

auto SCkDebuggerWidget_EntityTree::OnSelectionChanged(
    TSharedPtr<FCkEntityTreeNode> InNode,
    ESelectInfo::Type InSelectInfo) -> void
{
    if (NOT SelectionModel.IsValid() || NOT TreeView.IsValid())
    { return; }

    const auto SelectedItems = TreeView->GetSelectedItems();

    auto SelectedEntities = TArray<FCk_Handle>{};
    SelectedEntities.Reserve(SelectedItems.Num());

    for (const auto& Item : SelectedItems)
    {
        if (Item.IsValid())
        {
            SelectedEntities.Add(Item->Entity);
        }
    }

    SelectionModel->Set_SelectedEntities(SelectedEntities);
}

auto SCkDebuggerWidget_EntityTree::OnExpansionChanged(
    TSharedPtr<FCkEntityTreeNode> InNode,
    bool InIsExpanded) -> void
{
    if (InNode.IsValid())
    {
        InNode->IsExpanded = InIsExpanded;
    }
}

auto SCkDebuggerWidget_EntityTree::OnContextMenuOpening() -> TSharedPtr<SWidget>
{
    if (NOT SelectionModel.IsValid())
    { return nullptr; }

    auto MenuBuilder = FMenuBuilder(true, nullptr);

    MenuBuilder.AddMenuEntry(
        FText::FromString(TEXT("Clear Selection")),
        FText::FromString(TEXT("Clears all selected entities")),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateLambda([this]()
        {
            if (SelectionModel.IsValid())
            {
                SelectionModel->Clear_Selection();
                if (TreeView.IsValid())
                {
                    TreeView->ClearSelection();
                }
            }
        }))
    );

    return MenuBuilder.MakeWidget();
}

auto SCkDebuggerWidget_EntityTree::Get_NodeDisplayText(TSharedPtr<FCkEntityTreeNode> InNode) const -> FText
{
    if (NOT InNode.IsValid())
    { return FText::GetEmpty(); }

    const auto& DebugName = UCk_Utils_Handle_UE::Get_DebugName(InNode->Entity);
    const auto& EntityId = InNode->Entity.Get_Entity().Get_ID();

    return FText::FromString(ck::Format_UE(TEXT("{} [{}]"), DebugName, EntityId));
}

auto SCkDebuggerWidget_EntityTree::Get_NodeColorAndOpacity(TSharedPtr<FCkEntityTreeNode> InNode) const -> FSlateColor
{
    if (NOT InNode.IsValid())
    { return FSlateColor::UseForeground(); }

    if (SelectionModel.IsValid() && SelectionModel->IsSelected(InNode->Entity))
    {
        return FSlateColor(FLinearColor(0.3f, 0.6f, 1.0f));
    }

    return FSlateColor::UseForeground();
}

auto SCkDebuggerWidget_EntityTree::DoesNodeMatchFilter(TSharedPtr<FCkEntityTreeNode> InNode) const -> bool
{
    if (NOT InNode.IsValid() || CurrentFilter.IsEmpty())
    { return true; }

    const auto& DebugName = UCk_Utils_Handle_UE::Get_DebugName(InNode->Entity);
    return DebugName.ToString().Contains(CurrentFilter, ESearchCase::IgnoreCase);
}