#pragma once

#include "CkDebugTools/CkDebugTools_Style.h"
#include "CkEcs/Handle/CkHandle.h"

#include "CkEcsDebugger/Subsystem/CkEcsDebugger_Subsystem.h"

#include <Widgets/SCompoundWidget.h>
#include <Widgets/Views/STreeView.h>
#include <Widgets/Input/SSearchBox.h>

// --------------------------------------------------------------------------------------------------------------------

struct CKDEBUGTOOLS_API FCkEntityTreeItem
{
    FCk_Handle Entity;
    TArray<TSharedPtr<FCkEntityTreeItem>> Children;
    bool IsExpanded = true;

    FCkEntityTreeItem(const FCk_Handle& InEntity) : Entity(InEntity) {}

    // Get display name for this entity
    auto Get_DisplayName() const -> FString
    {
        if (NOT ck::IsValid(Entity))
        {
            return TEXT("Invalid Entity");
        }

        FString DisplayName = Entity.ToString();

        // Add actor name if available
        if (UCk_Utils_OwningActor_UE::Has(Entity))
        {
            const auto EntityActor = UCk_Utils_OwningActor_UE::Get_EntityOwningActor(Entity);
            if (IsValid(EntityActor))
            {
                DisplayName += FString::Printf(TEXT(" - %s"), *EntityActor->GetName());
            }
        }

        return DisplayName;
    }
};

// --------------------------------------------------------------------------------------------------------------------

class SCkEntityTreeRow : public STableRow<TSharedPtr<FCkEntityTreeItem>>
{
public:
    SLATE_BEGIN_ARGS(SCkEntityTreeRow) {}
        SLATE_ARGUMENT(TSharedPtr<FCkEntityTreeItem>, TreeItem)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView) -> void
    {
        _TreeItem = InArgs._TreeItem;

        STableRow<TSharedPtr<FCkEntityTreeItem>>::Construct(
            STableRow<TSharedPtr<FCkEntityTreeItem>>::FArguments()
                .Padding(1.0f)
                .Content()
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]() -> FText
                    {
                        if (_TreeItem.IsValid())
                        {
                            const auto DisplayName = _TreeItem->Get_DisplayName();
                            return FText::FromString(DisplayName);
                        }
                        return FText::FromString(TEXT("Invalid"));
                    })
                    .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Regular"))
                    .ColorAndOpacity_Lambda([this]() -> FLinearColor
                    {
                        if (_TreeItem.IsValid() && ck::IsValid(_TreeItem->Entity))
                        {
                            return FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.Entity");
                        }
                        return FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.Unknown");
                    })
                    .ToolTipText_Lambda([this]() -> FText
                    {
                        if (_TreeItem.IsValid())
                        {
                            return FText::FromString(FString::Printf(TEXT("Entity: %s\nValid: %s\nChildren: %d"),
                                *_TreeItem->Entity.ToString(),
                                ck::IsValid(_TreeItem->Entity) ? TEXT("Yes") : TEXT("No"),
                                _TreeItem->Children.Num()));
                        }
                        return FText::FromString(TEXT("Invalid TreeItem"));
                    })
                ],
            InOwnerTableView
        );
    }

private:
    TSharedPtr<FCkEntityTreeItem> _TreeItem;
};

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGTOOLS_API SCkTreeEntitySelector : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkTreeEntitySelector) {}
        SLATE_EVENT(FSimpleDelegate, OnSelectionChanged)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // Get currently selected entities
    auto Get_SelectedEntities() const -> TArray<FCk_Handle>;

    // Force refresh of the entity tree
    auto Request_RefreshEntityTree() -> void;

    // Override Tick to auto-refresh
    auto Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override;

private:
    // UI Creation
    auto DoCreateSearchBar() -> TSharedRef<SWidget>;
    auto DoCreateEntityTree() -> TSharedRef<SWidget>;
    auto DoCreateRefreshButton() -> TSharedRef<SWidget>;

    // Tree Management
    auto DoRefreshEntityList() -> void;
    auto DoGetAllEntitiesFromWorld() -> TArray<FCk_Handle>;
    auto DoBuildTreeHierarchy(const TArray<FCk_Handle>& InEntities) -> TArray<TSharedPtr<FCkEntityTreeItem>>;

    // Tree View Callbacks
    auto DoGenerateTreeRow(TSharedPtr<FCkEntityTreeItem> InItem, const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>;
    auto DoGetTreeChildren(TSharedPtr<FCkEntityTreeItem> InItem, TArray<TSharedPtr<FCkEntityTreeItem>>& OutChildren) -> void;
    auto DoOnTreeSelectionChanged(TSharedPtr<FCkEntityTreeItem> InItem, ESelectInfo::Type SelectInfo) -> void;
    auto DoOnTreeDoubleClick(TSharedPtr<FCkEntityTreeItem> InItem) -> void;

    // Event Handlers
    auto DoOnSearchTextChanged(const FText& InText) -> void;

    // World context helpers
    auto Get_CurrentWorld() const -> UWorld*;
    auto Get_DebuggerSubsystem() const -> UCk_EcsDebugger_Subsystem_UE*;
    auto Get_TransientEntity() const -> FCk_Handle;

private:
    // UI Components
    TSharedPtr<STreeView<TSharedPtr<FCkEntityTreeItem>>> _EntityTreeView;
    TSharedPtr<SSearchBox> _SearchBox;

    // Data
    TArray<TSharedPtr<FCkEntityTreeItem>> _TreeItems;
    TMap<FCk_Handle, TSharedPtr<FCkEntityTreeItem>> _EntityToItemMap;
    FString _SearchFilter;

    // Events
    FSimpleDelegate OnSelectionChanged;
};
