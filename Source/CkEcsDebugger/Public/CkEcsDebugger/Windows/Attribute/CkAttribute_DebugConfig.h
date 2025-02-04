#pragma once

#include "CkAttribute/FloatAttribute/CkFloatAttribute_Fragment_Data.h"

#include "CkCore/Macros/CkMacros.h"

#include "CogWindowConfig.h"

#include "CkAttribute_DebugConfig.generated.h"

//--------------------------------------------------------------------------------------------------------------------------

UCLASS(Config = CkEcsDebugger)
class UCk_Attribute_DebugWindowConfig : public UCogWindowConfig
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Attribute_DebugWindowConfig);

public:
    auto
    Reset() -> void override;

    template <typename T_UtilsType, typename T_HandleType>
    auto
    Get_AttributeColor(
        const T_HandleType& InAttribute,
        ECk_MinMaxCurrent InAttributeComponent) const -> const FVector4f&;

public:
    UPROPERTY(Config)
    bool SortByName = true;

    UPROPERTY(Config)
    bool ShowOnlyModified = false;

    UPROPERTY(Config)
    FVector4f PositiveColor = FVector4f{0.0f, 1.0f, 0.5f, 1.0f};

    UPROPERTY(Config)
    FVector4f NegativeColor = FVector4f{1.0f, 0.5f, 0.5f, 1.0f};

    UPROPERTY(Config)
    FVector4f NeutralColor = FVector4f{1.0f, 1.0f, 1.0f, 1.0f};

    // TODO: GroupBySource (EntityExtensions)
};

// --------------------------------------------------------------------------------------------------------------------

template <typename T_UtilsType, typename T_HandleType>
auto
    UCk_Attribute_DebugWindowConfig::Get_AttributeColor(
        const T_HandleType& InAttribute,
        ECk_MinMaxCurrent InAttributeComponent) const
        -> const FVector4f& {
    const auto& BaseValue = T_UtilsType::Get_BaseValue(InAttribute, InAttributeComponent);
    const auto& CurrentValue = T_UtilsType::Get_FinalValue(InAttribute, InAttributeComponent);

    if constexpr(std::is_same_v< std::_Remove_cvref_t<decltype(BaseValue)>, FVector>)
    {
        if (CurrentValue.SquaredLength() > BaseValue.SquaredLength())
        {
            return PositiveColor;
        }

        if (CurrentValue.SquaredLength() < BaseValue.SquaredLength())
        {
            return NegativeColor;
        }
    }
    else
    {
        if (CurrentValue > BaseValue)
        {
            return PositiveColor;
        }

        if (CurrentValue < BaseValue)
        {
            return NegativeColor;
        }
    }

    return NeutralColor;
}

//--------------------------------------------------------------------------------------------------------------------------
