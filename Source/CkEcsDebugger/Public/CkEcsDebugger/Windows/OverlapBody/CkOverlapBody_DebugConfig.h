#pragma once

#include "CogWindowConfig.h"

#include "CkCore/Enums/CkEnums.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkOverlapBody_DebugConfig.generated.h"

//--------------------------------------------------------------------------------------------------------------------------

UCLASS(Config = CkEcsDebugger)
class UCk_OverlapBody_DebugWindowConfig : public UCogWindowConfig
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_OverlapBody_DebugWindowConfig);

public:
    auto
    Reset() -> void override;

    template <typename T_UtilsType, typename T_HandleType>
    auto
    Get_Color(
        const T_HandleType& InOverlapEntity) const -> const FVector4f&;

public:
    UPROPERTY(Config)
    bool SortByName = false;

    UPROPERTY(Config)
    bool ShowActive = true;

    UPROPERTY(Config)
    bool ShowInactive = true;

    UPROPERTY(Config)
    FVector4f ActiveColor = FVector4f(0.0f, 1.0f, 0.5f, 1.0f);

    UPROPERTY(Config)
    FVector4f InactiveColor = FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
};

// --------------------------------------------------------------------------------------------------------------------

template <typename T_UtilsType, typename T_HandleType>
auto
    UCk_OverlapBody_DebugWindowConfig::
    Get_Color(
        const T_HandleType& InOverlapEntity) const
    -> const FVector4f&
{
    switch(T_UtilsType::Get_EnableDisable(InOverlapEntity))
    {
        case ECk_EnableDisable::Enable: return ActiveColor;
        case ECk_EnableDisable::Disable: return InactiveColor;
    }

    return ActiveColor;
}

// --------------------------------------------------------------------------------------------------------------------
