#include "CkDebuggerWidget_EntityTree.h"

#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

class SCkDebuggerEntityTreeRow : public STableRow<TSharedPtr<FCkEntityTreeNode>>
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerEntityTreeRow) {}
        SLATE_ARGUMENT(TSharedPtr<FCkEntityTreeNode>, Node)
        SLATE_ARGUMENT(TSharedPtr<FCkDebuggerModel_EntitySelection>, SelectionModel)
        SLATE_ARGUMENT(TWeakPtr<SCkDebuggerWidget_EntityTree>, TreeWidget)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable) -> void
    {
        Node = InArgs._Node;
        SelectionModel = InArgs._SelectionModel;
        TreeWidget = InArgs._TreeWidget;

        STableRow<TSharedPtr<FCkEntityTreeNode>>::Construct(
            STableRow<TSharedPtr<FCkEntityTreeNode>>::FArguments()
            .Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
            .Padding(FMargin(FCkDebuggerStyle::Padding_Small))
            .ShowSelection(true)
            .Content()
            [
                SNew(SBox)
                .Padding(FMargin(FCkDebuggerStyle::Padding_Small, 2.0f))
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
                    [
                        SNew(SImage)
                        .Image(FAppStyle::GetBrush("Icons.FilledCircle"))
                        .ColorAndOpacity(this, &SCkDebuggerEntityTreeRow::Get_EntityStatusColor)
                        .DesiredSizeOverride(FVector2D(6.0f, 6.0f))
                    ]

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(this, &SCkDebuggerEntityTreeRow::Get_NodeDisplayText)
                        .ColorAndOpacity(this, &SCkDebuggerEntityTreeRow::Get_NodeTextColor)
                        .HighlightText(this, &SCkDebuggerEntityTreeRow::Get_HighlightText)
                    ]

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(this, &SCkDebuggerEntityTreeRow::Get_EntityIDText)
                        .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Monospace"))
                        .ColorAndOpacity(FCkDebuggerStyle::Color_Entity_ID)
                    ]
                ]
            ],
            InOwnerTable
        );
    }

private:
    auto Get_NodeDisplayText() const -> FText
    {
        if (NOT Node.IsValid())
        { return FText::GetEmpty(); }

        return FText::FromName(UCk_Utils_Handle_UE::Get_DebugName(Node->Entity));
    }

    auto Get_EntityIDText() const -> FText
    {
        if (NOT Node.IsValid())
        { return FText::GetEmpty(); }

        const auto EntityId = Node->Entity.Get_Entity().Get_ID();
        return FText::FromString(ck::Format_UE(TEXT("[{}]"), EntityId));
    }

    auto Get_NodeTextColor() const -> FSlateColor
    {
        if (NOT Node.IsValid())
        { return FCkDebuggerStyle::Color_Text_Primary; }

        if (SelectionModel.IsValid() && SelectionModel->IsSelected(Node->Entity))
        { return FCkDebuggerStyle::Color_Text_Highlight; }

        return FCkDebuggerStyle::Color_Text_Primary;
    }

    auto Get_EntityStatusColor() const -> FSlateColor
    {
        if (NOT Node.IsValid() || ck::Is_NOT_Valid(Node->Entity))
        { return FCkDebuggerStyle::Color_Error; }

        if (SelectionModel.IsValid() && SelectionModel->IsSelected(Node->Entity))
        { return FCkDebuggerStyle::Color_Selection; }

        return FCkDebuggerStyle::Color_Success;
    }

    auto Get_HighlightText() const -> FText
    {
        const auto TreeWidgetPinned = TreeWidget.Pin();
        if (NOT TreeWidgetPinned.IsValid())
        { return FText::GetEmpty(); }

        return TreeWidgetPinned->Get_CurrentFilter();
    }

    TSharedPtr<FCkEntityTreeNode> Node;
    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TWeakPtr<SCkDebuggerWidget_EntityTree> TreeWidget;
};

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
        .TreeItemsSource(&FilteredRootNodes)
        .OnGenerateRow(this, &SCkDebuggerWidget_EntityTree::OnGenerateRow)
        .OnGetChildren(this, &SCkDebuggerWidget_EntityTree::OnGetChildren)
        .OnSelectionChanged(this, &SCkDebuggerWidget_EntityTree::OnSelectionChanged)
        .OnExpansionChanged(this, &SCkDebuggerWidget_EntityTree::OnExpansionChanged)
        .OnContextMenuOpening(this, &SCkDebuggerWidget_EntityTree::OnContextMenuOpening)
        .SelectionMode(ESelectionMode::Multi)
        .ClearSelectionOnClick(false)
        .HighlightParentNodesForSelection(true)
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

    // Store current selection before refresh
    auto PreviouslySelectedEntities = TArray<FCk_Handle>{};
    if (SelectionModel.IsValid())
    {
        PreviouslySelectedEntities = SelectionModel->Get_SelectedEntities();
    }

    if (WorldModel->IsCacheDirty())
    {
        WorldModel->Refresh_EntityCache();
    }

    BuildEntityTree();
    ApplyFilterToNodes();
    UpdateFilteredRootNodes();

    if (TreeView.IsValid())
    {
        TreeView->RequestTreeRefresh();
    }

    // Restore selection for entities that still exist
    RestoreSelection(PreviouslySelectedEntities);
}

auto SCkDebuggerWidget_EntityTree::ApplyFilter(const FString& InFilterText) -> void
{
    CurrentFilter = InFilterText;
    ApplyFilterToNodes();
    UpdateFilteredRootNodes();

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
        // No filter - show everything
        for (const auto& Node : AllNodes)
        {
            if (Node.IsValid())
            {
                Node->IsVisible = true;
            }
        }
        return;
    }

    // First pass: Mark all nodes as invisible
    for (const auto& Node : AllNodes)
    {
        if (Node.IsValid())
        {
            Node->IsVisible = false;
        }
    }

    // Second pass: Find matching nodes and mark them + their ancestors as visible
    for (const auto& Node : AllNodes)
    {
        if (NOT Node.IsValid())
        { continue; }

        const auto Matches = DoesNodeMatchFilter(Node);
        if (Matches)
        {
            MarkNodeVisibilityRecursive(Node, true);
        }
    }

    // Third pass: Auto-expand parents of visible nodes when filtering
    if (TreeView.IsValid())
    {
        for (const auto& Node : AllNodes)
        {
            if (NOT Node.IsValid() || NOT Node->IsVisible)
            { continue; }

            // Check if this node has visible children
            auto HasVisibleChildren = false;
            for (const auto& Child : Node->Children)
            {
                if (Child.IsValid() && Child->IsVisible)
                {
                    HasVisibleChildren = true;
                    break;
                }
            }

            // If node has visible children, expand it
            if (HasVisibleChildren)
            {
                TreeView->SetItemExpansion(Node, true);
            }
        }
    }
}

auto SCkDebuggerWidget_EntityTree::UpdateFilteredRootNodes() -> void
{
    FilteredRootNodes.Empty();

    for (const auto& Node : RootNodes)
    {
        if (Node.IsValid() && Node->IsVisible)
        {
            FilteredRootNodes.Add(Node);
        }
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

auto SCkDebuggerWidget_EntityTree::RestoreSelection(const TArray<FCk_Handle>& InPreviousSelection) -> void
{
    if (NOT SelectionModel.IsValid() || NOT TreeView.IsValid())
    { return; }

    if (InPreviousSelection.IsEmpty())
    { return; }

    // Build a set of all current entities for fast lookup
    auto CurrentEntitySet = TSet<FCk_Handle>{};
    for (const auto& Node : AllNodes)
    {
        if (Node.IsValid())
        {
            CurrentEntitySet.Add(Node->Entity);
        }
    }

    // Find which previously selected entities still exist
    auto EntitiesToReselect = TArray<FCk_Handle>{};
    auto NodesToSelect = TArray<TSharedPtr<FCkEntityTreeNode>>{};

    for (const auto& PreviousEntity : InPreviousSelection)
    {
        if (NOT CurrentEntitySet.Contains(PreviousEntity))
        { continue; }

        EntitiesToReselect.Add(PreviousEntity);

        // Find the node for this entity
        for (const auto& Node : AllNodes)
        {
            if (Node.IsValid() && Node->Entity == PreviousEntity)
            {
                NodesToSelect.Add(Node);
                break;
            }
        }
    }

    if (EntitiesToReselect.IsEmpty())
    { return; }

    // Expand all parent nodes for selected items so they're visible
    for (const auto& Node : NodesToSelect)
    {
        if (NOT Node.IsValid())
        { continue; }

        auto CurrentParent = Node->Parent.Pin();
        while (CurrentParent.IsValid())
        {
            TreeView->SetItemExpansion(CurrentParent, true);
            CurrentParent = CurrentParent->Parent.Pin();
        }
    }

    // Update the selection model
    SelectionModel->Set_SelectedEntities(EntitiesToReselect);

    // Update the tree view selection
    TreeView->ClearSelection();
    for (const auto& Node : NodesToSelect)
    {
        TreeView->SetItemSelection(Node, true);
    }

    // Scroll to the first selected item to make it visible
    if (NodesToSelect.Num() > 0 && NodesToSelect[0].IsValid())
    {
        TreeView->RequestScrollIntoView(NodesToSelect[0]);
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
    return SNew(SCkDebuggerEntityTreeRow, InOwnerTable)
        .Node(InNode)
        .SelectionModel(SelectionModel)
        .TreeWidget(SharedThis(this));
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

auto SCkDebuggerWidget_EntityTree::DoesNodeMatchFilter(TSharedPtr<FCkEntityTreeNode> InNode) const -> bool
{
    if (NOT InNode.IsValid() || CurrentFilter.IsEmpty())
    { return true; }

    const auto& DebugName = UCk_Utils_Handle_UE::Get_DebugName(InNode->Entity);
    return DebugName.ToString().Contains(CurrentFilter, ESearchCase::IgnoreCase);
}