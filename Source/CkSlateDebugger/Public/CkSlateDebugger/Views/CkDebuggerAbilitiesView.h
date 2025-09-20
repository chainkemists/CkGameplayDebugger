#pragma once

#include "CkAbility/Ability/CkAbility_Fragment_Data.h"
#include "CkAbility/AbilityOwner/CkAbilityOwner_Fragment_Data.h"

#include "CkSlateDebugger/Views/CkDebuggerViewBase.h"

#include "Widgets/Views/SListView.h"

struct FCkAbilityInfo
{
    FCk_Handle_Ability Ability;
    FCk_Handle_AbilityOwner Owner;
    FName Name;
    FString DisplayName;
    ECk_Ability_Status Status;
    ECk_Ability_ActivationRequirementsResult CanActivate;
    bool bIsCondition;
    int32 Level;
};

class SCkDebuggerAbilitiesView : public SCkDebuggerViewBase
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerAbilitiesView) {}
        SLATE_ARGUMENT(TWeakPtr<SCkSlateDebuggerWindow>, DebuggerWindow)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual auto GetViewName() const -> FName override { return TEXT("Abilities"); }
    virtual auto GetViewDisplayName() const -> FText override;

    virtual auto UpdateView() -> void override;
    virtual auto RefreshView() -> void override;
    virtual auto OnSelectionChanged(const TArray<FCk_Handle>& NewSelection) -> void override;

private:
    auto BuildAbilityList() -> void;
    auto GenerateAbilityRow(TSharedPtr<FCkAbilityInfo> Item, const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>;
    auto OnAbilitySelectionChanged(TSharedPtr<FCkAbilityInfo> Item, ESelectInfo::Type SelectInfo) -> void;

    auto GetAbilityColor(const FCkAbilityInfo& Info) const -> FLinearColor;
    auto GetStatusText(ECk_Ability_Status Status) const -> FText;
    auto GetStatusColor(ECk_Ability_Status Status) const -> FLinearColor;

    auto OnActivationToggled(TSharedPtr<FCkAbilityInfo> Info) -> void;
    auto OnSearchTextChanged(const FText& NewText) -> void;

    auto ApplyFilters() -> void;
    auto PassesFilter(const FCkAbilityInfo& Info) const -> bool;

private:
    TSharedPtr<SListView<TSharedPtr<FCkAbilityInfo>>> AbilityListView;
    TSharedPtr<SSearchBox> SearchBox;
    TSharedPtr<SCheckBox> ShowActiveCheckBox;
    TSharedPtr<SCheckBox> ShowInactiveCheckBox;
    TSharedPtr<SCheckBox> ShowBlockedCheckBox;

    TArray<TSharedPtr<FCkAbilityInfo>> AllAbilities;
    TArray<TSharedPtr<FCkAbilityInfo>> FilteredAbilities;

    FString SearchFilter;
    bool bShowActive = true;
    bool bShowInactive = true;
    bool bShowBlocked = true;
};