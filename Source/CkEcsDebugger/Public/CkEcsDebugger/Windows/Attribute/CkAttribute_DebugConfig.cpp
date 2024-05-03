#include "CkAttribute_DebugConfig.h"

#include "CkAttribute/FloatAttribute/CkFloatAttribute_Utils.h"

//--------------------------------------------------------------------------------------------------------------------------

auto
    UCk_Attribute_DebugWindowConfig::
    Reset()
    -> void
{
    Super::Reset();

    SortByName = true;
    ShowOnlyModified = false;

    PositiveColor = FVector4f{0.0f, 1.0f, 0.5f, 1.0f};
    NegativeColor = FVector4f{1.0f, 0.5f, 0.5f, 1.0f};
    NeutralColor  = FVector4f{1.0f, 1.0f, 1.0f, 1.0f};
}

template <typename T_UtilsType, typename T_HandleType>
auto
    UCk_Attribute_DebugWindowConfig::
    Get_AttributeColor(
        const T_HandleType& InAttribute,
        ECk_MinMaxCurrent InAttributeComponent) const
    -> const FVector4f&
{
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
