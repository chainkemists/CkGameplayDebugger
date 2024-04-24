#pragma once

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"
#include "CkEcsDebugger/Windows/Ability/CkAbilityOwnerTags_DebugConfig.h"

// --------------------------------------------------------------------------------------------------------------------

struct FCk_Handle_AbilityOwner;

class FCk_AbilityOwnerTags_DebugWindow : public FCk_Ecs_DebugWindow
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
        FCk_Handle_AbilityOwner& InSelectionEntity) -> void;

private:
    ImGuiTextFilter _Filter;
    TObjectPtr<UCk_AbilityOwnerTags_DebugWindowConfig> _Config;
};

// --------------------------------------------------------------------------------------------------------------------

