#pragma once

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"
#include "CkEcsDebugger/Windows/Attribute/CkAttribute_DebugConfig.h"

// --------------------------------------------------------------------------------------------------------------------
// TODO: Templatize the base window so we can use it for all types of attributes

class FCk_FloatAttribute_DebugWindow : public FCk_Ecs_DebugWindow
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
        const FCk_Handle_FloatAttribute& InAttribute) const -> void;

private:
    ImGuiTextFilter _Filter;
    TObjectPtr<UCk_Attribute_DebugWindowConfig> _Config;
};

// --------------------------------------------------------------------------------------------------------------------

class FCk_ByteAttribute_DebugWindow : public FCk_Ecs_DebugWindow
{
    using Super = FCk_Ecs_DebugWindow;

public:
    auto
    Initialize() -> void override;

protected:

    auto
    RenderHelp() -> void override;

    auto
    RenderContent() -> void override;
};

// --------------------------------------------------------------------------------------------------------------------

class FCk_VectorAttribute_DebugWindow : public FCk_Ecs_DebugWindow
{
    using Super = FCk_Ecs_DebugWindow;

public:
    auto
    Initialize() -> void override;

protected:

    auto
    RenderHelp() -> void override;

    auto
    RenderContent() -> void override;
};

// --------------------------------------------------------------------------------------------------------------------

