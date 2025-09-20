#include "CkDebuggerEntitySelectionView.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"

#include "CkSlateDebugger/CkSlateDebuggerStyle.h"
#include "CkSlateDebugger/CkSlateDebuggerWindow.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SCkDebuggerEntitySelectionView"

void SCkDebuggerEntitySelectionView::Construct(const FArguments& InArgs)
{
    SCkDebuggerViewBase::Construct(
        SCkDebuggerViewBase::FArguments()
        .DebuggerWindow(InArgs._DebuggerWindow)
    );

    ChildSlot
    [
        SNew(SVerticalBox)

        // Header with search and count
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0, 0, 0, 4)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SAssignNew(SearchBox, SSearchBox)
                .HintText(LOCTEXT("SearchHint", "Search entities..."))
                .OnTextChanged(this, &SCkDebuggerEntitySelectionView::OnSearchTextChanged)
                .OnTextCommitted(this, &SCkDebuggerEntitySelectionView::OnSearchTextCommitted)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(8, 0, 0, 0)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(this, &SCkDebuggerEntitySelectionView::GetEntityCountText)
                .TextStyle(&FCkSlateDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Small"))
            ]
        ]

        // Tree view
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
            .Padding(4)
            [
                SAssignNew(EntityTreeView, STreeView<TSharedPtr<FCkEntityTreeNode>>)
                .TreeItemsSource(&FilteredNodes)
                .OnGenerateRow(this, &SCkDebuggerEntitySelectionView::GenerateTreeRow)
                .OnGetChildren(this, &SCkDebuggerEntitySelectionView::GetTreeChildren)
                .OnSelectionChanged(this, &SCkDebuggerEntitySelectionView::OnTreeSelectionChanged)
                .OnExpansionChanged(this, &SCkDebuggerEntitySelectionView::OnTreeExpansionChanged)
                .SelectionMode(ESelectionMode::Multi)
            ]
        ]
    ];
}

auto SCkDebuggerEntitySelectionView::GetViewDisplayName() const -> FText
{
    return LOCTEXT("EntitySelection", "Entity Selection");
}

auto SCkDebuggerEntitySelectionView::UpdateView() -> void
{
    if (NOT bNeedsRebuild)
    { return; }

    const double CurrentTime = FPlatformTime::Seconds();
    if (CurrentTime - LastRebuildTime < RebuildCooldown)
    { return; }

    BuildEntityTree();
    LastRebuildTime = CurrentTime;
    bNeedsRebuild = false;
}

auto SCkDebuggerEntitySelectionView::RefreshView() -> void
{
    bNeedsRebuild = true;
    UpdateView();
}

auto SCkDebuggerEntitySelectionView::OnWorldChanged(UWorld* NewWorld) -> void
{
    SCkDebuggerViewBase::OnWorldChanged(NewWorld);
    bNeedsRebuild = true;
}

auto SCkDebuggerEntitySelectionView::BuildEntityTree() -> void
{
    RootNodes.Empty();
    EntityNodeMap.Empty();

    auto* World = GetSelectedWorld();
    if (NOT ck::IsValid(World))
    {
        FilteredNodes = RootNodes;
        if (EntityTreeView.IsValid())
        {
            EntityTreeView->RequestTreeRefresh();
        }
        return;
    }

    auto* EcsSubsystem = World->GetSubsystem<UCk_EcsWorld_Subsystem_UE>();
    if (NOT ck::IsValid(EcsSubsystem))
    {
        FilteredNodes = RootNodes;
        if (EntityTreeView.IsValid())
        {
            EntityTreeView->RequestTreeRefresh();
        }
        return;
    }

    auto TransientEntity = EcsSubsystem->Get_TransientEntity();
    if (NOT ck::IsValid(TransientEntity))
    {
        FilteredNodes = RootNodes;
        if (EntityTreeView.IsValid())
        {
            EntityTreeView->RequestTreeRefresh();
        }
        return;
    }

    // Build hierarchy
    TMap<FCk_Handle, TArray<FCk_Handle>> ParentToChildren;
    TArray<FCk_Handle> AllEntities;

    // Collect all entities
    TransientEntity.View<ck::FFragment_LifetimeOwner, CK_IGNORE_PENDING_KILL>().ForEach(
        [&](FCk_Entity Entity, const ck::FFragment_LifetimeOwner& LifetimeOwner)
        {
            auto Handle = ck::MakeHandle(Entity, TransientEntity);
            AllEntities.Add(Handle);

            auto Parent = LifetimeOwner.Get_Entity();
            if (ck::IsValid(Parent) && Parent != TransientEntity)
            {
                ParentToChildren.FindOrAdd(Parent).Add(Handle);
            }
            else
            {
                ParentToChildren.FindOrAdd(TransientEntity).Add(Handle);
            }
        }
    );

    // Build tree recursively
    std::function<void(const FCk_Handle&, TSharedPtr<FCkEntityTreeNode>)> BuildNodeRecursive;
    BuildNodeRecursive = [&](const FCk_Handle& Entity, TSharedPtr<FCkEntityTreeNode> ParentNode)
    {
        auto Node = CreateTreeNode(Entity, ParentNode);
        if (NOT Node.IsValid())
        { return; }

        EntityNodeMap.Add(Entity, Node);

        if (ParentNode.IsValid())
        {
            ParentNode->Children.Add(Node);
        }
        else
        {
            RootNodes.Add(Node);
        }

        if (auto* Children = ParentToChildren.Find(Entity))
        {
            for (const auto& Child : *Children)
            {
                BuildNodeRecursive(Child, Node);
            }
        }
    };

    // Start from transient entity
    if (auto* RootChildren = ParentToChildren.Find(TransientEntity))
    {
        for (const auto& Child : *RootChildren)
        {
            BuildNodeRecursive(Child, nullptr);
        }
    }

    // Apply filter if needed
    if (NOT CurrentFilter.IsEmpty())
    {
        ApplyFilter(CurrentFilter);
    }
    else
    {
        FilteredNodes = RootNodes;
    }

    if (EntityTreeView.IsValid())
    {
        EntityTreeView->RequestTreeRefresh();
    }
}

auto SCkDebuggerEntitySelectionView::CreateTreeNode(const FCk_Handle& Entity, TSharedPtr<FCkEntityTreeNode> ParentNode) -> TSharedPtr<FCkEntityTreeNode>
{
    if (NOT ck::IsValid(Entity))
    { return nullptr; }

    auto Node = FCkEntityTreeNode::Create(Entity);
    Node->DisplayName = GetEntityDisplayName(Entity);
    Node->Parent = ParentNode;

    return Node;
}

auto SCkDebuggerEntitySelectionView::ApplyFilter(const FString& FilterText) -> void
{
    FilteredNodes.Empty();

    if (FilterText.IsEmpty())
    {
        FilteredNodes = RootNodes;
        return;
    }

    for (const auto& RootNode : RootNodes)
    {
        if (ApplyFilterRecursive(RootNode, FilterText))
        {
            FilteredNodes.Add(RootNode);
        }
    }
}

auto SCkDebuggerEntitySelectionView::ApplyFilterRecursive(TSharedPtr<FCkEntityTreeNode> Node, const FString& FilterText) -> bool
{
    if (NOT Node.IsValid())
    { return false; }

    bool bMatches = Node->DisplayName.Contains(FilterText, ESearchCase::IgnoreCase);
    Node->bMatchesFilter = bMatches;

    bool bChildMatches = false;
    for (const auto& Child : Node->Children)
    {
        if (ApplyFilterRecursive(Child, FilterText))
        {
            bChildMatches = true;
        }
    }

    if (bChildMatches && NOT bMatches)
    {
        Node->bIsExpanded = true;
        if (EntityTreeView.IsValid())
        {
            EntityTreeView->SetItemExpansion(Node, true);
        }
    }

    return bMatches || bChildMatches;
}

auto SCkDebuggerEntitySelectionView::GenerateTreeRow(TSharedPtr<FCkEntityTreeNode> Node, const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>
{
    return SNew(STableRow<TSharedPtr<FCkEntityTreeNode>>, OwnerTable)
    [
        SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        .Padding(4, 2)
        [
            SNew(STextBlock)
            .Text(FText::FromString(Node->DisplayName))
            .ColorAndOpacity_Lambda([Node, this]()
            {
                if (Node->bMatchesFilter && NOT CurrentFilter.IsEmpty())
                {
                    return FCkSlateDebuggerStyle::Get().GetColor("CkDebugger.Color.Entity");
                }
                return FLinearColor::White;
            })
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(4, 2)
        [
            SNew(STextBlock)
            .Text(FText::FromString(GetEntityTagText(Node->Entity)))
            .TextStyle(&FCkSlateDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Small"))
            .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
        ]
    ];
}

auto SCkDebuggerEntitySelectionView::GetTreeChildren(TSharedPtr<FCkEntityTreeNode> Parent, TArray<TSharedPtr<FCkEntityTreeNode>>& OutChildren) -> void
{
    if (Parent.IsValid())
    {
        OutChildren = Parent->Children;
    }
}

auto SCkDebuggerEntitySelectionView::OnTreeSelectionChanged(TSharedPtr<FCkEntityTreeNode> Node, ESelectInfo::Type SelectInfo) -> void
{
    if (NOT Node.IsValid())
    { return; }

    if (SelectInfo == ESelectInfo::Direct)
    { return; }

    TArray<TSharedPtr<FCkEntityTreeNode>> SelectedNodes;
    EntityTreeView->GetSelectedItems(SelectedNodes);

    TArray<FCk_Handle> SelectedEntities;
    for (const auto& SelectedNode : SelectedNodes)
    {
        if (SelectedNode.IsValid() && ck::IsValid(SelectedNode->Entity))
        {
            SelectedEntities.Add(SelectedNode->Entity);
        }
    }

    if (auto Window = GetDebuggerWindow())
    {
        Window->SetSelectedEntities(SelectedEntities);
    }
}

auto SCkDebuggerEntitySelectionView::OnTreeExpansionChanged(TSharedPtr<FCkEntityTreeNode> Node, bool bIsExpanded) -> void
{
    if (Node.IsValid())
    {
        Node->bIsExpanded = bIsExpanded;
    }
}

auto SCkDebuggerEntitySelectionView::OnSearchTextChanged(const FText& NewText) -> void
{
    CurrentFilter = NewText.ToString();
    ApplyFilter(CurrentFilter);

    if (EntityTreeView.IsValid())
    {
        EntityTreeView->RequestTreeRefresh();
    }
}

auto SCkDebuggerEntitySelectionView::OnSearchTextCommitted(const FText& NewText, ETextCommit::Type CommitType) -> void
{
    OnSearchTextChanged(NewText);
}

auto SCkDebuggerEntitySelectionView::GetEntityCountText() const -> FText
{
    int32 TotalCount = 0;
    int32 FilteredCount = 0;

    std::function<void(const TArray<TSharedPtr<FCkEntityTreeNode>>&)> CountNodes;
    CountNodes = [&](const TArray<TSharedPtr<FCkEntityTreeNode>>& Nodes)
    {
        for (const auto& Node : Nodes)
        {
            if (Node.IsValid())
            {
                TotalCount++;
                if (Node->bMatchesFilter || CurrentFilter.IsEmpty())
                {
                    FilteredCount++;
                }
                CountNodes(Node->Children);
            }
        }
    };

    CountNodes(RootNodes);

    if (NOT CurrentFilter.IsEmpty() && FilteredCount != TotalCount)
    {
        return FText::Format(LOCTEXT("EntityCountFiltered", "{0} / {1} entities"),
            FText::AsNumber(FilteredCount),
            FText::AsNumber(TotalCount));
    }

    return FText::Format(LOCTEXT("EntityCount", "{0} entities"),
        FText::AsNumber(TotalCount));
}

auto SCkDebuggerEntitySelectionView::GetEntityDisplayName(const FCk_Handle& Entity) const -> FString
{
    return UCk_Utils_Handle_UE::Get_DebugName(Entity).ToString();
}

auto SCkDebuggerEntitySelectionView::GetEntityTagText(const FCk_Handle& Entity) const -> FString
{
    TArray<FString> Tags;

    if (UCk_Utils_Net_UE::Get_Replication(Entity) == ECk_Replication::Replicates)
    {
        Tags.Add(TEXT("Replicated"));
    }

    if (UCk_Utils_OwningActor_UE::Has(Entity))
    {
        if (auto* Actor = UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor(Entity))
        {
            Tags.Add(Actor->GetClass()->GetName());
        }
    }

    if (Tags.Num() > 0)
    {
        return FString::Join(Tags, TEXT(", "));
    }

    return FString();
}