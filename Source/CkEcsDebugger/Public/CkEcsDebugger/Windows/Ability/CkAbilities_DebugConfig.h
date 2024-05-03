#pragma once

#include "CkAbility/Ability/CkAbility_Fragment_Data.h"

#include "CkCore/Macros/CkMacros.h"

#include "CogWindowConfig.h"

#include "CkAbilities_DebugConfig.generated.h"

//--------------------------------------------------------------------------------------------------------------------------

UCLASS(Config = CkEcsDebugger)
class UCk_Abilities_DebugWindowConfig : public UCogWindowConfig
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Abilities_DebugWindowConfig);

public:
    auto
    Reset() -> void override;

    auto
    Get_AbilityColor(
        const FCk_Handle_Ability& InAbility) const -> const FVector4f&;

public:
    UPROPERTY(Config)
    bool SortByName = false;

    UPROPERTY(Config)
    bool BlueprintNameOnly = false;

    UPROPERTY(Config)
    bool ShowActive = true;

    UPROPERTY(Config)
    bool ShowInactive = true;

    UPROPERTY(Config)
    bool ShowBlocked = true;

    UPROPERTY(Config)
    bool ShowSubAbilities = true;

    UPROPERTY(Config)
    FVector4f ActiveColor = FVector4f(0.0f, 1.0f, 0.5f, 1.0f);

    UPROPERTY(Config)
    FVector4f InactiveColor = FVector4f(1.0f, 1.0f, 1.0f, 1.0f);

    UPROPERTY(Config)
    FVector4f BlockedColor = FVector4f(1.0f, 0.5f, 0.5f, 1.0f);

    // TODO: GroupBySource (EntityExtensions)
};

//--------------------------------------------------------------------------------------------------------------------------
