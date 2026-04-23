#include "CkDebuggerWidget_EntityTree.h"

#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"

#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/String/CkFuzzyMatch_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_InspectorFilter.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
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
                        .ColorAndOpacity(CkDebugStyle::EntityId())
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
        { return CkDebugStyle::Text(); }

        // Inspector-filter dim takes precedence over selection — dimmed entities still
        // read as dimmed even when clicked, so the user knows the row didn't pass the filter.
        // The same dim applies to non-matches in Highlight search mode.
        if (NOT Node->IsFilterMatch || NOT Node->IsSearchMatch)
        { return CkDebugStyle::TextMute(); }

        if (SelectionModel.IsValid() && SelectionModel->IsSelected(Node->Entity))
        { return CkDebugStyle::TextStrong(); }

        return CkDebugStyle::Text();
    }

    auto Get_EntityStatusColor() const -> FSlateColor
    {
        if (NOT Node.IsValid() || ck::Is_NOT_Valid(Node->Entity))
        { return CkDebugStyle::Err(); }

        if (NOT Node->IsFilterMatch || NOT Node->IsSearchMatch)
        { return CkDebugStyle::TextMute(); }

        if (SelectionModel.IsValid() && SelectionModel->IsSelected(Node->Entity))
        { return CkDebugStyle::Selection(); }

        return CkDebugStyle::Ok();
    }

    auto Get_HighlightText() const -> FText
    {
        const auto TreeWidgetPinned = TreeWidget.Pin();
        if (NOT TreeWidgetPinned.IsValid())
        { return FText::GetEmpty(); }

        // Prefer the Highlight bar — it's the user's "find this" query. Fall
        // back to the Filter so substrings still draw when only Filter is set.
        const auto HighlightText = TreeWidgetPinned->Get_CurrentHighlight();
        return HighlightText.IsEmpty() ? TreeWidgetPinned->Get_CurrentFilter() : HighlightText;
    }

    TSharedPtr<FCkEntityTreeNode> Node;
    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TWeakPtr<SCkDebuggerWidget_EntityTree> TreeWidget;
};

SCkDebuggerWidget_EntityTree::~SCkDebuggerWidget_EntityTree()
{
    if (FilterChangedHandle.IsValid() && FilterModel.IsValid())
    {
        FilterModel->OnFilterChanged.Remove(FilterChangedHandle);
        FilterChangedHandle.Reset();
    }
}

auto SCkDebuggerWidget_EntityTree::Construct(
    const FArguments& InArgs,
    TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel,
    TSharedPtr<FCkDebuggerModel_WorldContext> InWorldModel,
    TSharedPtr<FCkDebuggerModel_InspectorFilter> InFilterModel) -> void
{
    SelectionModel = InSelectionModel;
    WorldModel = InWorldModel;
    FilterModel = InFilterModel;

    if (SelectionModel.IsValid())
    {
        SelectionModel->OnSelectionChanged.AddSP(this, &SCkDebuggerWidget_EntityTree::OnExternalSelectionChanged);
    }

    if (FilterModel.IsValid())
    {
        // TWeakPtr capture so a destroyed tree widget cannot be called back into.
        auto WeakSelf = TWeakPtr<SCkDebuggerWidget_EntityTree>(SharedThis(this));
        FilterChangedHandle = FilterModel->OnFilterChanged.AddLambda(
        [WeakSelf]()
        {
            const auto Pinned = WeakSelf.Pin();
            if (NOT Pinned.IsValid())
            { return; }

            Pinned->ApplyInspectorFilter();
            // Exclusions are applied inside ApplyFilterToNodes (re-runs the visibility pass)
            // so toggling an exclusion immediately hides/reveals matching rows.
            Pinned->ApplyFilterToNodes();
            Pinned->UpdateFilteredRootNodes();
            if (Pinned->TreeView.IsValid())
            {
                Pinned->TreeView->RequestTreeRefresh();
            }
        });
    }

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

    if (TimeSinceLastRefresh >= RefreshInterval)
    {
        TimeSinceLastRefresh = 0.0f;

        if (NeedsRefresh || (WorldModel.IsValid() && WorldModel->IsCacheDirty()))
        {
            RefreshTree();
            NeedsRefresh = false;
        }
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
    ApplyInspectorFilter();
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

auto SCkDebuggerWidget_EntityTree::ApplyHighlight(const FString& InHighlightText) -> void
{
    if (CurrentHighlight == InHighlightText)
    { return; }

    CurrentHighlight = InHighlightText;
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
    NodeMap.Empty();

    if (NOT WorldModel.IsValid())
    { return; }

    const auto& CachedEntities = WorldModel->Get_CachedEntities();
    BuildHierarchy(CachedEntities);
}

auto SCkDebuggerWidget_EntityTree::BuildHierarchy(const TArray<FCk_Handle>& InEntities) -> void
{
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
    // Two-pass pipeline driven by the dual search bar:
    //   1. Filter pass — CurrentFilter hides non-matches via IsVisible.
    //   2. Highlight pass — CurrentHighlight (independent input) flags
    //      IsSearchMatch on the visible set so the row's text colour can dim
    //      anything that doesn't match.
    // Either string can be empty; an empty Filter shows everything, an empty
    // Highlight makes every visible row read as a match.

    // ---- Pass 1: visibility from CurrentFilter ----
    if (CurrentFilter.IsEmpty())
    {
        for (const auto& Node : AllNodes)
        {
            if (Node.IsValid()) { Node->IsVisible = true; }
        }
    }
    else
    {
        for (const auto& Node : AllNodes)
        {
            if (Node.IsValid()) { Node->IsVisible = false; }
        }

        for (const auto& Node : AllNodes)
        {
            if (NOT Node.IsValid())
            { continue; }

            const auto& DebugName = UCk_Utils_Handle_UE::Get_DebugName(Node->Entity);
            if (ck::fuzzy::Match(CurrentFilter, DebugName.ToString(), {}).Get_IsMatch())
            {
                MarkNodeVisibilityRecursive(Node, true);
            }
        }
    }

    // ---- Pass 1b: force-hide entities matched by the persistent exclusion list ----
    // Exclusions override the search filter — an entity the user has added to the
    // "always hide" list should never appear even when they type a matching filter.
    if (FilterModel.IsValid() && FilterModel->Get_HasActiveExclusion())
    {
        for (const auto& Node : AllNodes)
        {
            if (NOT Node.IsValid() || NOT Node->IsVisible)
            { continue; }

            if (FilterModel->Test_Entity_IsExcluded(Node->Entity))
            {
                Node->IsVisible = false;
            }
        }
    }

    // ---- Pass 2: IsSearchMatch from CurrentHighlight ----
    if (CurrentHighlight.IsEmpty())
    {
        for (const auto& Node : AllNodes)
        {
            if (Node.IsValid()) { Node->IsSearchMatch = true; }
        }
    }
    else
    {
        for (const auto& Node : AllNodes)
        {
            if (NOT Node.IsValid())
            { continue; }

            const auto& DebugName = UCk_Utils_Handle_UE::Get_DebugName(Node->Entity);
            Node->IsSearchMatch = ck::fuzzy::Match(CurrentHighlight, DebugName.ToString(), {}).Get_IsMatch();
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

auto SCkDebuggerWidget_EntityTree::ApplyInspectorFilter() -> void
{
    // Walk the FULL node list (independent of IsVisible) so the inspector filter and the
    // search filter compose correctly: search hides via IsVisible, inspector filter dims
    // via IsFilterMatch.
    const auto HasFilter = FilterModel.IsValid();
    for (const auto& Node : AllNodes)
    {
        if (NOT Node.IsValid())
        { continue; }

        Node->IsFilterMatch = HasFilter
            ? FilterModel->Test_Entity(Node->Entity)
            : true;
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

    // Find which previously selected entities still exist using O(1) NodeMap lookup
    auto EntitiesToReselect = TArray<FCk_Handle>{};
    auto NodesToSelect = TArray<TSharedPtr<FCkEntityTreeNode>>{};

    for (const auto& PreviousEntity : InPreviousSelection)
    {
        if (const auto FoundNode = NodeMap.Find(PreviousEntity))
        {
            EntitiesToReselect.Add(PreviousEntity);
            NodesToSelect.Add(*FoundNode);
        }
    }

    // If no previous selection was restored, try to auto-select the locally controlled character
    if (EntitiesToReselect.IsEmpty())
    {
        TrySelectLocallyControlledCharacter();
        return;
    }

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

auto SCkDebuggerWidget_EntityTree::TrySelectLocallyControlledCharacter() -> void
{
    if (NOT SelectionModel.IsValid() || NOT TreeView.IsValid())
    { return; }

    // Search for the locally controlled character
    TSharedPtr<FCkEntityTreeNode> LocallyControlledNode = nullptr;

    for (const auto& Node : AllNodes)
    {
        if (NOT Node.IsValid())
        { continue; }

        const auto ControlledResult = UCk_Utils_Net_UE::Get_IsEntityLocallyControlled_ByPlayer(Node->Entity);

        if (ControlledResult == ECk_Utils_Net_IsLocallyControlled_Result::IsLocallyControlled)
        {
            LocallyControlledNode = Node;
            break;
        }
    }

    // If no locally controlled character found, don't select anything
    if (NOT LocallyControlledNode.IsValid())
    { return; }

    // Expand all parent nodes so the character is visible
    auto CurrentParent = LocallyControlledNode->Parent.Pin();
    while (CurrentParent.IsValid())
    {
        TreeView->SetItemExpansion(CurrentParent, true);
        CurrentParent = CurrentParent->Parent.Pin();
    }

    // Select the locally controlled character
    const auto EntitiesToSelect = TArray<FCk_Handle>{ LocallyControlledNode->Entity };
    SelectionModel->Set_SelectedEntities(EntitiesToSelect);

    TreeView->ClearSelection();
    TreeView->SetItemSelection(LocallyControlledNode, true);
    TreeView->RequestScrollIntoView(LocallyControlledNode);
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

    if (IsUpdatingSelection)
    { return; }

    IsUpdatingSelection = true;

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

    IsUpdatingSelection = false;
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

    // Copy entries: prefer the right-clicked row when STableRow auto-selected it,
    // otherwise fall back to all currently-selected rows. Joined with newlines so
    // multi-select copy is useful for pasting into a list.
    auto SelectedNodes = TArray<TSharedPtr<FCkEntityTreeNode>>{};
    if (TreeView.IsValid())
    {
        SelectedNodes = TreeView->GetSelectedItems();
    }

    if (SelectedNodes.Num() > 0)
    {
        auto NameLines = TArray<FString>{};
        auto IdLines = TArray<FString>{};
        auto NameAndIdLines = TArray<FString>{};
        for (const auto& Node : SelectedNodes)
        {
            if (NOT Node.IsValid())
            { continue; }

            const auto NameStr = UCk_Utils_Handle_UE::Get_DebugName(Node->Entity).ToString();
            const auto IdStr = ck::Format_UE(TEXT("{}"), Node->Entity.Get_Entity().Get_ID());
            NameLines.Add(NameStr);
            IdLines.Add(IdStr);
            NameAndIdLines.Add(ck::Format_UE(TEXT("{} [{}]"), NameStr, IdStr));
        }

        ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
            FText::FromString(TEXT("Copy Name")),
            FText::FromString(TEXT("Copy the entity name(s) to the clipboard")),
            FString::Join(NameLines, TEXT("\n")));

        ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
            FText::FromString(TEXT("Copy ID")),
            FText::FromString(TEXT("Copy the entity ID(s) to the clipboard")),
            FString::Join(IdLines, TEXT("\n")));

        ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
            FText::FromString(TEXT("Copy Name + ID")),
            FText::FromString(TEXT("Copy \"Name [ID]\" line(s) to the clipboard")),
            FString::Join(NameAndIdLines, TEXT("\n")));

        MenuBuilder.AddMenuSeparator();
    }

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


auto SCkDebuggerWidget_EntityTree::OnExternalSelectionChanged(const TArray<FCk_Handle>& InNewSelection) -> void
{
    if (IsUpdatingSelection || NOT TreeView.IsValid())
    { return; }

    IsUpdatingSelection = true;

    TreeView->ClearSelection();

    for (const auto& Entity : InNewSelection)
    {
        if (const auto FoundNode = NodeMap.Find(Entity))
        {
            TreeView->SetItemSelection(*FoundNode, true);

            // Expand parents so the selected node is visible
            auto CurrentParent = (*FoundNode)->Parent.Pin();
            while (CurrentParent.IsValid())
            {
                TreeView->SetItemExpansion(CurrentParent, true);
                CurrentParent = CurrentParent->Parent.Pin();
            }
        }
    }

    // Scroll to the first selected item
    if (InNewSelection.Num() > 0)
    {
        if (const auto FoundNode = NodeMap.Find(InNewSelection[0]))
        {
            TreeView->RequestScrollIntoView(*FoundNode);
        }
    }

    IsUpdatingSelection = false;
}