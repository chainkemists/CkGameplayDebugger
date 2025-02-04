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

//--------------------------------------------------------------------------------------------------------------------------
