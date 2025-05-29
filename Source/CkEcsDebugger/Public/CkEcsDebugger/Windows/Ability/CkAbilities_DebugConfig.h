#pragma once

#include "CkAbility/Ability/CkAbility_Fragment_Data.h"

#include "CkCore/Macros/CkMacros.h"

#include "CogCommonConfig.h"

#include "CkAbilities_DebugConfig.generated.h"

//--------------------------------------------------------------------------------------------------------------------------

UCLASS(Config = CkEcsDebugger)
class UCk_Abilities_DebugWindowConfig : public UCogCommonConfig
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Abilities_DebugWindowConfig);

public:
    auto
    Reset() -> void override;

    auto
    Get_AbilityColor(
        const FCk_Handle_Ability& InAbility) const -> FVector4f;

private:
    inline static FVector4f _ActiveColor{0.0f, 1.0f, 0.5f, 1.0f};
    inline static FVector4f _ActiveConditionColor{1.0f, 0.95f, 0.0f, 1.0f};
    inline static FVector4f _InactiveColor{0.8f, 0.8f, 0.8f, 1.0f};
    inline static FVector4f _BlockedColor{1.0f, 0.5f, 0.5f, 1.0f};

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
    FVector4f ActiveColor = _ActiveColor;

    UPROPERTY(Config)
    FVector4f ActiveConditionColor = _ActiveConditionColor;

    UPROPERTY(Config)
    FVector4f InactiveColor = _InactiveColor;

    UPROPERTY(Config)
    FVector4f BlockedColor = _BlockedColor;

    // TODO: GroupBySource (EntityExtensions)
};

//--------------------------------------------------------------------------------------------------------------------------
