// --------------------------------------------------------------------------------------------------------------------

// CkSimpleEntitySelector_Widget.cpp

#include "CkSimpleEntitySelector_Widget.h"

#include "CkEcsDebugger/Subsystem/CkEcsDebugger_Subsystem.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include <Widgets/Layout/SBorder.h>
#include <Widgets/Text/STextBlock.h>
#include <Widgets/Input/SButton.h>

// --------------------------------------------------------------------------------------------------------------------

auto SCkSimpleEntitySelector::Construct(const FArguments& InArgs) -> void
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
                .Text(FText::FromString(TEXT("Select Entity")))
                .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Bold"))
                .ColorAndOpacity(FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.Primary"))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                DoCreateEntityComboBox()
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 4, 0, 0)
            [
                SNew(SButton)
                .ButtonStyle(&FCkDebugToolsStyle::Get().GetWidgetStyle<FButtonStyle>("CkDebugTools.Button.Secondary"))
                .Text(FText::FromString(TEXT("Refresh List")))
                .OnClicked_Lambda([this]() -> FReply
                {
                    Request_RefreshEntityList();
                    return FReply::Handled();
                })
            ]
        ]
    ];

    // Initial refresh
    Request_RefreshEntityList();
}

auto SCkSimpleEntitySelector::DoCreateEntityComboBox() -> TSharedRef<SWidget>
{
    return SAssignNew(_EntityComboBox, SComboBox<TSharedPtr<FCk_Handle>>)
        .OptionsSource(&_AvailableEntities)
        .OnGenerateWidget(this, &SCkSimpleEntitySelector::DoGenerateComboWidget)
        .OnSelectionChanged(this, &SCkSimpleEntitySelector::DoOnEntitySelected)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() -> FText
            {
                const auto SelectedEntity = _EntityComboBox->GetSelectedItem();
                if (NOT SelectedEntity.IsValid())
                {
                    return FText::FromString(TEXT("No Entity Selected"));
                }

                const auto Entity = *SelectedEntity;
                const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(Entity);
                return FText::FromString(FString::Printf(TEXT("%s [%s]"),
                    *DebugName.ToString(),
                    *Entity.Get_Entity().ToString()));
            })
            .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Regular"))
        ];
}

auto SCkSimpleEntitySelector::DoGenerateComboWidget(TSharedPtr<FCk_Handle> InEntity) -> TSharedRef<SWidget>
{
    if (NOT InEntity.IsValid())
    {
        return SNew(STextBlock).Text(FText::FromString(TEXT("Invalid Entity")));
    }

    const auto Entity = *InEntity;
    const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(Entity);
    const auto EntityString = Entity.Get_Entity().ToString();

    FString DisplayText = FString::Printf(TEXT("%s [%s]"), *DebugName.ToString(), *EntityString);

    // Add actor name if available
    if (UCk_Utils_OwningActor_UE::Has(Entity))
    {
        const auto EntityActor = UCk_Utils_OwningActor_UE::Get_EntityOwningActor(Entity);
        if (IsValid(EntityActor))
        {
            DisplayText += FString::Printf(TEXT(" - %s"), *EntityActor->GetName());
        }
    }

    return SNew(STextBlock)
        .Text(FText::FromString(DisplayText))
        .Font(FCkDebugToolsStyle::Get().GetFontStyle("CkDebugTools.Font.Regular"))
        .ColorAndOpacity(FCkDebugToolsStyle::Get().GetColor("CkDebugTools.Color.Entity"));
}

auto SCkSimpleEntitySelector::DoOnEntitySelected(TSharedPtr<FCk_Handle> InSelectedEntity, ESelectInfo::Type SelectInfo) -> void
{
    if (NOT InSelectedEntity.IsValid())
    {
        return;
    }

    const auto DebuggerSubsystem = Get_DebuggerSubsystem();
    if (NOT IsValid(DebuggerSubsystem))
    {
        return;
    }

    // Set the selected entity in the subsystem
    TArray<FCk_Handle> SelectedEntities = { *InSelectedEntity };
    const auto SelectedWorld = DebuggerSubsystem->Get_SelectedWorld();
    DebuggerSubsystem->Set_SelectionEntities(SelectedEntities, SelectedWorld);

    // Notify callback
    OnSelectionChanged.ExecuteIfBound();
}

auto SCkSimpleEntitySelector::Request_RefreshEntityList() -> void
{
    _AvailableEntities.Empty();

    const auto AllEntities = DoGetAllEntitiesFromWorld();

    for (const auto& Entity : AllEntities)
    {
        _AvailableEntities.Add(MakeShareable(new FCk_Handle(Entity)));
    }

    // Refresh the combo box
    if (_EntityComboBox.IsValid())
    {
        _EntityComboBox->RefreshOptions();
    }
}

auto SCkSimpleEntitySelector::DoGetAllEntitiesFromWorld() -> TArray<FCk_Handle>
{
    TArray<FCk_Handle> Entities;

    const auto CurrentWorld = Get_CurrentWorld();
    if (NOT IsValid(CurrentWorld))
    {
        return Entities;
    }

    const auto DebuggerSubsystem = Get_DebuggerSubsystem();
    if (NOT IsValid(DebuggerSubsystem))
    {
        return Entities;
    }

    const auto SelectedWorld = DebuggerSubsystem->Get_SelectedWorld();
    if (ck::Is_NOT_Valid(SelectedWorld))
    {
        return Entities;
    }

    auto TransientEntity = Get_TransientEntity();

    // Simple entity discovery - get all entities with lifetime owners
    TransientEntity.View<ck::FFragment_LifetimeOwner, CK_IGNORE_PENDING_KILL>().ForEach(
        [&](FCk_Entity InEntity, const ck::FFragment_LifetimeOwner& InFragment)
        {
            const auto Handle = ck::MakeHandle(InEntity, TransientEntity);

            if (Handle == TransientEntity)
                return;

            Entities.Add(Handle);
        });

    // Sort alphabetically by debug name
    ck::algo::Sort(Entities, [](const FCk_Handle& A, const FCk_Handle& B) -> bool
    {
        return UCk_Utils_Handle_UE::Get_DebugName(A).ToString() < UCk_Utils_Handle_UE::Get_DebugName(B).ToString();
    });

    return Entities;
}

auto SCkSimpleEntitySelector::Get_SelectedEntities() const -> TArray<FCk_Handle>
{
    const auto DebuggerSubsystem = Get_DebuggerSubsystem();
    if (NOT IsValid(DebuggerSubsystem))
    {
        return {};
    }

    return DebuggerSubsystem->Get_SelectionEntities();
}

auto SCkSimpleEntitySelector::Get_CurrentWorld() const -> UWorld*
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

auto SCkSimpleEntitySelector::Get_DebuggerSubsystem() const -> UCk_EcsDebugger_Subsystem_UE*
{
    const auto CurrentWorld = Get_CurrentWorld();
    if (NOT IsValid(CurrentWorld))
    {
        return nullptr;
    }

    return CurrentWorld->GetSubsystem<UCk_EcsDebugger_Subsystem_UE>();
}

auto SCkSimpleEntitySelector::Get_TransientEntity() const -> FCk_Handle
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
