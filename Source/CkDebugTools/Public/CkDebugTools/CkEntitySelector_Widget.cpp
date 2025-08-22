//
//// CkEntitySelector_Widget.cpp
//#include "CkEntitySelector_Widget.h"
//
//#include "CkEcsDebugger/Subsystem/CkEcsDebugger_Subsystem.h"
//#include "CkEcs/Handle/CkHandle_Utils.h"
//#include "CkEcs/OwningActor/CkOwningActor_Utils.h"
//
//#include <Widgets/Layout/SBorder.h>
//#include <Widgets/Layout/SBox.h>
//#include <Widgets/Text/STextBlock.h>
//#include <Widgets/Input/SButton.h>
//#include <Widgets/Input/SCheckBox.h>
//#include <Framework/MultiBox/MultiBoxBuilder.h>
//#include <Widgets/Views/STableRow.h>
//
//// --------------------------------------------------------------------------------------------------------------------
//
//class SCkEntityTreeRow : public SMultiColumnTableRow<TSharedPtr<FCkEntityTreeItem>>
//{
//public:
//    SLATE_BEGIN_ARGS(SCkEntityTreeRow) {}
//        SLATE_ARGUMENT(TSharedPtr<FCkEntityTreeItem>, TreeItem)
//        SLATE_ARGUMENT(TWeakPtr<SCkEntitySelector>, OwnerSelector)
//    SLATE_END_ARGS()
//
//    auto Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView) -> void
//    {
//        _TreeItem = InArgs._TreeItem;
//        _OwnerSelector = InArgs._OwnerSelector;
//
//        SMultiColumnTableRow<TSharedPtr<FCkEntityTreeItem>>::Construct(
//            FSuperRowType::FArguments()
//                .Padding(1.0f),
//            InOwnerTableView
//        );
//    }
//
//    auto GenerateWidgetForColumn(const FName& ColumnName) -> TSharedRef<SWidget> override
//    {
//        if (NOT _TreeItem.IsValid())
//        {
//            return SNew(STextBlock).Text(FText::FromString(TEXT("Invalid")));
//        }
//
//        const auto Entity = _TreeItem->Entity;
//
//        if (ColumnName == TEXT("Entity"))
//        {
//            // Get debug name
//            const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(Entity);
//            const auto EntityString = Entity.Get_Entity().ToString();
//            const auto DisplayText = FString::Printf(TEXT("%s [%s]"), *DebugName.ToString(), *EntityString);
//
//            // Determine text color
//            FLinearColor TextColor = FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.Primary");
//
//            // Check if this is the local player
//            if (UCk_Utils_OwningActor_UE::Has(Entity))
//            {
//                const auto EntityActor = UCk_Utils_OwningActor_UE::Get_EntityOwningActor(Entity);
//                // Add logic to check if this is local player - simplified for now
//                // TextColor = FLinearColor::Yellow; // Local player highlight
//            }
//
//            return SNew(STextBlock)
//                .Text(FText::FromString(DisplayText))
//                .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Regular"))
//                .ColorAndOpacity(TextColor)
//                .ToolTipText(FText::FromString(FString::Printf(TEXT("Entity: %s\nActor: %s"),
//                    *Entity.ToString(),
//                    UCk_Utils_OwningActor_UE::Has(Entity)
//                        ? *UCk_Utils_OwningActor_UE::Get_EntityOwningActor(Entity)->GetName()
//                        : TEXT("None"))));
//        }
//
//        return SNew(STextBlock).Text(FText::FromString(TEXT("Unknown Column")));
//    }
//
//private:
//    TSharedPtr<FCkEntityTreeItem> _TreeItem;
//    TWeakPtr<SCkEntitySelector> _OwnerSelector;
//};
//
//// --------------------------------------------------------------------------------------------------------------------
//
//auto SCkEntitySelector::Construct(const FArguments& InArgs) -> void
//{
//    OnSelectionChanged = InArgs._OnSelectionChanged;
//
//    ChildSlot
//    [
//        SNew(SBorder)
//        .BorderImage(&FCkDebugToolsStyle::Get().GetWidgetStyle<FTableRowStyle>("CkDebugTools.TableRow.Normal").EvenRowBackgroundBrush)
//        .Padding(4.0f)
//        [
//            SNew(SVerticalBox)
//
//            // Toolbar with menus
//            + SVerticalBox::Slot()
//            .AutoHeight()
//            [
//                DoCreateToolbar()
//            ]
//
//            // Search bar
//            + SVerticalBox::Slot()
//            .AutoHeight()
//            .Padding(0, 4, 0, 4)
//            [
//                SAssignNew(_SearchBox, SSearchBox)
//                .HintText(FText::FromString(TEXT("Filter entities...")))
//                .OnTextChanged(this, &SCkEntitySelector::DoOnSearchTextChanged)
//            ]
//
//            // Entity tree
//            + SVerticalBox::Slot()
//            .FillHeight(1.0f)
//            [
//                DoCreateEntityTree()
//            ]
//        ]
//    ];
//
//    // Initial refresh
//    DoRefreshEntityList(true);
//}
//
//auto SCkEntitySelector::DoCreateToolbar() -> TSharedRef<SWidget>
//{
//    return SNew(SHorizontalBox)
//
//        + SHorizontalBox::Slot()
//        .AutoWidth()
//        .Padding(0, 0, 4, 0)
//        [
//            SNew(SComboButton)
//            .ButtonContent()
//            [
//                SNew(STextBlock)
//                .Text(FText::FromString(TEXT("Display")))
//                .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Regular"))
//            ]
//            .MenuContent()
//            [
//                DoCreateDisplayMenu()
//            ]
//        ]
//
//        + SHorizontalBox::Slot()
//        .AutoWidth()
//        .Padding(0, 0, 4, 0)
//        [
//            SNew(SComboButton)
//            .ButtonContent()
//            [
//                SNew(STextBlock)
//                .Text(FText::FromString(TEXT("Sorting")))
//                .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Regular"))
//            ]
//            .MenuContent()
//            [
//                DoCreateSortingMenu()
//            ]
//        ]
//
//        + SHorizontalBox::Slot()
//        .AutoWidth()
//        .Padding(0, 0, 4, 0)
//        [
//            SNew(SComboButton)
//            .ButtonContent()
//            [
//                SNew(STextBlock)
//                .Text(FText::FromString(TEXT("Filtering")))
//                .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Regular"))
//            ]
//            .MenuContent()
//            [
//                DoCreateFilteringMenu()
//            ]
//        ]
//
//        + SHorizontalBox::Slot()
//        .AutoWidth()
//        .Padding(0, 0, 4, 0)
//        [
//            SNew(SComboButton)
//            .ButtonContent()
//            [
//                SNew(STextBlock)
//                .Text(FText::FromString(TEXT("Update")))
//                .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Regular"))
//            ]
//            .MenuContent()
//            [
//                DoCreateUpdateMenu()
//            ]
//        ]
//
//        + SHorizontalBox::Slot()
//        .AutoWidth()
//        [
//            SNew(SComboButton)
//            .ButtonContent()
//            [
//                SNew(STextBlock)
//                .Text(FText::FromString(TEXT("Selection")))
//                .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Regular"))
//            ]
//            .MenuContent()
//            [
//                DoCreateSelectionMenu()
//            ]
//        ]
//
//        + SHorizontalBox::Slot()
//        .FillWidth(1.0f)
//        [
//            SNew(SSpacer)
//        ]
//
//        + SHorizontalBox::Slot()
//        .AutoWidth()
//        [
//            SNew(SButton)
//            .ButtonStyle(&FCkDebugToolsStyle::Get().GetWidgetStyle<FButtonStyle>("CkDebugTools.Button.Primary"))
//            .Text(FText::FromString(TEXT("Refresh")))
//            .OnClicked_Lambda([this]() -> FReply
//            {
//                DoRefreshEntityList(true);
//                return FReply::Handled();
//            })
//        ];
//}
//
//auto SCkEntitySelector::DoCreateEntityTree() -> TSharedRef<SWidget>
//{
//    return SAssignNew(_EntityTreeView, STreeView<TSharedPtr<FCkEntityTreeItem>>)
//        .TreeItemsSource(&_TreeItems)
//        .OnGenerateRow(this, &SCkEntitySelector::DoGenerateTreeRow)
//        .OnGetChildren(this, &SCkEntitySelector::DoGetTreeChildren)
//        .OnSelectionChanged(this, &SCkEntitySelector::DoOnTreeSelectionChanged)
//        .OnMouseButtonDoubleClick(this, &SCkEntitySelector::DoOnTreeDoubleClick)
//        .SelectionMode(ESelectionMode::Multi)
//        .HeaderRow
//        (
//            SNew(SHeaderRow)
//            .Style(&FCkDebugToolsStyle::Get().GetWidgetStyle<FHeaderRowStyle>("CkDebugTools.HeaderRow.Normal"))
//
//            + SHeaderRow::Column(TEXT("Entity"))
//            .DefaultLabel(FText::FromString(TEXT("Entity")))
//            .FillWidth(1.0f)
//        );
//}
//
//auto SCkEntitySelector::DoRefreshEntityList(bool InForceRefresh) -> void
//{
//    if (NOT InForceRefresh && NOT DoShouldUpdateThisFrame())
//    {
//        return;
//    }
//
//    // Get all entities from world
//    auto AllEntities = DoGetAllEntitiesFromWorld();
//
//    // Apply fragment filtering
//    if (_FragmentFilter != ECkDebugger_EntitiesListFragmentFilteringTypes::None)
//    {
//        AllEntities = DoFilterEntities(AllEntities);
//    }
//
//    // Sort entities
//    DoSortEntities(AllEntities);
//
//    // Build tree structure
//    _TreeItems = DoBuildTreeHierarchy(AllEntities);
//
//    // Refresh tree view
//    if (_EntityTreeView.IsValid())
//    {
//        _EntityTreeView->RequestTreeRefresh();
//
//        // Auto-expand root items
//        for (const auto& Item : _TreeItems)
//        {
//            _EntityTreeView->SetItemExpansion(Item, true);
//        }
//    }
//
//    _LastUpdateTime = FPlatformTime::Seconds();
//    _NeedsRefresh = false;
//}
//
//auto SCkEntitySelector::DoGetAllEntitiesFromWorld() -> TArray<FCk_Handle>
//{
//    TArray<FCk_Handle> Entities;
//
//    const auto CurrentWorld = Get_CurrentWorld();
//    if (NOT IsValid(CurrentWorld))
//    {
//        return Entities;
//    }
//
//    const auto DebuggerSubsystem = Get_DebuggerSubsystem();
//    if (NOT IsValid(DebuggerSubsystem))
//    {
//        return Entities;
//    }
//
//    const auto SelectedWorld = DebuggerSubsystem->Get_SelectedWorld();
//    if (ck::Is_NOT_Valid(SelectedWorld))
//    {
//        return Entities;
//    }
//
//    auto TransientEntity = Get_TransientEntity();
//
//    // Use ECS view to get all entities (similar to ImGui version)
//    TransientEntity.View<ck::FFragment_LifetimeOwner, CK_IGNORE_PENDING_KILL>().ForEach(
//        [&](FCk_Entity InEntity, const ck::FFragment_LifetimeOwner& InFragment)
//        {
//            const auto Handle = ck::MakeHandle(InEntity, TransientEntity);
//
//            if (Handle == TransientEntity)
//                return;
//
//            // Apply search filter
//            if (NOT _SearchFilter.IsEmpty())
//            {
//                const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(Handle);
//                if (NOT DebugName.ToString().ToLower().Contains(_SearchFilter.ToLower()))
//                {
//                    // Also check actor name if available
//                    if (UCk_Utils_OwningActor_UE::Has(Handle))
//                    {
//                        const auto EntityActor = UCk_Utils_OwningActor_UE::Get_EntityOwningActor(Handle);
//                        if (NOT IsValid(EntityActor) ||
//                            NOT EntityActor->GetName().ToLower().Contains(_SearchFilter.ToLower()))
//                        {
//                            return;
//                        }
//                    }
//                    else
//                    {
//                        return;
//                    }
//                }
//            }
//
//            Entities.Add(Handle);
//        });
//
//    return Entities;
//}
//
//auto SCkEntitySelector::Get_SelectedEntities() const -> TArray<FCk_Handle>
//{
//    const auto DebuggerSubsystem = Get_DebuggerSubsystem();
//    if (NOT IsValid(DebuggerSubsystem))
//    {
//        return {};
//    }
//
//    return DebuggerSubsystem->Get_SelectionEntities();
//}
//
//// ... Continue with remaining method implementations following the same pattern as the ImGui version
//
//auto SCkEntitySelector::Get_CurrentWorld() const -> UWorld*
//{
//    for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
//    {
//        if (WorldContext.WorldType == EWorldType::PIE || WorldContext.WorldType == EWorldType::Game)
//        {
//            return WorldContext.World();
//        }
//    }
//    return nullptr;
//}
//
//auto SCkEntitySelector::Get_DebuggerSubsystem() const -> UCk_EcsDebugger_Subsystem_UE*
//{
//    const auto CurrentWorld = Get_CurrentWorld();
//    if (NOT IsValid(CurrentWorld))
//    {
//        return nullptr;
//    }
//
//    return CurrentWorld->GetSubsystem<UCk_EcsDebugger_Subsystem_UE>();
//}
//
//auto SCkEntitySelector::Get_TransientEntity() const -> FCk_Handle
//{
//    const auto DebuggerSubsystem = Get_DebuggerSubsystem();
//    if (NOT IsValid(DebuggerSubsystem))
//    {
//        return FCk_Handle{};
//    }
//
//    const auto SelectedWorld = DebuggerSubsystem->Get_SelectedWorld();
//    if (ck::Is_NOT_Valid(SelectedWorld))
//    {
//        return FCk_Handle{};
//    }
//
//    return UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(SelectedWorld);
//}
