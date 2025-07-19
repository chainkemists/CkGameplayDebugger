#pragma once

#include "CkAbility/AbilityOwner/CkAbilityOwner_Fragment_Data.h"

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"
#include "CkEcsDebugger/Windows/Ability/CkAbilities_DebugConfig.h"

// --------------------------------------------------------------------------------------------------------------------

struct FCk_Handle_AbilityOwner;
struct FCk_Handle_Ability;

class FCk_Abilities_DebugWindow : public FCk_Ecs_DebugWindow
{
    using Super = FCk_Ecs_DebugWindow;

public:
    auto
    Initialize() -> void override;

protected:
    auto
    ResetConfig() -> void override;

    auto
    RenderHelp() -> void override;

    auto
    RenderContent() -> void override;

    auto
    RenderTick(
        float InDeltaT) -> void override;

    auto
    GameTick(
        float InDeltaT) -> void override;

    auto
    RenderMenu() -> void;

    auto
    RenderOpenedAbilities() -> void;

    auto
    RenderTable(
        const TArray<FCk_Handle_AbilityOwner>& InAbilityOwners) -> void;

    auto
    RenderAbilityInfo(
        const FCk_Handle_Ability& InAbility) -> void;

    auto
    RenderAbilityContextMenu(
        const FCk_Handle_Ability& InAbility,
        int32 InIndex) -> void;

    auto
    OpenAbility(
        const FCk_Handle_Ability& InAbility) -> void;

    auto
    CloseAbility(
        const FCk_Handle_Ability& InAbility) -> void;

    auto
    ProcessAbilityActivation(
        FCk_Handle_Ability& InAbility) -> void;

private:
    auto
    DoGet_AbilityName(
        const FCk_Handle_Ability& InAbility) const
        -> FName;

    static auto
    DoGet_AbilityTimer(
        const FCk_Handle_Ability& InAbility)
        -> FString;

private:
    auto
    AddToFilteredAbilities(
        FCk_Handle_AbilityOwner InAbilityOwner) -> void;

private:
    TObjectPtr<UCk_Abilities_DebugWindowConfig> _Config;

    FCk_Handle_Ability _AbilityHandleToActivate;
    TArray<FCk_Handle_Ability> _OpenedAbilities;

    TMap<FCk_Handle_AbilityOwner, TArray<FCk_Handle_Ability>> _FilteredAbilities;
};

// --------------------------------------------------------------------------------------------------------------------