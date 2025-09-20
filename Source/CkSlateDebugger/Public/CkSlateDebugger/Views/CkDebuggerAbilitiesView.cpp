#include "CkDebuggerAbilitiesView.h"

#include "CkSlateDebugger/CkSlateDebuggerStyle.h"
#include "CkSlateDebugger/CkSlateDebuggerWindow.h"

#include "CkAbility/Ability/CkAbility_Utils.h"
#include "CkAbility/Ability/CkAbility_Script.h"
#include "CkAbility/AbilityOwner/CkAbilityOwner_Utils.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Debug/CkDebug_Utils.h"

#include "CkEcs/Handle/CkHandle_Utils.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SCkDebuggerAbilitiesView"

void SCkDebuggerAbilitiesView::Construct(const FArguments& InArgs)
{
    SCkDebuggerViewBase::Construct(
        SCkDebuggerViewBase::FArguments()
        .DebuggerWindow(InArgs._DebuggerWindow)
    );

    ChildSlot
    [
        SNew(SVerticalBox)

        // Filter controls
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0, 0, 0, 4)
        [
            SNew(SHorizontalBox)

            // Search
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SAssignNew(SearchBox, SSearchBox)
                .HintText(LOCTEXT("SearchAbilities", "Search abilities..."))
                .OnTextChanged(this, &SCkDebuggerAbilitiesView::OnSearchTextChanged)
            ]

            // Filters
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(8, 0, 4, 0)
            .VAlign(VAlign_Center)
            [
                SAssignNew(ShowActiveCheckBox, SCheckBox)
                .IsChecked(bShowActive ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                .OnCheckStateChanged_Lambda([this](ECheckBoxState State)
                {
                    bShowActive = State == ECheckBoxState::Checked;
                    ApplyFilters();
                })
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ShowActive", "Active"))
                    .ColorAndOpacity(FLinearColor(0.0f, 1.0f, 0.5f))
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(4, 0)
            .VAlign(VAlign_Center)
            [
                SAssignNew(ShowInactiveCheckBox, SCheckBox)
                .IsChecked(bShowInactive ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                .OnCheckStateChanged_Lambda([this](ECheckBoxState State)
                {
                    bShowInactive = State == ECheckBoxState::Checked;
                    ApplyFilters();
                })
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ShowInactive", "Inactive"))
                    .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
                ]
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(4, 0)
            .VAlign(VAlign_Center)
            [
                SAssignNew(ShowBlockedCheckBox, SCheckBox)
                .IsChecked(bShowBlocked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                .OnCheckStateChanged_Lambda([this](ECheckBoxState State)
                {
                    bShowBlocked = State == ECheckBoxState::Checked;
                    ApplyFilters();
                })
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ShowBlocked", "Blocked"))
                    .ColorAndOpacity(FLinearColor(1.0f, 0.5f, 0.5f))
                ]
            ]
        ]

        // Ability list
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
            .Padding(4)
            [
                SAssignNew(AbilityListView, SListView<TSharedPtr<FCkAbilityInfo>>)
                .ListItemsSource(&FilteredAbilities)
                .OnGenerateRow(this, &SCkDebuggerAbilitiesView::GenerateAbilityRow)
                .OnSelectionChanged(this, &SCkDebuggerAbilitiesView::OnAbilitySelectionChanged)
                .SelectionMode(ESelectionMode::Single)
                .HeaderRow(
                    SNew(SHeaderRow)

                    + SHeaderRow::Column("Active")
                    .DefaultLabel(LOCTEXT("ActiveColumn", ""))
                    .FixedWidth(30)

                    + SHeaderRow::Column("Status")
                    .DefaultLabel(LOCTEXT("StatusColumn", "Status"))
                    .FixedWidth(80)

                    + SHeaderRow::Column("Name")
                    .DefaultLabel(LOCTEXT("NameColumn", "Ability Name"))
                    .FillWidth(0.5f)

                    + SHeaderRow::Column("Owner")
                    .DefaultLabel(LOCTEXT("OwnerColumn", "Owner"))
                    .FillWidth(0.3f)

                    + SHeaderRow::Column("Blocked")
                    .DefaultLabel(LOCTEXT("BlockedColumn", "Blocked"))
                    .FillWidth(0.2f)
                )
            ]
        ]
    ];
}

auto SCkDebuggerAbilitiesView::GetViewDisplayName() const -> FText
{
    return LOCTEXT("Abilities", "Abilities");
}

auto SCkDebuggerAbilitiesView::UpdateView() -> void
{
    // Periodic updates if needed
}

auto SCkDebuggerAbilitiesView::RefreshView() -> void
{
    BuildAbilityList();
}

auto SCkDebuggerAbilitiesView::OnSelectionChanged(const TArray<FCk_Handle>& NewSelection) -> void
{
    SCkDebuggerViewBase::OnSelectionChanged(NewSelection);
    BuildAbilityList();
}

auto SCkDebuggerAbilitiesView::BuildAbilityList() -> void
{
    AllAbilities.Empty();

    for (const auto& Entity : GetSelectedEntities())
    {
        if (NOT ck::IsValid(Entity))
        { continue; }

        auto AbilityOwner = UCk_Utils_AbilityOwner_UE::Cast(Entity);
        if (NOT ck::IsValid(AbilityOwner))
        { continue; }

        UCk_Utils_AbilityOwner_UE::ForEach_Ability(AbilityOwner,
            [this, AbilityOwner](const FCk_Handle_Ability& Ability)
            {
                auto Info = MakeShareable(new FCkAbilityInfo);
                Info.Object->Ability = Ability;
                Info.Object->Owner = AbilityOwner;
                Info.Object->Name = UCk_Utils_Ability_UE::Get_DisplayName(Ability);
                Info.Object->DisplayName = Info.Object->Name.ToString();
                Info.Object->Status = UCk_Utils_Ability_UE::Get_Status(Ability);
                Info.Object->CanActivate = UCk_Utils_Ability_UE::Get_CanActivate(Ability);

                auto ScriptClass = UCk_Utils_Ability_UE::Get_ScriptClass(Ability);
                Info.Object->bIsCondition = ScriptClass &&
                    ScriptClass->ImplementsInterface(UCk_Ability_Condition_Interface::StaticClass());

                Info.Object->Level = 0; // TODO: Calculate hierarchy level

                AllAbilities.Add(Info);
            });
    }

    ApplyFilters();
}

auto SCkDebuggerAbilitiesView::GenerateAbilityRow(TSharedPtr<FCkAbilityInfo> Item, const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>
{
    if (NOT Item.IsValid())
    { return SNew(STableRow<TSharedPtr<FCkAbilityInfo>>, OwnerTable); }

    return SNew(STableRow<TSharedPtr<FCkAbilityInfo>>, OwnerTable)
    [
        SNew(SHorizontalBox)

        // Activation checkbox
        + SHorizontalBox::Slot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .AutoWidth()
        [
            SNew(SCheckBox)
            .IsChecked(Item->Status == ECk_Ability_Status::Active ?
                ECheckBoxState::Checked : ECheckBoxState::Unchecked)
            .OnCheckStateChanged_Lambda([this, Item](ECheckBoxState State)
            {
                OnActivationToggled(Item);
            })
        ]

        // Status
        + SHorizontalBox::Slot()
        .Padding(4, 2)
        .AutoWidth()
        [
            SNew(STextBlock)
            .Text(GetStatusText(Item->Status))
            .ColorAndOpacity(GetStatusColor(Item->Status))
        ]

        // Name
        + SHorizontalBox::Slot()
        .Padding(4, 2)
        .FillWidth(0.5f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(Item->DisplayName))
            .ColorAndOpacity(GetAbilityColor(*Item))
        ]

        // Owner
        + SHorizontalBox::Slot()
        .Padding(4, 2)
        .FillWidth(0.3f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(UCk_Utils_Handle_UE::Get_DebugName(Item->Owner).ToString()))
            .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
        ]

        // Blocked reason
        + SHorizontalBox::Slot()
        .Padding(4, 2)
        .FillWidth(0.2f)
        [
            SNew(STextBlock)
            .Text_Lambda([Item]()
            {
                if (Item->CanActivate != ECk_Ability_ActivationRequirementsResult::RequirementsMet &&
                    Item->CanActivate != ECk_Ability_ActivationRequirementsResult::RequirementsMet_ButAlreadyActive)
                {
                    return LOCTEXT("Blocked", "Blocked");
                }
                return FText::GetEmpty();
            })
            .ColorAndOpacity(FLinearColor(1.0f, 0.5f, 0.5f))
        ]
    ];
}

auto SCkDebuggerAbilitiesView::OnAbilitySelectionChanged(TSharedPtr<FCkAbilityInfo> Item, ESelectInfo::Type SelectInfo) -> void
{
    // Handle ability selection
}

auto SCkDebuggerAbilitiesView::GetAbilityColor(const FCkAbilityInfo& Info) const -> FLinearColor
{
    if (Info.Status == ECk_Ability_Status::Active)
    {
        return Info.bIsCondition ?
            FLinearColor(1.0f, 0.95f, 0.0f) : // Yellow for conditions
            FLinearColor(0.0f, 1.0f, 0.5f);   // Green for active
    }

    if (Info.CanActivate == ECk_Ability_ActivationRequirementsResult::RequirementsMet)
    {
        return FLinearColor(0.8f, 0.8f, 0.8f); // Gray for inactive
    }

    return FLinearColor(1.0f, 0.5f, 0.5f); // Red for blocked
}

auto SCkDebuggerAbilitiesView::GetStatusText(ECk_Ability_Status Status) const -> FText
{
    switch (Status)
    {
        case ECk_Ability_Status::Active:
            return LOCTEXT("Active", "Active");
        case ECk_Ability_Status::NotActive:
            return LOCTEXT("Inactive", "Inactive");
        default:
            return FText::GetEmpty();
    }
}

auto SCkDebuggerAbilitiesView::GetStatusColor(ECk_Ability_Status Status) const -> FLinearColor
{
    return Status == ECk_Ability_Status::Active ?
        FLinearColor(0.0f, 1.0f, 0.5f) :
        FLinearColor(0.5f, 0.5f, 0.5f);
}

auto SCkDebuggerAbilitiesView::OnActivationToggled(TSharedPtr<FCkAbilityInfo> Info) -> void
{
    if (NOT Info.IsValid())
    { return; }

    if (Info->Status == ECk_Ability_Status::Active)
    {
        UCk_Utils_AbilityOwner_UE::Request_DeactivateAbility(
            Info->Owner,
            FCk_Request_AbilityOwner_DeactivateAbility{Info->Ability},
            {}
        );
    }
    else
    {
        UCk_Utils_AbilityOwner_UE::Request_TryActivateAbility(
            Info->Owner,
            FCk_Request_AbilityOwner_ActivateAbility{Info->Ability},
            {}
        );
    }

    RefreshView();
}

auto SCkDebuggerAbilitiesView::OnSearchTextChanged(const FText& NewText) -> void
{
    SearchFilter = NewText.ToString();
    ApplyFilters();
}

auto SCkDebuggerAbilitiesView::ApplyFilters() -> void
{
    FilteredAbilities.Empty();

    for (const auto& Ability : AllAbilities)
    {
        if (PassesFilter(*Ability))
        {
            FilteredAbilities.Add(Ability);
        }
    }

    if (AbilityListView.IsValid())
    {
        AbilityListView->RequestListRefresh();
    }
}

auto SCkDebuggerAbilitiesView::PassesFilter(const FCkAbilityInfo& Info) const -> bool
{
    // Status filter
    if (Info.Status == ECk_Ability_Status::Active && NOT bShowActive)
    { return false; }

    if (Info.Status == ECk_Ability_Status::NotActive)
    {
        bool bIsBlocked = Info.CanActivate != ECk_Ability_ActivationRequirementsResult::RequirementsMet;

        if (bIsBlocked && NOT bShowBlocked)
        { return false; }

        if (NOT bIsBlocked && NOT bShowInactive)
        { return false; }
    }

    // Search filter
    if (NOT SearchFilter.IsEmpty())
    {
        if (NOT Info.DisplayName.Contains(SearchFilter, ESearchCase::IgnoreCase))
        { return false; }
    }

    return true;
}

#undef LOCTEXT_NAMESPACE