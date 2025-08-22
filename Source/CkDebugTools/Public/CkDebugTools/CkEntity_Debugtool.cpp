#include "CkDebugTools/CkEntity_Debugtool.h"

#include "CkEcs/OwningActor/CkOwningActor_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"
#include "CkRelationship/Team/CkTeam_Utils.h"
#include "CkCore/Debug/CkDebugDraw_Utils.h"

#include "CkDebugTools/CkDebugTools_Style.h"

#include "CkEcs/ContextOwner/CkContextOwner_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkEntityDebugInfoRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView) -> void
{
    _InfoItem = InArgs._InfoItem;

    SMultiColumnTableRow<TSharedPtr<FCkEntityDebugInfo>>::Construct(
        FSuperRowType::FArguments()
            .Padding(1.0f),
        InOwnerTableView
    );
}

auto SCkEntityDebugInfoRow::GenerateWidgetForColumn(const FName& ColumnName) -> TSharedRef<SWidget>
{
    if (NOT _InfoItem.IsValid())
    {
        return SNew(STextBlock).Text(FText::FromString(TEXT("Invalid")));
    }

    if (ColumnName == TEXT("Property"))
    {
        if (_InfoItem->Type == FCkEntityDebugInfo::EInfoType::Section)
        {
            return SNew(STextBlock)
                .Text(FText::FromString(_InfoItem->Key))
                .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Bold"))
                .ColorAndOpacity(FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.Accent"));
        }
        else
        {
            return SNew(STextBlock)
                .Text(FText::FromString(_InfoItem->Key))
                .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Regular"))
                .ColorAndOpacity(FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.Secondary"));
        }
    }

    if (ColumnName == TEXT("Value"))
    {
        if (_InfoItem->Type == FCkEntityDebugInfo::EInfoType::Section)
        {
            return SNew(STextBlock); // Empty for section headers
        }
        else
        {
            return SNew(STextBlock)
                .Text(FText::FromString(_InfoItem->Value))
                .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Regular"))
                .ColorAndOpacity(_InfoItem->Color);
        }
    }

    return SNew(STextBlock).Text(FText::FromString(TEXT("Unknown Column")));
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkEntityDebugTool::Construct(const FArguments& InArgs) -> void
{
    _EntityDiscovery = MakeShared<FCkEntityDiscovery>();
    _DataDiscovery = _EntityDiscovery;

    // Call parent construct which will create the entity selector tree
    SCkDebugTool_Base::Construct(SCkDebugTool_Base::FArguments());

    // Debug: Let's verify we're using the tree selector
    UE_LOG(LogTemp, Warning, TEXT("SCkEntityDebugTool::Construct - EntitySelector valid: %s"),
        _EntitySelector.IsValid() ? TEXT("Yes") : TEXT("No"));
}

auto SCkEntityDebugTool::DoCreateContentPanel() -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(&FCkDebugToolsStyle::Get().GetWidgetStyle<FTableRowStyle>("CkDebugTools.TableRow.Normal").EvenRowBackgroundBrush)
        .Padding(CkDebugToolsConstants::PanelPadding)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text_Lambda([this]() -> FText
                {
                    const auto SelectedEntities = Get_SelectedEntities();
                    if (SelectedEntities.Num() == 0)
                    {
                        return FText::FromString(TEXT("Entity Inspector"));
                    }
                    else if (SelectedEntities.Num() == 1)
                    {
                        return FText::FromString(TEXT("Entity Inspector - Single Entity"));
                    }
                    else
                    {
                        return FText::FromString(FString::Printf(TEXT("Entity Inspector - %d Entities"), SelectedEntities.Num()));
                    }
                })
                .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Header"))
                .ColorAndOpacity(FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.Primary"))
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(0, 4, 0, 0)
            [
                DoCreateEntityInfoList()
            ]
        ];
}

auto SCkEntityDebugTool::DoCreateEntityInfoList() -> TSharedRef<SWidget>
{
    return SAssignNew(_EntityInfoTreeView, STreeView<TSharedPtr<FCkEntityDebugInfo>>)
        .TreeItemsSource(&_EntityInfoItems)
        .OnGenerateRow_Lambda([](TSharedPtr<FCkEntityDebugInfo> InItem, const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>
        {
            return SNew(SCkEntityDebugInfoRow, OwnerTable)
                .InfoItem(InItem);
        })
        .OnGetChildren_Lambda([](TSharedPtr<FCkEntityDebugInfo> InItem, TArray<TSharedPtr<FCkEntityDebugInfo>>& OutChildren)
        {
            OutChildren = InItem->Children;
        })
        .HeaderRow
        (
            SNew(SHeaderRow)

            + SHeaderRow::Column(TEXT("Property"))
            .DefaultLabel(FText::FromString(TEXT("Property")))
            .FillWidth(0.4f)

            + SHeaderRow::Column(TEXT("Value"))
            .DefaultLabel(FText::FromString(TEXT("Value")))
            .FillWidth(0.6f)
        );
}

auto SCkEntityDebugTool::DoRefreshToolData() -> void
{
    DoRefreshEntityInfoList();
}

auto SCkEntityDebugTool::DoRefreshEntityInfoList() -> void
{
    _EntityInfoItems.Empty();

    const auto SelectedEntities = Get_SelectedEntities();

    if (SelectedEntities.Num() == 0)
    {
        auto NoSelectionInfo = MakeShareable(new FCkEntityDebugInfo(TEXT("No entities selected"), TEXT("Select an entity to view its details"), Get_NoneColor()));
        _EntityInfoItems.Add(NoSelectionInfo);
    }
    else
    {
        for (int32 Index = 0; Index < SelectedEntities.Num(); ++Index)
        {
            const auto& Entity = SelectedEntities[Index];

            if (SelectedEntities.Num() > 1)
            {
                // Create section header for multi-entity selection
                auto EntitySection = FCkEntityDebugInfo::CreateSection(FString::Printf(TEXT("Entity %d - %s"), Index + 1, *Entity.ToString()));
                _EntityInfoItems.Add(EntitySection);

                // Collect info as children of the section
                const int32 SectionIndex = _EntityInfoItems.Num() - 1;
                DoCollectEntityInfo(Entity);

                // Move collected items to be children of the section
                for (int32 ChildIndex = SectionIndex + 1; ChildIndex < _EntityInfoItems.Num(); ++ChildIndex)
                {
                    _EntityInfoItems[SectionIndex]->Children.Add(_EntityInfoItems[ChildIndex]);
                }

                // Remove the children from the root level
                _EntityInfoItems.RemoveAt(SectionIndex + 1, _EntityInfoItems.Num() - SectionIndex - 1);
            }
            else
            {
                // Single entity - add items directly to root
                DoCollectEntityInfo(Entity);
            }
        }
    }

    if (_EntityInfoTreeView.IsValid())
    {
        _EntityInfoTreeView->RequestTreeRefresh();

        // Expand all sections by default
        for (const auto& Item : _EntityInfoItems)
        {
            if (Item->Type == FCkEntityDebugInfo::EInfoType::Section)
            {
                _EntityInfoTreeView->SetItemExpansion(Item, true);
            }
        }
    }
}

auto SCkEntityDebugTool::DoCollectEntityInfo(const FCk_Handle& InEntity) -> void
{
    if (NOT ck::IsValid(InEntity))
    {
        auto InvalidInfo = MakeShareable(new FCkEntityDebugInfo(TEXT("Entity Status"), TEXT("INVALID"), Get_UnknownColor()));
        _EntityInfoItems.Add(InvalidInfo);
        return;
    }

    DoAddBasicInfo(InEntity);
    DoAddTransformInfo(InEntity);
    DoAddNetworkInfo(InEntity);
    DoAddRelationshipInfo(InEntity);
}

auto SCkEntityDebugTool::DoAddBasicInfo(const FCk_Handle& InEntity) -> void
{
    // Basic Entity Information
    auto BasicSection = FCkEntityDebugInfo::CreateSection(TEXT("Basic Information"));
    _EntityInfoItems.Add(BasicSection);

    auto HandleInfo = MakeShareable(new FCkEntityDebugInfo(TEXT("Handle"), InEntity.ToString(), Get_EntityColor()));
    BasicSection->Children.Add(HandleInfo);

    auto EntityInfo = MakeShareable(new FCkEntityDebugInfo(TEXT("Entity ID"), InEntity.Get_Entity().ToString(), Get_NumberColor()));
    BasicSection->Children.Add(EntityInfo);

    // Actor Information
    if (UCk_Utils_OwningActor_UE::Has(InEntity))
    {
        const auto EntityActor = UCk_Utils_OwningActor_UE::Get_EntityOwningActor(InEntity);
        auto ActorInfo = MakeShareable(new FCkEntityDebugInfo(TEXT("Actor"), IsValid(EntityActor) ? EntityActor->GetName() : TEXT("None"), Get_EntityColor()));
        BasicSection->Children.Add(ActorInfo);
    }
    else
    {
        auto NoActorInfo = MakeShareable(new FCkEntityDebugInfo(TEXT("Actor"), TEXT("None"), Get_NoneColor()));
        BasicSection->Children.Add(NoActorInfo);
    }

    // Debug Name
    const auto DebugName = InEntity.Get_DebugName();
    auto DebugNameInfo = MakeShareable(new FCkEntityDebugInfo(TEXT("Debug Name"), DebugName.IsNone() ? TEXT("None") : DebugName.ToString(), Get_EntityColor()));
    BasicSection->Children.Add(DebugNameInfo);
}

auto SCkEntityDebugTool::DoAddTransformInfo(const FCk_Handle& InEntity) -> void
{
    if (NOT UCk_Utils_Transform_UE::Has(InEntity))
    {
        return;
    }

    // Transform Information
    auto TransformSection = FCkEntityDebugInfo::CreateSection(TEXT("Transform"));
    _EntityInfoItems.Add(TransformSection);

    const auto Transform = UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(InEntity);

    auto LocationInfo = MakeShareable(new FCkEntityDebugInfo(TEXT("Location"), Transform.GetLocation().ToString(), Get_VectorColor()));
    TransformSection->Children.Add(LocationInfo);

    auto RotationInfo = MakeShareable(new FCkEntityDebugInfo(TEXT("Rotation"), Transform.GetRotation().Rotator().ToString(), Get_VectorColor()));
    TransformSection->Children.Add(RotationInfo);

    auto ScaleInfo = MakeShareable(new FCkEntityDebugInfo(TEXT("Scale"), Transform.GetScale3D().ToString(), Get_VectorColor()));
    TransformSection->Children.Add(ScaleInfo);

    // Draw debug visualization
    UCk_Utils_DebugDraw_UE::DrawDebugTransformGizmo(UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InEntity), Transform);

    const auto EntityWorld = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InEntity);
    const auto TextLocation = Transform.GetLocation() + FVector(0.0f, 0.0f, 50.0f);

    UCk_Utils_DebugDraw_UE::DrawDebugString(
        EntityWorld,
        TextLocation,
        InEntity.ToString(),
        nullptr,
        FLinearColor::White,
        0.0f);
}

auto SCkEntityDebugTool::DoAddNetworkInfo(const FCk_Handle& InEntity) -> void
{
    // Network Information
    auto NetworkSection = FCkEntityDebugInfo::CreateSection(TEXT("Network"));
    _EntityInfoItems.Add(NetworkSection);

    const auto NetMode = UCk_Utils_Net_UE::Get_EntityNetMode(InEntity);
    auto NetModeInfo = MakeShareable(new FCkEntityDebugInfo(TEXT("Net Mode"), ck::Format_UE(TEXT("{}"), NetMode), Get_EnumColor()));
    NetworkSection->Children.Add(NetModeInfo);

    const auto NetRole = UCk_Utils_Net_UE::Get_EntityNetRole(InEntity);
    auto NetRoleInfo = MakeShareable(new FCkEntityDebugInfo(TEXT("Net Role"), ck::Format_UE(TEXT("{}"), NetRole), Get_EnumColor()));
    NetworkSection->Children.Add(NetRoleInfo);
}

auto SCkEntityDebugTool::DoAddRelationshipInfo(const FCk_Handle& InEntity) -> void
{
    // Relationship Information
    auto RelationshipSection = FCkEntityDebugInfo::CreateSection(TEXT("Relationships"));
    _EntityInfoItems.Add(RelationshipSection);

    // Team Information
    if (const auto TeamEntity = UCk_Utils_Team_UE::Cast(InEntity); ck::IsValid(TeamEntity))
    {
        const auto TeamID = UCk_Utils_Team_UE::Get_ID(TeamEntity);
        auto TeamInfo = MakeShareable(new FCkEntityDebugInfo(TEXT("Team"), FString::Printf(TEXT("%d (Starts from ZERO)"), TeamID), Get_NumberColor()));
        RelationshipSection->Children.Add(TeamInfo);
    }
    else
    {
        auto NoTeamInfo = MakeShareable(new FCkEntityDebugInfo(TEXT("Team"), TEXT("Unknown"), Get_UnknownColor()));
        RelationshipSection->Children.Add(NoTeamInfo);
    }

    // Context Owner Information
    if (UCk_Utils_ContextOwner_UE::Has(InEntity))
    {
        const auto ContextOwner = UCk_Utils_ContextOwner_UE::Get_ContextOwner(InEntity);
        auto ContextOwnerInfo = MakeShareable(new FCkEntityDebugInfo(TEXT("Context Owner"), ContextOwner.ToString(), Get_EntityColor()));
        RelationshipSection->Children.Add(ContextOwnerInfo);
    }
    else
    {
        auto NoContextOwnerInfo = MakeShareable(new FCkEntityDebugInfo(TEXT("Context Owner"), TEXT("None"), Get_NoneColor()));
        RelationshipSection->Children.Add(NoContextOwnerInfo);
    }

    // Lifetime Owner Information
    if (InEntity.Has<ck::FFragment_LifetimeOwner>())
    {
        const auto LifetimeOwner = InEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
        auto LifetimeOwnerInfo = MakeShareable(new FCkEntityDebugInfo(TEXT("Lifetime Owner"), LifetimeOwner.ToString(), Get_EntityColor()));
        RelationshipSection->Children.Add(LifetimeOwnerInfo);
    }
    else
    {
        auto NoLifetimeOwnerInfo = MakeShareable(new FCkEntityDebugInfo(TEXT("Lifetime Owner"), TEXT("None"), Get_NoneColor()));
        RelationshipSection->Children.Add(NoLifetimeOwnerInfo);
    }
}

// --------------------------------------------------------------------------------------------------------------------
