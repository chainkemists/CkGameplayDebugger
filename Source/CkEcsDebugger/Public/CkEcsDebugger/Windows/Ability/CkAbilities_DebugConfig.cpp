#include "CkAbilities_DebugConfig.h"

#include "CkAbility/Ability/CkAbility_Utils.h"

//--------------------------------------------------------------------------------------------------------------------------

auto
    UCk_Abilities_DebugWindowConfig::
    Reset()
    -> void
{
    Super::Reset();

    SortByName = false;
    ShowActive = true;
    ShowInactive = true;
    ShowBlocked = true;

    ActiveColor = FVector4f{0.0f, 1.0f, 0.5f, 1.0f};
    InactiveColor = FVector4f{1.0f, 1.0f, 1.0f, 1.0f};
    BlockedColor = FVector4f{1.0f, 0.5f, 0.5f, 1.0f};
}

auto
    UCk_Abilities_DebugWindowConfig::
    Get_AbilityColor(
        const FCk_Handle_Ability& InAbility) const
    -> const FVector4f&
{
    if (UCk_Utils_Ability_UE::Get_Status(InAbility) == ECk_Ability_Status::Active)
    { return ActiveColor; }

    if (const auto& CanActivateResult = UCk_Utils_Ability_UE::Get_CanActivate(InAbility);
        CanActivateResult == ECk_Ability_ActivationRequirementsResult::RequirementsMet)
    { return InactiveColor; }

    return BlockedColor;
}

//--------------------------------------------------------------------------------------------------------------------------
