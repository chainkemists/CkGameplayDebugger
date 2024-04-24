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

auto
    UCk_Attribute_DebugWindowConfig::
    Get_AttributeColor(
        const FCk_Handle_FloatAttribute& InAttribute,
        ECk_MinMaxCurrent InAttributeComponent) const
    -> const FVector4f&
{
    const auto& BaseValue = UCk_Utils_FloatAttribute_UE::Get_BaseValue(InAttribute, InAttributeComponent);
    const auto& CurrentValue = UCk_Utils_FloatAttribute_UE::Get_FinalValue(InAttribute, InAttributeComponent);

    if (CurrentValue > BaseValue)
    {
        return PositiveColor;
    }

    if (CurrentValue < BaseValue)
    {
        return NegativeColor;
    }

    return NeutralColor;
}

//--------------------------------------------------------------------------------------------------------------------------
