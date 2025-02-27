#include "CkAbilities_DebugConfig.h"

#include "CkAbility/Ability/CkAbility_Script.h"
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

    ActiveColor = _ActiveColor;;
    ActiveConditionColor = _ActiveConditionColor;
    InactiveColor = _InactiveColor;
    BlockedColor = _BlockedColor;
}

auto
    UCk_Abilities_DebugWindowConfig::
    Get_AbilityColor(
        const FCk_Handle_Ability& InAbility) const
    -> FVector4f
{
    if (UCk_Utils_Ability_UE::Get_Status(InAbility) == ECk_Ability_Status::Active)
    {
        if (UCk_Utils_Ability_UE::Get_ScriptClass(InAbility)->ImplementsInterface(UCk_Ability_Condition_Interface::StaticClass()))
        {
            return ActiveConditionColor;
        }

        return ActiveColor;
    }

    if (const auto& CanActivateResult = UCk_Utils_Ability_UE::Get_CanActivate(InAbility);
        CanActivateResult == ECk_Ability_ActivationRequirementsResult::RequirementsMet)
    {
        if (UCk_Utils_Ability_UE::Get_ScriptClass(InAbility)->ImplementsInterface(UCk_Ability_Condition_Interface::StaticClass()))
        {
            return ActiveConditionColor * FVector4f{ 1.0f, 1.0f, 1.0f, 0.6f };
        }
        return InactiveColor;
    }

    return BlockedColor;
}

//--------------------------------------------------------------------------------------------------------------------------
