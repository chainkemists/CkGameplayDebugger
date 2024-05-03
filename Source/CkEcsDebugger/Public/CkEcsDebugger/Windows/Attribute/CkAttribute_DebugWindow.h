#pragma once

#include "CkByteAttribute_Fragment_Data.h"
#include "CkByteAttribute_Utils.h"
#include "CkFloatAttribute_Utils.h"
#include "CkVectorAttribute_Utils.h"

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"
#include "CkEcsDebugger/Windows/Attribute/CkAttribute_DebugConfig.h"

// --------------------------------------------------------------------------------------------------------------------

template <typename T_HandleType, typename T_UtilsType, typename T_Type>
class FCk_Attribute_DebugWindow : public FCk_Ecs_DebugWindow
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
    RenderMenu() -> void;

    auto
    RenderTable(
        FCk_Handle& InSelectionEntity) -> void;

    auto
    DrawAttributeInfo(
        const T_HandleType& InAttribute) const -> void;

private:
    ImGuiTextFilter _Filter;
    TObjectPtr<UCk_Attribute_DebugWindowConfig> _Config;
};

// --------------------------------------------------------------------------------------------------------------------

class FCk_ByteAttribute_DebugWindow : public FCk_Attribute_DebugWindow<FCk_Handle_ByteAttribute, UCk_Utils_ByteAttribute_UE, uint8> { };
class FCk_FloatAttribute_DebugWindow : public FCk_Attribute_DebugWindow<FCk_Handle_FloatAttribute, UCk_Utils_FloatAttribute_UE, float> { };
class FCk_VectorAttribute_DebugWindow : public FCk_Attribute_DebugWindow<FCk_Handle_VectorAttribute, UCk_Utils_VectorAttribute_UE, FVector> { };

// --------------------------------------------------------------------------------------------------------------------
