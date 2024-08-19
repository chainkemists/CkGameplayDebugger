#pragma once

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"
#include "CkEcsDebugger/Windows/EntityCollection/CkEntityCollection_DebugConfig.h"

// --------------------------------------------------------------------------------------------------------------------

class FCk_EntityCollection_DebugWindow : public FCk_Ecs_DebugWindow
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

private:
    ImGuiTextFilter _Filter;
    TObjectPtr<UCk_EntityCollection_DebugWindowConfig> _Config;
};

// --------------------------------------------------------------------------------------------------------------------

