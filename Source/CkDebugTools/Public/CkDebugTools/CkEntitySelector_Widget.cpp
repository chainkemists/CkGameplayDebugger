#include "CkEntitySelector_Widget.h"

#include "CkEcsDebugger/Subsystem/CkEcsDebugger_Subsystem.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include <Widgets/Layout/SBorder.h>
#include <Widgets/Text/STextBlock.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Views/STableRow.h>

// --------------------------------------------------------------------------------------------------------------------

auto SCkTreeEntitySelector::Construct(const FArguments& InArgs) -> void
{
    OnSelectionChanged = InArgs._OnSelectionChanged;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(&FCkDebugToolsStyle::Get().GetWidgetStyle<FTableRowStyle>("CkDebugTools.TableRow.Normal").EvenRowBackgroundBrush)
        .Padding(4.0f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 4)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Entity Hierarchy")))
                .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Bold"))
                .ColorAndOpacity(FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.Primary"))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 4)
            [
                DoCreateSearchBar()
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                DoCreateEntityTree()
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 4, 0, 0)
            [
                DoCreateRefreshButton()
            ]
        ]
    ];

    // Initial refresh
    Request_RefreshEntityTree();
}

auto SCkTreeEntitySelector::DoCreateSearchBar() -> TSharedRef<SWidget>
{
    return SAssignNew(_SearchBox, SSearchBox)
        .HintText(FText::FromString(TEXT("Filter entities...")))
        .OnTextChanged(this, &SCkTreeEntitySelector::DoOnSearchTextChanged);
}

auto SCkTreeEntitySelector::DoCreateEntityTree() -> TSharedRef<SWidget>
{
    return SAssignNew(_EntityTreeView, STreeView<TSharedPtr<FCkEntityTreeItem>>)
        .TreeItemsSource(&_TreeItems)
        .OnGenerateRow(this, &SCkTreeEntitySelector::DoGenerateTreeRow)
        .OnGetChildren(this, &SCkTreeEntitySelector::DoGetTreeChildren)
        .OnSelectionChanged(this, &SCkTreeEntitySelector::DoOnTreeSelectionChanged)
        .OnMouseButtonDoubleClick(this, &SCkTreeEntitySelector::DoOnTreeDoubleClick)
        .SelectionMode(ESelectionMode::Multi)
        .HeaderRow
        (
            SNew(SHeaderRow)
            .Style(&FCkDebugToolsStyle::Get().GetWidgetStyle<FHeaderRowStyle>("CkDebugTools.HeaderRow.Normal"))

            + SHeaderRow::Column(TEXT("Entity"))
            .DefaultLabel(FText::FromString(TEXT("Entity Hierarchy")))
            .FillWidth(1.0f)
        );
}

auto SCkTreeEntitySelector::DoCreateRefreshButton() -> TSharedRef<SWidget>
{
    return SNew(SButton)
        .ButtonStyle(&FCkDebugToolsStyle::Get().GetWidgetStyle<FButtonStyle>("CkDebugTools.Button.Primary"))
        .Text(FText::FromString(TEXT("Refresh Tree")))
        .OnClicked_Lambda([this]() -> FReply
        {
            Request_RefreshEntityTree();
            return FReply::Handled();
        });
}

auto SCkTreeEntitySelector::Request_RefreshEntityTree() -> void
{
    DoRefreshEntityList();
}

auto SCkTreeEntitySelector::DoRefreshEntityList() -> void
{
    // Get all entities from world (this now includes search filtering)
    auto AllEntities = DoGetAllEntitiesFromWorld();

    // Build tree structure
    _TreeItems = DoBuildTreeHierarchy(AllEntities);

    // Refresh tree view
    if (_EntityTreeView.IsValid())
    {
        _EntityTreeView->RequestTreeRefresh();

        // Auto-expand root items (matching ImGui behavior)
        for (const auto& Item : _TreeItems)
        {
            _EntityTreeView->SetItemExpansion(Item, true);
        }
    }
}

auto SCkTreeEntitySelector::DoGetAllEntitiesFromWorld() -> TArray<FCk_Handle>
{
    TSet<FCk_Handle> EntitiesSet;

    const auto CurrentWorld = Get_CurrentWorld();
    if (NOT IsValid(CurrentWorld))
    {
        return {};
    }

    const auto DebuggerSubsystem = Get_DebuggerSubsystem();
    if (NOT IsValid(DebuggerSubsystem))
    {
        return {};
    }

    const auto SelectedWorld = DebuggerSubsystem->Get_SelectedWorld();
    if (ck::Is_NOT_Valid(SelectedWorld))
    {
        return {};
    }

    auto TransientEntity = Get_TransientEntity();

    const auto& AddEntityToListFunc = [&](FCk_Entity InEntity)
    {
        const auto Handle = ck::MakeHandle(InEntity, TransientEntity);

        if (EntitiesSet.Contains(Handle))
        { return; }

        if (Handle == TransientEntity)
        { return; }

        // Apply search filter here
        if (NOT _SearchFilter.IsEmpty())
        {
            const auto SearchLower = _SearchFilter.ToLower();
            bool ShouldInclude = false;

            // Search by entity string representation
            if (Handle.ToString().ToLower().Contains(SearchLower))
            {
                ShouldInclude = true;
            }

            // Search by actor name if it has one
            if (NOT ShouldInclude && UCk_Utils_OwningActor_UE::Has(Handle))
            {
                const auto EntityActor = UCk_Utils_OwningActor_UE::Get_EntityOwningActor(Handle);
                if (IsValid(EntityActor) && EntityActor->GetName().ToLower().Contains(SearchLower))
                {
                    ShouldInclude = true;
                }
            }

            if (NOT ShouldInclude)
            { return; }
        }

        EntitiesSet.Add(Handle);
    };

    // Get all entities using lifetime owner fragment (like ImGui version)
    TransientEntity.View<ck::FFragment_LifetimeOwner, CK_IGNORE_PENDING_KILL>().ForEach(
        [&](FCk_Entity InEntity, const ck::FFragment_LifetimeOwner& InFragment)
        {
            AddEntityToListFunc(InEntity);
        });

    // If we have entities, add all their parents to maintain hierarchy context
    // (This matches the ImGui version's approach)
    for (auto EntitiesTempArray = EntitiesSet.Array();
         const auto& Entity : EntitiesTempArray)
    {
        auto CurrentEntity = Entity;
        while (CurrentEntity.Has<ck::FFragment_LifetimeOwner>())
        {
            CurrentEntity = CurrentEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
            if (CurrentEntity != TransientEntity)
            {
                EntitiesSet.Add(CurrentEntity);
            }
        }
    }

    // Remove transient entity if it got added
    EntitiesSet.Remove(TransientEntity);

    return EntitiesSet.Array();
}

// Remove the DoApplySearchFilter method since filtering is now done in DoGetAllEntitiesFromWorld

auto SCkTreeEntitySelector::DoBuildTreeHierarchy(const TArray<FCk_Handle>& InEntities) -> TArray<TSharedPtr<FCkEntityTreeItem>>
{
    TArray<TSharedPtr<FCkEntityTreeItem>> RootItems;
    TMap<FCk_Handle, TSharedPtr<FCkEntityTreeItem>> EntityToItemMap;

    // Create tree items for all entities
    for (const auto& Entity : InEntities)
    {
        auto TreeItem = MakeShared<FCkEntityTreeItem>(Entity);
        EntityToItemMap.Add(Entity, TreeItem);
    }

    const auto TransientEntity = Get_TransientEntity();

    // Only render root entities (entities without lifetime owners, or whose lifetime owner is TransientEntity)
    // This matches the RenderEntityTree logic from the ImGui version
    for (const auto& Entity : InEntities)
    {
        auto TreeItem = EntityToItemMap[Entity];

        if (NOT Entity.Has<ck::FFragment_LifetimeOwner>() ||
            Entity.Get<ck::FFragment_LifetimeOwner>().Get_Entity() == TransientEntity)
        {
            RootItems.Add(TreeItem);
        }
    }

    // Build parent-child relationships for all items
    for (const auto& Entity : InEntities)
    {
        if (Entity.Has<ck::FFragment_LifetimeOwner>())
        {
            const auto LifetimeOwner = Entity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();

            if (LifetimeOwner != TransientEntity)
            {
                if (auto OwnerItem = EntityToItemMap.Find(LifetimeOwner))
                {
                    auto ChildItem = EntityToItemMap[Entity];
                    (*OwnerItem)->Children.Add(ChildItem);
                }
            }
        }
    }

    // Sort using the same sorting function as ImGui version
    const auto& BaseSortingFunction = [](const TSharedPtr<FCkEntityTreeItem>& InA, const TSharedPtr<FCkEntityTreeItem>& InB) -> bool
    {
        if (NOT InA.IsValid() || NOT InB.IsValid())
            return false;

        // For now, sort alphabetically by entity string (can be enhanced later with other sorting modes)
        return InA->Entity.ToString() < InB->Entity.ToString();
    };

    ck::algo::Sort(RootItems, BaseSortingFunction);

    // Sort children recursively
    TFunction<void(TSharedPtr<FCkEntityTreeItem>)> SortChildren = [&](TSharedPtr<FCkEntityTreeItem> Item)
    {
        if (NOT Item.IsValid())
            return;

        ck::algo::Sort(Item->Children, BaseSortingFunction);

        // Recursively sort grandchildren
        for (const auto& Child : Item->Children)
        {
            SortChildren(Child);
        }
    };

    for (const auto& RootItem : RootItems)
    {
        SortChildren(RootItem);
    }

    return RootItems;
}

auto SCkTreeEntitySelector::DoGenerateTreeRow(TSharedPtr<FCkEntityTreeItem> InItem, const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>
{
    return SNew(SCkEntityTreeRow, OwnerTable)
        .TreeItem(InItem);
}

auto SCkTreeEntitySelector::DoGetTreeChildren(TSharedPtr<FCkEntityTreeItem> InItem, TArray<TSharedPtr<FCkEntityTreeItem>>& OutChildren) -> void
{
    if (InItem.IsValid())
    {
        OutChildren = InItem->Children;
    }
}

auto SCkTreeEntitySelector::DoOnTreeSelectionChanged(TSharedPtr<FCkEntityTreeItem> InItem, ESelectInfo::Type SelectInfo) -> void
{
    // Get all selected items from tree view
    auto SelectedItems = _EntityTreeView->GetSelectedItems();
    TArray<FCk_Handle> SelectedEntities;

    for (const auto& Item : SelectedItems)
    {
        if (Item.IsValid())
        {
            SelectedEntities.Add(Item->Entity);
        }
    }

    // Update debugger subsystem
    const auto DebuggerSubsystem = Get_DebuggerSubsystem();
    if (IsValid(DebuggerSubsystem))
    {
        const auto SelectedWorld = DebuggerSubsystem->Get_SelectedWorld();
        DebuggerSubsystem->Set_SelectionEntities(SelectedEntities, SelectedWorld);
    }

    // Notify callback
    OnSelectionChanged.ExecuteIfBound();
}

auto SCkTreeEntitySelector::DoOnTreeDoubleClick(TSharedPtr<FCkEntityTreeItem> InItem) -> void
{
    if (NOT InItem.IsValid())
    {
        return;
    }

    // Focus on the entity's actor if it has one
    if (UCk_Utils_OwningActor_UE::Has(InItem->Entity))
    {
        const auto EntityActor = UCk_Utils_OwningActor_UE::Get_EntityOwningActor(InItem->Entity);
        if (IsValid(EntityActor))
        {
            // Focus in editor
            GEditor->SelectNone(true, true);
            GEditor->SelectActor(EntityActor, true, true);
        }
    }
}

auto SCkTreeEntitySelector::DoOnSearchTextChanged(const FText& InText) -> void
{
    _SearchFilter = InText.ToString();
    Request_RefreshEntityTree();
}

auto SCkTreeEntitySelector::Get_SelectedEntities() const -> TArray<FCk_Handle>
{
    const auto DebuggerSubsystem = Get_DebuggerSubsystem();
    if (NOT IsValid(DebuggerSubsystem))
    {
        return {};
    }

    return DebuggerSubsystem->Get_SelectionEntities();
}

auto SCkTreeEntitySelector::Get_CurrentWorld() const -> UWorld*
{
    for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
    {
        if (WorldContext.WorldType == EWorldType::PIE || WorldContext.WorldType == EWorldType::Game)
        {
            return WorldContext.World();
        }
    }
    return nullptr;
}

auto SCkTreeEntitySelector::Get_DebuggerSubsystem() const -> UCk_EcsDebugger_Subsystem_UE*
{
    const auto CurrentWorld = Get_CurrentWorld();
    if (NOT IsValid(CurrentWorld))
    {
        return nullptr;
    }

    return CurrentWorld->GetSubsystem<UCk_EcsDebugger_Subsystem_UE>();
}

auto SCkTreeEntitySelector::Get_TransientEntity() const -> FCk_Handle
{
    const auto DebuggerSubsystem = Get_DebuggerSubsystem();
    if (NOT IsValid(DebuggerSubsystem))
    {
        return FCk_Handle{};
    }

    const auto SelectedWorld = DebuggerSubsystem->Get_SelectedWorld();
    if (ck::Is_NOT_Valid(SelectedWorld))
    {
        return FCk_Handle{};
    }

    return UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(SelectedWorld);
}
